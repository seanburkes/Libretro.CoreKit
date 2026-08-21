#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"

struct core_api
{
   void (*retro_set_environment)(retro_environment_t);
   void (*retro_set_video_refresh)(retro_video_refresh_t);
   void (*retro_set_audio_sample)(retro_audio_sample_t);
   void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
   void (*retro_set_input_poll)(retro_input_poll_t);
   void (*retro_set_input_state)(retro_input_state_t);
   void (*retro_init)(void);
   void (*retro_deinit)(void);
   unsigned (*retro_api_version)(void);
   void (*retro_get_system_info)(struct retro_system_info *);
   void (*retro_get_system_av_info)(struct retro_system_av_info *);
   void (*retro_set_controller_port_device)(unsigned, unsigned);
   void (*retro_reset)(void);
   void (*retro_run)(void);
   size_t (*retro_serialize_size)(void);
   bool (*retro_serialize)(void *, size_t);
   bool (*retro_unserialize)(const void *, size_t);
   void (*retro_cheat_reset)(void);
   void (*retro_cheat_set)(unsigned, bool, const char *);
   bool (*retro_load_game)(const struct retro_game_info *);
   bool (*retro_load_game_special)(unsigned, const struct retro_game_info *, size_t);
   void (*retro_unload_game)(void);
   unsigned (*retro_get_region)(void);
   void *(*retro_get_memory_data)(unsigned);
   size_t (*retro_get_memory_size)(unsigned);
};

struct observations
{
   unsigned pixel_format_calls;
   unsigned input_descriptor_calls;
   unsigned controller_info_calls;
   unsigned input_bitmask_calls;
   unsigned variable_update_calls;
   unsigned audio_video_enable_calls;
   unsigned fast_forwarding_calls;
   unsigned video_calls;
   unsigned audio_batch_calls;
   unsigned audio_sample_calls;
   unsigned input_poll_calls;
   unsigned input_state_calls;
   uint64_t last_video_hash;
   bool provide_right;
   bool left_sprite;
   bool right_sprite;
   bool callback_error;
};

static struct observations observations;

#define CHIP8_OPCODE(value) (uint8_t)((value) >> 8), (uint8_t)(value)

enum
{
   chip8_state_size = 6212,
   chip8_state_pc_offset = 6,
   chip8_state_random_offset = 10,
   chip8_state_delay_offset = 14,
   chip8_state_sound_offset = 15,
};

static const uint8_t test_content[] = {
   0x00, 0xE0,
   0x60, 0x00,
   0x61, 0x08,
   0x62, 0x06,
   0xE2, 0xA1,
   0x70, 0x04,
   0xA2, 0x12,
   0xD0, 0x15,
   0x12, 0x10,
   0xF0, 0x90, 0x90, 0x90, 0xF0, 0x00,
};

static const uint8_t arithmetic_content[] = {
   CHIP8_OPCODE(0x6004),
   CHIP8_OPCODE(0xB208),
   CHIP8_OPCODE(0x61EE),
   CHIP8_OPCODE(0x61EE),
   CHIP8_OPCODE(0x61EE),
   CHIP8_OPCODE(0x61EE),
   CHIP8_OPCODE(0x600A),
   CHIP8_OPCODE(0x6105),
   CHIP8_OPCODE(0x8010),
   CHIP8_OPCODE(0x6203),
   CHIP8_OPCODE(0x6F01),
   CHIP8_OPCODE(0x8021),
   CHIP8_OPCODE(0x8022),
   CHIP8_OPCODE(0x8023),
   CHIP8_OPCODE(0x8EF0),
   CHIP8_OPCODE(0x63FA),
   CHIP8_OPCODE(0x640A),
   CHIP8_OPCODE(0x8344),
   CHIP8_OPCODE(0x6509),
   CHIP8_OPCODE(0x6604),
   CHIP8_OPCODE(0x8565),
   CHIP8_OPCODE(0x6704),
   CHIP8_OPCODE(0x6809),
   CHIP8_OPCODE(0x8787),
   CHIP8_OPCODE(0x6905),
   CHIP8_OPCODE(0x8906),
   CHIP8_OPCODE(0x6A81),
   CHIP8_OPCODE(0x8A0E),
   CHIP8_OPCODE(0x6B01),
   CHIP8_OPCODE(0x6C01),
   CHIP8_OPCODE(0x5BC0),
   CHIP8_OPCODE(0x6DEE),
   CHIP8_OPCODE(0x6D11),
   CHIP8_OPCODE(0x6C02),
   CHIP8_OPCODE(0x9BC0),
   CHIP8_OPCODE(0x6DEE),
   CHIP8_OPCODE(0xA300),
   CHIP8_OPCODE(0xFF55),
   CHIP8_OPCODE(0xA3E0),
   CHIP8_OPCODE(0x6E10),
   CHIP8_OPCODE(0x605A),
   CHIP8_OPCODE(0xFE1E),
   CHIP8_OPCODE(0xF055),
   CHIP8_OPCODE(0x60E7),
   CHIP8_OPCODE(0xA330),
   CHIP8_OPCODE(0xF033),
   CHIP8_OPCODE(0x6001),
   CHIP8_OPCODE(0x6102),
   CHIP8_OPCODE(0x6203),
   CHIP8_OPCODE(0xA320),
   CHIP8_OPCODE(0xF255),
   CHIP8_OPCODE(0x6000),
   CHIP8_OPCODE(0x6100),
   CHIP8_OPCODE(0x6200),
   CHIP8_OPCODE(0xA320),
   CHIP8_OPCODE(0xF265),
   CHIP8_OPCODE(0xA323),
   CHIP8_OPCODE(0xF255),
   CHIP8_OPCODE(0x600A),
   CHIP8_OPCODE(0xF029),
   CHIP8_OPCODE(0xF465),
   CHIP8_OPCODE(0xA340),
   CHIP8_OPCODE(0xF455),
   CHIP8_OPCODE(0x0000),
};

static const uint8_t timer_random_content[] = {
   CHIP8_OPCODE(0x6005),
   CHIP8_OPCODE(0xF015),
   CHIP8_OPCODE(0x6003),
   CHIP8_OPCODE(0xF018),
   CHIP8_OPCODE(0xF10A),
   CHIP8_OPCODE(0xF207),
   CHIP8_OPCODE(0xC3FF),
   CHIP8_OPCODE(0xC4F0),
   CHIP8_OPCODE(0xA350),
   CHIP8_OPCODE(0xF455),
   CHIP8_OPCODE(0x1214),
};

static bool check(bool condition, const char *message)
{
   if (!condition)
      fprintf(stderr, "validation failed: %s\n", message);
   return condition;
}

static uint64_t hash_bytes(const void *data, size_t size)
{
   const uint8_t *bytes = data;
   uint64_t hash = UINT64_C(14695981039346656037);
   size_t index;

   for (index = 0; index < size; index++)
   {
      hash ^= bytes[index];
      hash *= UINT64_C(1099511628211);
   }
   return hash;
}

static bool RETRO_CALLCONV environment_callback(unsigned command, void *data)
{
   if (command == RETRO_ENVIRONMENT_GET_LOG_INTERFACE)
      return false;

   if (command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
   {
      if (data == NULL || *(const enum retro_pixel_format *)data != RETRO_PIXEL_FORMAT_XRGB8888)
         observations.callback_error = true;
      observations.pixel_format_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS)
   {
      const struct retro_input_descriptor *descriptors = data;
      if (descriptors == NULL || descriptors[0].description == NULL ||
          descriptors[0].id != RETRO_DEVICE_ID_JOYPAD_UP ||
          strcmp(descriptors[0].description, "CHIP-8 key 2") != 0 ||
          descriptors[1].id != RETRO_DEVICE_ID_JOYPAD_DOWN ||
          descriptors[2].id != RETRO_DEVICE_ID_JOYPAD_LEFT ||
          descriptors[3].id != RETRO_DEVICE_ID_JOYPAD_RIGHT ||
          descriptors[4].id != RETRO_DEVICE_ID_JOYPAD_A ||
          descriptors[5].id != RETRO_DEVICE_ID_JOYPAD_B ||
          descriptors[6].description != NULL)
         observations.callback_error = true;
      observations.input_descriptor_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_SET_CONTROLLER_INFO)
   {
      const struct retro_controller_info *controllers = data;
      if (controllers == NULL || controllers[0].types == NULL ||
          controllers[0].num_types != 1 || controllers[0].types[0].desc == NULL ||
          strcmp(controllers[0].types[0].desc, "RetroPad") != 0 ||
          controllers[0].types[0].id != RETRO_DEVICE_JOYPAD ||
          controllers[1].types != NULL)
         observations.callback_error = true;
      observations.controller_info_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_GET_INPUT_BITMASKS)
   {
      if (data != NULL)
         observations.callback_error = true;
      observations.input_bitmask_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
   {
      if (data == NULL)
         observations.callback_error = true;
      else
         *(bool *)data = false;
      observations.variable_update_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE)
   {
      if (data == NULL)
         observations.callback_error = true;
      else
         *(int *)data = RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_AUDIO;
      observations.audio_video_enable_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_GET_FASTFORWARDING)
   {
      if (data == NULL)
         observations.callback_error = true;
      else
         *(bool *)data = false;
      observations.fast_forwarding_calls++;
      return true;
   }

   return false;
}

static void RETRO_CALLCONV video_callback(
      const void *data, unsigned width, unsigned height, size_t pitch)
{
   const uint32_t *pixels = data;
   const uint32_t white = UINT32_C(0x00FFFFFF);
   const uint32_t black = UINT32_C(0x00000000);

   if (data == NULL || width != 64 || height != 32 || pitch != 64 * sizeof(*pixels))
   {
      observations.callback_error = true;
   }
   else
   {
      observations.last_video_hash = hash_bytes(data, pitch * height);
      observations.left_sprite =
         pixels[(8 * 64)] == white && pixels[(8 * 64) + 3] == white &&
         pixels[(8 * 64) + 4] == black;
      observations.right_sprite =
         pixels[(8 * 64)] == black && pixels[(8 * 64) + 4] == white &&
         pixels[(8 * 64) + 7] == white;
   }
   observations.video_calls++;
}

static void RETRO_CALLCONV audio_sample_callback(int16_t left, int16_t right)
{
   (void)left;
   (void)right;
   observations.audio_sample_calls++;
}

static size_t RETRO_CALLCONV audio_batch_callback(const int16_t *data, size_t frames)
{
   size_t index;

   if (data == NULL || frames != 800)
      observations.callback_error = true;
   for (index = 0; data != NULL && index < frames * 2; index++)
   {
      if (data[index] != 0)
         observations.callback_error = true;
   }
   observations.audio_batch_calls++;
   return frames;
}

static void RETRO_CALLCONV input_poll_callback(void)
{
   observations.input_poll_calls++;
}

static int16_t RETRO_CALLCONV input_state_callback(
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (port != 0 || device != RETRO_DEVICE_JOYPAD || index != 0)
      observations.callback_error = true;

   observations.input_state_calls++;
   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      return observations.provide_right
         ? (int16_t)(1U << RETRO_DEVICE_ID_JOYPAD_RIGHT)
         : 0;
   return observations.provide_right && id == RETRO_DEVICE_ID_JOYPAD_RIGHT ? 1 : 0;
}

static bool assign_symbol(
      void *handle, const char *name, void *destination, size_t destination_size)
{
   void *symbol = dlsym(handle, name);
   if (symbol == NULL || destination_size != sizeof(symbol))
   {
      fprintf(stderr, "missing or incompatible required export: %s\n", name);
      return false;
   }

   memcpy(destination, &symbol, sizeof(symbol));
   return true;
}

#define LOAD_SYMBOL(api, handle, name) \
   do \
   { \
      if (!assign_symbol((handle), #name, &(api)->name, sizeof((api)->name))) \
         return false; \
   } while (false)

static bool load_api(void *handle, struct core_api *api)
{
   memset(api, 0, sizeof(*api));
   LOAD_SYMBOL(api, handle, retro_set_environment);
   LOAD_SYMBOL(api, handle, retro_set_video_refresh);
   LOAD_SYMBOL(api, handle, retro_set_audio_sample);
   LOAD_SYMBOL(api, handle, retro_set_audio_sample_batch);
   LOAD_SYMBOL(api, handle, retro_set_input_poll);
   LOAD_SYMBOL(api, handle, retro_set_input_state);
   LOAD_SYMBOL(api, handle, retro_init);
   LOAD_SYMBOL(api, handle, retro_deinit);
   LOAD_SYMBOL(api, handle, retro_api_version);
   LOAD_SYMBOL(api, handle, retro_get_system_info);
   LOAD_SYMBOL(api, handle, retro_get_system_av_info);
   LOAD_SYMBOL(api, handle, retro_set_controller_port_device);
   LOAD_SYMBOL(api, handle, retro_reset);
   LOAD_SYMBOL(api, handle, retro_run);
   LOAD_SYMBOL(api, handle, retro_serialize_size);
   LOAD_SYMBOL(api, handle, retro_serialize);
   LOAD_SYMBOL(api, handle, retro_unserialize);
   LOAD_SYMBOL(api, handle, retro_cheat_reset);
   LOAD_SYMBOL(api, handle, retro_cheat_set);
   LOAD_SYMBOL(api, handle, retro_load_game);
   LOAD_SYMBOL(api, handle, retro_load_game_special);
   LOAD_SYMBOL(api, handle, retro_unload_game);
   LOAD_SYMBOL(api, handle, retro_get_region);
   LOAD_SYMBOL(api, handle, retro_get_memory_data);
   LOAD_SYMBOL(api, handle, retro_get_memory_size);
   return true;
}

static uint16_t read_u16_le(const uint8_t *data)
{
   return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *data)
{
   return (uint32_t)data[0] |
      ((uint32_t)data[1] << 8) |
      ((uint32_t)data[2] << 16) |
      ((uint32_t)data[3] << 24);
}

static bool load_test_content(
      const struct core_api *api,
      const uint8_t *content,
      size_t content_size,
      const char *path)
{
   struct retro_game_info game;

   memset(&game, 0, sizeof(game));
   game.path = path;
   game.data = content;
   game.size = content_size;
   return api->retro_load_game(&game);
}

static bool run_arithmetic_suite(const struct core_api *api)
{
   static const uint8_t expected_registers[] = {
      0x00, 0x05, 0x03, 0x04, 0x0A, 0x05, 0x04, 0x05,
      0x09, 0x02, 0x02, 0x01, 0x02, 0x11, 0x01, 0x01,
   };
   static const uint8_t expected_values[] = {0x01, 0x02, 0x03};
   static const uint8_t expected_bcd[] = {0x02, 0x03, 0x01};
   static const uint8_t expected_font[] = {0xF0, 0x90, 0xF0, 0x90, 0x90};
   uint8_t state[chip8_state_size];
   uint8_t *system_ram;
   unsigned frame;

   if (!check(load_test_content(
                 api,
                 arithmetic_content,
                 sizeof(arithmetic_content),
                 "/corekit/arithmetic.ch8"),
              "arithmetic content load"))
      return false;

   system_ram = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (!check(system_ram != NULL, "arithmetic system RAM"))
      return false;

   for (frame = 0; frame < 6; frame++)
      api->retro_run();

   if (!check(memcmp(&system_ram[0x300], expected_registers, sizeof(expected_registers)) == 0,
              "arithmetic and skip instruction results") ||
       !check(memcmp(&system_ram[0x320], expected_values, sizeof(expected_values)) == 0 &&
                 memcmp(&system_ram[0x323], expected_values, sizeof(expected_values)) == 0,
              "register store and load results") ||
       !check(memcmp(&system_ram[0x330], expected_bcd, sizeof(expected_bcd)) == 0,
              "binary-coded decimal result") ||
       !check(memcmp(&system_ram[0x340], expected_font, sizeof(expected_font)) == 0,
              "font lookup and register load result") ||
       !check(system_ram[0x3F0] == 0x5A, "index-register addition result") ||
       !check(api->retro_serialize(state, sizeof(state)), "serialize arithmetic state") ||
       !check(read_u16_le(&state[8]) == 0x340,
              "register transfers leave index register unchanged"))
      return false;

   api->retro_unload_game();
   return true;
}

static bool run_timer_random_suite(const struct core_api *api)
{
   static const uint8_t expected_output[] = {0x03, 0x06, 0x04, 0x22, 0xC0};
   uint8_t wait_state[chip8_state_size];
   uint8_t completed_state[chip8_state_size];
   uint8_t replay_state[chip8_state_size];
   uint8_t reset_state[chip8_state_size];
   uint8_t *system_ram;
   unsigned frame;

   observations.provide_right = false;
   if (!check(load_test_content(
                 api,
                 timer_random_content,
                 sizeof(timer_random_content),
                 "/corekit/timer-random.ch8"),
              "timer and random content load"))
      return false;

   system_ram = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (!check(system_ram != NULL, "timer and random system RAM"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(wait_state, sizeof(wait_state)),
              "serialize key-wait state") ||
       !check(read_u16_le(&wait_state[chip8_state_pc_offset]) == 0x208,
              "key wait retains program counter") ||
       !check(read_u32_le(&wait_state[chip8_state_random_offset]) == UINT32_C(0xC0DEF00D),
              "random state is unchanged while waiting") ||
       !check(wait_state[chip8_state_delay_offset] == 4 &&
                 wait_state[chip8_state_sound_offset] == 2,
              "timers tick at end of waiting frame"))
      return false;

   observations.provide_right = true;
   api->retro_run();
   if (!check(memcmp(&system_ram[0x350], expected_output, sizeof(expected_output)) == 0,
              "key, timer, and deterministic random results") ||
       !check(api->retro_serialize(completed_state, sizeof(completed_state)),
              "serialize timer and random state") ||
       !check(read_u16_le(&completed_state[chip8_state_pc_offset]) == 0x214,
              "timer program reaches terminal loop") ||
       !check(read_u32_le(&completed_state[chip8_state_random_offset]) == UINT32_C(0x394B8BCA),
              "deterministic random sequence state") ||
       !check(completed_state[chip8_state_delay_offset] == 3 &&
                 completed_state[chip8_state_sound_offset] == 1,
              "timers tick once per completed frame"))
      return false;

   if (!check(api->retro_unserialize(wait_state, sizeof(wait_state)),
              "restore key-wait state"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(replay_state, sizeof(replay_state)),
              "serialize replayed timer and random state") ||
       !check(memcmp(completed_state, replay_state, sizeof(completed_state)) == 0,
              "timer and random state replay deterministically"))
      return false;

   observations.provide_right = false;
   api->retro_reset();
   api->retro_run();
   if (!check(api->retro_serialize(reset_state, sizeof(reset_state)),
              "serialize reset timer and random state") ||
       !check(memcmp(wait_state, reset_state, sizeof(wait_state)) == 0,
              "reset restores timers and random seed"))
      return false;

   for (frame = 0; frame < 4; frame++)
      api->retro_run();
   if (!check(api->retro_serialize(reset_state, sizeof(reset_state)),
              "serialize expired timer state") ||
       !check(reset_state[chip8_state_delay_offset] == 0 &&
                 reset_state[chip8_state_sound_offset] == 0,
              "timers expire while waiting for a key"))
      return false;

   api->retro_unload_game();
   return true;
}

static bool run_session(const struct core_api *api)
{
   const size_t expected_state_size = chip8_state_size;
   uint8_t one_byte_content = 0;
   uint8_t *oversized_content = NULL;
   uint8_t *state = NULL;
   uint8_t *current_state = NULL;
   uint8_t *system_ram;
   void *system_ram_pointer;
   struct retro_game_info game;
   struct retro_system_info system_info;
   struct retro_system_av_info av_info;
   uint64_t no_input_hash;
   bool valid = false;

   memset(&observations, 0, sizeof(observations));
   memset(&system_info, 0, sizeof(system_info));
   api->retro_get_system_info(&system_info);
   if (!check(api->retro_api_version() == RETRO_API_VERSION, "API version") ||
       !check(system_info.library_name != NULL &&
                 strcmp(system_info.library_name, "CoreKit CHIP-8") == 0,
              "library name") ||
       !check(system_info.library_version != NULL &&
                 strcmp(system_info.library_version, "0.2.0-phase4") == 0,
              "library version") ||
       !check(system_info.valid_extensions != NULL &&
                 strcmp(system_info.valid_extensions, "ch8") == 0,
              "content extension") ||
       !check(!system_info.need_fullpath && !system_info.block_extract,
              "in-memory content metadata"))
      return false;

   api->retro_set_environment(environment_callback);
   api->retro_set_video_refresh(video_callback);
   api->retro_set_audio_sample(audio_sample_callback);
   api->retro_set_audio_sample_batch(audio_batch_callback);
   api->retro_set_input_poll(input_poll_callback);
   api->retro_set_input_state(input_state_callback);

   if (!check(!api->retro_load_game(NULL), "load before initialization"))
      return false;

   api->retro_init();
   if (!check(observations.input_descriptor_calls == 1, "input descriptors") ||
       !check(observations.input_bitmask_calls == 1, "input bitmask negotiation"))
      return false;

   if (!check(!api->retro_load_game(NULL), "contentless load rejection"))
      return false;

   memset(&game, 0, sizeof(game));
   game.data = &one_byte_content;
   game.size = 1;
   if (!check(!api->retro_load_game(&game), "truncated content rejection"))
      return false;

   oversized_content = calloc(3585, 1);
   if (oversized_content == NULL)
      goto cleanup;
   game.data = oversized_content;
   game.size = 3585;
   if (!check(!api->retro_load_game(&game), "oversized content rejection"))
      goto cleanup;

   game.path = "/corekit/test.ch8";
   game.data = test_content;
   game.size = sizeof(test_content);
   game.meta = NULL;
   if (!check(api->retro_load_game(&game), "bounded in-memory content load") ||
       !check(observations.pixel_format_calls == 1, "pixel format negotiation") ||
       !check(observations.controller_info_calls == 1, "controller metadata"))
      goto cleanup;

   memset(&av_info, 0, sizeof(av_info));
   api->retro_get_system_av_info(&av_info);
   if (!check(av_info.geometry.base_width == 64 && av_info.geometry.base_height == 32,
              "CHIP-8 geometry") ||
       !check(av_info.geometry.aspect_ratio == 2.0f, "CHIP-8 aspect ratio") ||
       !check(av_info.timing.fps == 60.0, "frame rate"))
      goto cleanup;

   system_ram_pointer = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   system_ram = system_ram_pointer;
   if (!check(system_ram != NULL, "system RAM pointer") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) == 4096,
              "system RAM size") ||
       !check(memcmp(&system_ram[0x200], test_content, sizeof(test_content)) == 0,
              "content copied to CHIP-8 program memory") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM) == NULL,
              "save RAM remains unsupported"))
      goto cleanup;

   api->retro_run();
   no_input_hash = observations.last_video_hash;
   if (!check(observations.left_sprite && !observations.right_sprite,
              "program executes without input") ||
       !check(observations.input_poll_calls == 1 && observations.input_state_calls == 1,
              "RetroPad polling") ||
       !check(observations.audio_batch_calls == 1, "silent audio timing batch") ||
       !check(observations.audio_sample_calls == 0, "batch audio preference"))
      goto cleanup;

   if (!check(api->retro_serialize_size() == expected_state_size,
              "serialized state size"))
      goto cleanup;
   state = malloc(expected_state_size);
   current_state = malloc(expected_state_size);
   if (state == NULL || current_state == NULL)
      goto cleanup;
   if (!check(api->retro_serialize(state, expected_state_size), "serialize state") ||
       !check(memcmp(state, "C8S2", 4) == 0, "serialized state header"))
      goto cleanup;

   memcpy(current_state, state, expected_state_size);
   current_state[6] |= 1;
   if (!check(!api->retro_unserialize(current_state, expected_state_size),
              "malformed state rejection") ||
       !check(api->retro_serialize(current_state, expected_state_size),
              "serialize after rejected state") ||
       !check(memcmp(state, current_state, expected_state_size) == 0,
              "rejected state is transactional"))
      goto cleanup;

   memcpy(current_state, state, expected_state_size);
   memcpy(current_state, "C8S1\x01\x00", 6);
   if (!check(!api->retro_unserialize(current_state, expected_state_size),
              "version-1 state rejection") ||
       !check(api->retro_serialize(current_state, expected_state_size),
              "serialize after rejected version-1 state") ||
       !check(memcmp(state, current_state, expected_state_size) == 0,
              "rejected version-1 state is transactional"))
      goto cleanup;

   memcpy(current_state, state, expected_state_size);
   memset(&current_state[chip8_state_random_offset], 0, sizeof(uint32_t));
   if (!check(!api->retro_unserialize(current_state, expected_state_size),
              "zero random state rejection") ||
       !check(api->retro_serialize(current_state, expected_state_size),
              "serialize after rejected random state") ||
       !check(memcmp(state, current_state, expected_state_size) == 0,
              "rejected random state is transactional"))
      goto cleanup;

   observations.provide_right = true;
   api->retro_reset();
   api->retro_run();
   if (!check(observations.right_sprite && !observations.left_sprite,
              "RetroPad changes program output") ||
       !check(observations.last_video_hash != no_input_hash,
              "input-sensitive video hash") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == system_ram_pointer,
              "system RAM pointer survives reset"))
      goto cleanup;

   observations.provide_right = false;
   if (!check(api->retro_unserialize(state, expected_state_size), "unserialize state"))
      goto cleanup;
   api->retro_run();
   if (!check(observations.last_video_hash == no_input_hash,
              "state restores deterministic video") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == system_ram_pointer,
              "system RAM pointer survives state load"))
      goto cleanup;

   api->retro_reset();
   api->retro_run();
   if (!check(observations.last_video_hash == no_input_hash,
              "reset restores deterministic execution"))
      goto cleanup;

   observations.provide_right = true;
   api->retro_set_controller_port_device(0, RETRO_DEVICE_NONE);
   api->retro_reset();
   api->retro_run();
   if (!check(observations.left_sprite && !observations.right_sprite,
              "disabled controller suppresses RetroPad state"))
      goto cleanup;

   if (!check(!api->retro_load_game_special(0, &game, 1),
              "special content remains unsupported") ||
       !check(api->retro_get_region() == RETRO_REGION_NTSC, "libretro region"))
      goto cleanup;

   api->retro_cheat_reset();
   api->retro_cheat_set(0, false, NULL);
   api->retro_unload_game();
   if (!check(api->retro_serialize_size() == 0, "state unavailable after unload") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == NULL,
              "system RAM unavailable after unload"))
      goto cleanup;

   api->retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
   if (!run_arithmetic_suite(api) || !run_timer_random_suite(api) ||
       !check(api->retro_serialize_size() == 0,
              "state unavailable after instruction suites"))
      goto cleanup;

   api->retro_deinit();
   if (!check(!observations.callback_error, "frontend callback arguments"))
      goto cleanup;

   valid = true;

cleanup:
   if (!valid)
      api->retro_deinit();
   free(current_state);
   free(state);
   free(oversized_content);
   return valid;
}

static bool parse_cycles(const char *text, unsigned *cycles)
{
   char *end;
   unsigned long parsed;

   errno = 0;
   parsed = strtoul(text, &end, 10);
   if (errno != 0 || *text == '\0' || *end != '\0' || parsed == 0 || parsed > 1000000)
      return false;
   *cycles = (unsigned)parsed;
   return true;
}

static bool write_result(const char *path, unsigned cycles, uint64_t video_hash)
{
   FILE *stream = fopen(path, "w");
   bool written;

   if (stream == NULL)
      return false;
   written = fprintf(
      stream,
      "{\"result\":\"pass\",\"cycles\":%u,\"video_hash\":\"%016llx\"}\n",
      cycles,
      (unsigned long long)video_hash) >= 0;
   if (fclose(stream) != 0)
      written = false;
   return written;
}

int main(int argc, char **argv)
{
   const char *result_path = NULL;
   unsigned cycles = 25;
   unsigned cycle;
   uint64_t video_hash = 0;

   if (argc < 2 || argc > 4)
   {
      fprintf(stderr, "usage: %s CORE_PATH [CYCLES] [RESULT_PATH]\n", argv[0]);
      return EXIT_FAILURE;
   }
   if (argc >= 3 && !parse_cycles(argv[2], &cycles))
   {
      fprintf(stderr, "invalid cycle count: %s\n", argv[2]);
      return EXIT_FAILURE;
   }
   if (argc == 4)
      result_path = argv[3];

   for (cycle = 0; cycle < cycles; cycle++)
   {
      void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
      struct core_api api;

      if (handle == NULL)
      {
         fprintf(stderr, "could not open CHIP-8 core: %s\n", dlerror());
         return EXIT_FAILURE;
      }
      if (!load_api(handle, &api) || !run_session(&api))
      {
         (void)dlclose(handle);
         fprintf(stderr, "CHIP-8 lifecycle cycle %u failed\n", cycle + 1);
         return EXIT_FAILURE;
      }
      if (cycle == 0)
         video_hash = observations.last_video_hash;
      if (dlclose(handle) != 0)
      {
         fprintf(stderr, "could not close CHIP-8 core: %s\n", dlerror());
         return EXIT_FAILURE;
      }
   }

   if (result_path != NULL && !write_result(result_path, cycles, video_hash))
   {
      fprintf(stderr, "could not write result file: %s\n", result_path);
      return EXIT_FAILURE;
   }

   printf("PASS: %u CHIP-8 content lifecycle cycles\n", cycles);
   return EXIT_SUCCESS;
}
