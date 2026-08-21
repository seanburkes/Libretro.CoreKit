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
   unsigned core_options_v2_calls;
   unsigned input_bitmask_calls;
   unsigned variable_update_calls;
   unsigned variable_calls;
   unsigned audio_video_enable_calls;
   unsigned fast_forwarding_calls;
   unsigned video_calls;
   unsigned audio_batch_calls;
   unsigned audio_sample_calls;
   unsigned input_poll_calls;
   unsigned input_state_calls;
   uint64_t last_video_hash;
   uint64_t last_audio_hash;
   uint64_t tone_audio_hash;
   uint16_t input_mask;
   bool option_update_pending;
   bool alternative_quirks;
   bool left_sprite;
   bool right_sprite;
   bool last_audio_silent;
   bool last_audio_tone;
   bool callback_error;
};

static struct observations observations;

struct keypad_mapping
{
   unsigned retro_id;
   uint8_t chip8_key;
   const char *description;
};

static const struct keypad_mapping keypad_mappings[] = {
   {RETRO_DEVICE_ID_JOYPAD_B, 0x0, "CHIP-8 key 0"},
   {RETRO_DEVICE_ID_JOYPAD_Y, 0x1, "CHIP-8 key 1"},
   {RETRO_DEVICE_ID_JOYPAD_SELECT, 0xC, "CHIP-8 key C"},
   {RETRO_DEVICE_ID_JOYPAD_START, 0xD, "CHIP-8 key D"},
   {RETRO_DEVICE_ID_JOYPAD_UP, 0x2, "CHIP-8 key 2"},
   {RETRO_DEVICE_ID_JOYPAD_DOWN, 0x8, "CHIP-8 key 8"},
   {RETRO_DEVICE_ID_JOYPAD_LEFT, 0x4, "CHIP-8 key 4"},
   {RETRO_DEVICE_ID_JOYPAD_RIGHT, 0x6, "CHIP-8 key 6"},
   {RETRO_DEVICE_ID_JOYPAD_A, 0x5, "CHIP-8 key 5"},
   {RETRO_DEVICE_ID_JOYPAD_X, 0x3, "CHIP-8 key 3"},
   {RETRO_DEVICE_ID_JOYPAD_L, 0x7, "CHIP-8 key 7"},
   {RETRO_DEVICE_ID_JOYPAD_R, 0x9, "CHIP-8 key 9"},
   {RETRO_DEVICE_ID_JOYPAD_L2, 0xA, "CHIP-8 key A"},
   {RETRO_DEVICE_ID_JOYPAD_R2, 0xB, "CHIP-8 key B"},
   {RETRO_DEVICE_ID_JOYPAD_L3, 0xE, "CHIP-8 key E"},
   {RETRO_DEVICE_ID_JOYPAD_R3, 0xF, "CHIP-8 key F"},
};

struct option_expectation
{
   const char *key;
   const char *category;
   const char *default_value;
   const char *alternate_value;
};

static const struct option_expectation option_expectations[] = {
   {"corekit_chip8_shift_source", "emulation", "vx", "vy"},
   {"corekit_chip8_logic_vf", "emulation", "preserve", "clear"},
   {"corekit_chip8_memory_index", "emulation", "unchanged", "increment"},
   {"corekit_chip8_jump_offset", "emulation", "v0", "vx"},
   {"corekit_chip8_index_overflow", "emulation", "preserve", "set"},
   {"corekit_chip8_sprite_edges", "video", "wrap", "clip"},
};

#define CHIP8_OPCODE(value) (uint8_t)((value) >> 8), (uint8_t)(value)

enum
{
   chip8_state_size = 6216,
   chip8_state_pc_offset = 6,
   chip8_state_random_offset = 10,
   chip8_state_delay_offset = 14,
   chip8_state_sound_offset = 15,
   chip8_state_stack_pointer_offset = 16,
   chip8_state_halted_offset = 17,
   chip8_state_quirks_offset = 18,
   chip8_state_audio_phase_offset = 20,
   chip8_state_register_offset = 24,
   chip8_state_stack_offset = 40,
   chip8_state_display_offset = 72,
   chip8_all_quirks = 0x3F,
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
   CHIP8_OPCODE(0x6002),
   CHIP8_OPCODE(0xF018),
   CHIP8_OPCODE(0xF10A),
   CHIP8_OPCODE(0xF207),
   CHIP8_OPCODE(0xC3FF),
   CHIP8_OPCODE(0xC4F0),
   CHIP8_OPCODE(0xA350),
   CHIP8_OPCODE(0xF455),
   CHIP8_OPCODE(0x1214),
};

static const uint8_t keypad_content[] = {
   CHIP8_OPCODE(0xF00A),
   CHIP8_OPCODE(0xA300),
   CHIP8_OPCODE(0xF055),
   CHIP8_OPCODE(0x1200),
};

static const uint8_t shift_logic_transfer_content[] = {
   CHIP8_OPCODE(0x6003),
   CHIP8_OPCODE(0x6108),
   CHIP8_OPCODE(0x8016),
   CHIP8_OPCODE(0x82F0),
   CHIP8_OPCODE(0x6FAA),
   CHIP8_OPCODE(0x63F0),
   CHIP8_OPCODE(0x640F),
   CHIP8_OPCODE(0x8341),
   CHIP8_OPCODE(0x85F0),
   CHIP8_OPCODE(0xA300),
   CHIP8_OPCODE(0xF555),
   CHIP8_OPCODE(0x0000),
};

static const uint8_t jump_content[] = {
   CHIP8_OPCODE(0x6002),
   CHIP8_OPCODE(0x6220),
   CHIP8_OPCODE(0xB2F0),
};

static const uint8_t default_jump_target[] = {
   CHIP8_OPCODE(0x63D0),
   CHIP8_OPCODE(0xA320),
   CHIP8_OPCODE(0xF355),
   CHIP8_OPCODE(0x0000),
};

static const uint8_t alternative_jump_target[] = {
   CHIP8_OPCODE(0x63A0),
   CHIP8_OPCODE(0xA320),
   CHIP8_OPCODE(0xF355),
   CHIP8_OPCODE(0x0000),
};

static const uint8_t index_overflow_content[] = {
   CHIP8_OPCODE(0x6F7E),
   CHIP8_OPCODE(0xAFFF),
   CHIP8_OPCODE(0x6001),
   CHIP8_OPCODE(0xF01E),
};

static const uint8_t sprite_edges_content[] = {
   CHIP8_OPCODE(0x607F),
   CHIP8_OPCODE(0x613F),
   CHIP8_OPCODE(0xA20A),
   CHIP8_OPCODE(0xD011),
   CHIP8_OPCODE(0x0000),
   0xFF,
};

static const uint8_t unsupported_instruction_content[] = {
   CHIP8_OPCODE(0x5011),
};

static const uint8_t return_underflow_content[] = {
   CHIP8_OPCODE(0x00EE),
};

static const uint8_t recursive_call_content[] = {
   CHIP8_OPCODE(0x2200),
};

static const uint8_t odd_jump_content[] = {
   CHIP8_OPCODE(0x1201),
};

static const uint8_t end_jump_content[] = {
   CHIP8_OPCODE(0x1FFF),
};

static const uint8_t end_call_content[] = {
   CHIP8_OPCODE(0x2FFF),
};

static const uint8_t skip_past_memory_content[] = {
   CHIP8_OPCODE(0x6000),
   CHIP8_OPCODE(0x1FFE),
};

static const uint8_t boundary_call_content[] = {
   CHIP8_OPCODE(0x1FFE),
};

static const uint8_t jump_overflow_content[] = {
   CHIP8_OPCODE(0x60FF),
   CHIP8_OPCODE(0xBFFF),
};

static const uint8_t invalid_font_content[] = {
   CHIP8_OPCODE(0x60FF),
   CHIP8_OPCODE(0xF029),
};

static const uint8_t out_of_range_bcd_content[] = {
   CHIP8_OPCODE(0xAFFF),
   CHIP8_OPCODE(0x60FF),
   CHIP8_OPCODE(0xF033),
};

static const uint8_t out_of_range_store_content[] = {
   CHIP8_OPCODE(0xAFFF),
   CHIP8_OPCODE(0x60AA),
   CHIP8_OPCODE(0x61BB),
   CHIP8_OPCODE(0xF155),
};

static const uint8_t out_of_range_load_content[] = {
   CHIP8_OPCODE(0xAFFF),
   CHIP8_OPCODE(0xF165),
};

static const uint8_t out_of_range_sprite_content[] = {
   CHIP8_OPCODE(0xAFFF),
   CHIP8_OPCODE(0x6000),
   CHIP8_OPCODE(0x6100),
   CHIP8_OPCODE(0xD012),
};

static const uint8_t zero_height_sprite_content[] = {
   CHIP8_OPCODE(0xA050),
   CHIP8_OPCODE(0x6000),
   CHIP8_OPCODE(0x6100),
   CHIP8_OPCODE(0xD010),
};

static const uint8_t skip_past_memory_patch[] = {
   CHIP8_OPCODE(0x3000),
};

static const uint8_t boundary_call_patch[] = {
   CHIP8_OPCODE(0x2200),
};

static const uint8_t out_of_range_load_patch[] = {0xCC};

struct malformed_program_case
{
   const char *name;
   const char *path;
   const uint8_t *content;
   size_t content_size;
   unsigned frames_to_halt;
   uint16_t expected_program_counter;
   uint8_t expected_stack_pointer;
   uint16_t patch_address;
   const uint8_t *patch;
   size_t patch_size;
   uint16_t preserved_address;
   uint8_t preserved_value;
};

#define MALFORMED_CASE(label, fixture, frames, pc, stack) \
   {label, "/corekit/malformed-" label ".ch8", fixture, sizeof(fixture), \
    frames, pc, stack, 0, NULL, 0, UINT16_MAX, 0}

static const struct malformed_program_case malformed_program_cases[] = {
   MALFORMED_CASE("unsupported-instruction", unsupported_instruction_content, 1, 0x202, 0),
   MALFORMED_CASE("return-underflow", return_underflow_content, 1, 0x202, 0),
   MALFORMED_CASE("recursive-call-overflow", recursive_call_content, 2, 0x202, 16),
   MALFORMED_CASE("odd-jump", odd_jump_content, 1, 0x203, 0),
   MALFORMED_CASE("end-jump", end_jump_content, 1, 0xFFF, 0),
   MALFORMED_CASE("end-call", end_call_content, 1, 0xFFF, 1),
   {
      "skip-past-memory", "/corekit/malformed-skip-past-memory.ch8",
      skip_past_memory_content, sizeof(skip_past_memory_content), 1, 0x1002, 0,
      0xFFE, skip_past_memory_patch, sizeof(skip_past_memory_patch), UINT16_MAX, 0,
   },
   {
      "boundary-call-overflow", "/corekit/malformed-boundary-call-overflow.ch8",
      boundary_call_content, sizeof(boundary_call_content), 3, 0x1000, 16,
      0xFFE, boundary_call_patch, sizeof(boundary_call_patch), UINT16_MAX, 0,
   },
   MALFORMED_CASE("jump-overflow", jump_overflow_content, 1, 0x204, 0),
   MALFORMED_CASE("invalid-font-digit", invalid_font_content, 1, 0x204, 0),
   {
      "out-of-range-bcd", "/corekit/malformed-out-of-range-bcd.ch8",
      out_of_range_bcd_content, sizeof(out_of_range_bcd_content), 1, 0x206, 0,
      0, NULL, 0, 0xFFF, 0,
   },
   {
      "out-of-range-store", "/corekit/malformed-out-of-range-store.ch8",
      out_of_range_store_content, sizeof(out_of_range_store_content), 1, 0x208, 0,
      0, NULL, 0, 0xFFF, 0,
   },
   {
      "out-of-range-load", "/corekit/malformed-out-of-range-load.ch8",
      out_of_range_load_content, sizeof(out_of_range_load_content), 1, 0x204, 0,
      0xFFF, out_of_range_load_patch, sizeof(out_of_range_load_patch), 0xFFF, 0xCC,
   },
   MALFORMED_CASE("out-of-range-sprite", out_of_range_sprite_content, 1, 0x208, 0),
   MALFORMED_CASE("zero-height-sprite", zero_height_sprite_content, 1, 0x208, 0),
};

#undef MALFORMED_CASE

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

static bool validate_core_options(const struct retro_core_options_v2 *options)
{
   size_t index;

   if (options == NULL || options->categories == NULL || options->definitions == NULL ||
       options->categories[0].key == NULL ||
       strcmp(options->categories[0].key, "emulation") != 0 ||
       options->categories[1].key == NULL ||
       strcmp(options->categories[1].key, "video") != 0 ||
       options->categories[2].key != NULL)
      return false;

   for (index = 0;
        index < sizeof(option_expectations) / sizeof(option_expectations[0]);
        index++)
   {
      const struct retro_core_option_v2_definition *definition =
         &options->definitions[index];
      const struct option_expectation *expected = &option_expectations[index];
      if (definition->key == NULL || strcmp(definition->key, expected->key) != 0 ||
          definition->desc == NULL || definition->info == NULL ||
          definition->category_key == NULL ||
          strcmp(definition->category_key, expected->category) != 0 ||
          definition->values[0].value == NULL ||
          strcmp(definition->values[0].value, expected->default_value) != 0 ||
          definition->values[0].label == NULL ||
          definition->values[1].value == NULL ||
          strcmp(definition->values[1].value, expected->alternate_value) != 0 ||
          definition->values[1].label == NULL ||
          definition->values[2].value != NULL ||
          definition->default_value == NULL ||
          strcmp(definition->default_value, expected->default_value) != 0)
         return false;
   }

   return options->definitions[index].key == NULL;
}

static const char *get_option_value(const char *key)
{
   size_t index;

   for (index = 0;
        index < sizeof(option_expectations) / sizeof(option_expectations[0]);
        index++)
   {
      if (strcmp(key, option_expectations[index].key) == 0)
         return observations.alternative_quirks
            ? option_expectations[index].alternate_value
            : option_expectations[index].default_value;
   }

   return NULL;
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

   if (command == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2)
   {
      if (!validate_core_options(data))
         observations.callback_error = true;
      observations.core_options_v2_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS)
   {
      const struct retro_input_descriptor *descriptors = data;
      size_t mapping_index;
      if (descriptors == NULL)
         observations.callback_error = true;
      else
      {
         for (mapping_index = 0;
              mapping_index < sizeof(keypad_mappings) / sizeof(keypad_mappings[0]);
              mapping_index++)
         {
            if (descriptors[mapping_index].port != 0 ||
                descriptors[mapping_index].device != RETRO_DEVICE_JOYPAD ||
                descriptors[mapping_index].index != 0 ||
                descriptors[mapping_index].id != keypad_mappings[mapping_index].retro_id ||
                descriptors[mapping_index].description == NULL ||
                strcmp(descriptors[mapping_index].description,
                       keypad_mappings[mapping_index].description) != 0)
               observations.callback_error = true;
         }
         if (descriptors[mapping_index].description != NULL)
            observations.callback_error = true;
      }
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
      {
         *(bool *)data = observations.option_update_pending;
         observations.option_update_pending = false;
      }
      observations.variable_update_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_GET_VARIABLE)
   {
      struct retro_variable *variable = data;
      const char *value = NULL;
      if (variable == NULL || variable->key == NULL ||
          (value = get_option_value(variable->key)) == NULL)
         observations.callback_error = true;
      else
         variable->value = value;
      observations.variable_calls++;
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
   size_t frame;
   bool silent = true;
   bool tone = true;
   bool saw_positive = false;
   bool saw_negative = false;

   if (data == NULL || frames != 800)
      observations.callback_error = true;
   observations.last_audio_hash = data == NULL
      ? 0
      : hash_bytes(data, frames * 2 * sizeof(*data));
   for (frame = 0; data != NULL && frame < frames; frame++)
   {
      int16_t left = data[frame * 2];
      int16_t right = data[(frame * 2) + 1];

      if (left != right)
         observations.callback_error = true;
      if (left != 0)
         silent = false;
      if (left == 6000)
         saw_positive = true;
      else if (left == -6000)
         saw_negative = true;
      else
         tone = false;
   }
   observations.last_audio_silent = silent;
   observations.last_audio_tone = tone && saw_positive && saw_negative;
   if (observations.last_audio_tone && observations.tone_audio_hash == 0)
      observations.tone_audio_hash = observations.last_audio_hash;
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
      return (int16_t)observations.input_mask;
   return id < 16 && (observations.input_mask & (uint16_t)(1U << id)) != 0 ? 1 : 0;
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

static void write_u32_le(uint8_t *data, uint32_t value)
{
   data[0] = (uint8_t)value;
   data[1] = (uint8_t)(value >> 8);
   data[2] = (uint8_t)(value >> 16);
   data[3] = (uint8_t)(value >> 24);
}

static void write_u16_le(uint8_t *data, uint16_t value)
{
   data[0] = (uint8_t)value;
   data[1] = (uint8_t)(value >> 8);
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
   static const uint8_t expected_output[] = {0x02, 0x06, 0x04, 0x22, 0xC0};
   uint8_t wait_state[chip8_state_size];
   uint8_t completed_state[chip8_state_size];
   uint8_t replay_state[chip8_state_size];
   uint8_t reset_state[chip8_state_size];
   uint8_t *system_ram;
   uint64_t first_audio_hash;
   uint64_t second_audio_hash;
   unsigned frame;

   observations.input_mask = 0;
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
       !check(observations.last_audio_tone, "sound timer emits stereo tone") ||
       !check(observations.last_audio_hash != 0, "first tone audio hash") ||
       !check(read_u16_le(&wait_state[chip8_state_pc_offset]) == 0x208,
              "key wait retains program counter") ||
       !check(read_u32_le(&wait_state[chip8_state_random_offset]) == UINT32_C(0xC0DEF00D),
              "random state is unchanged while waiting") ||
       !check(wait_state[chip8_state_delay_offset] == 4 &&
                 wait_state[chip8_state_sound_offset] == 1,
              "timers tick at end of waiting frame") ||
       !check(read_u32_le(&wait_state[chip8_state_audio_phase_offset]) == 16000,
              "first tone frame advances audio phase"))
      return false;
   first_audio_hash = observations.last_audio_hash;

   observations.input_mask = (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   api->retro_run();
   if (!check(memcmp(&system_ram[0x350], expected_output, sizeof(expected_output)) == 0,
              "key, timer, and deterministic random results") ||
       !check(observations.last_audio_tone, "continued sound-timer tone") ||
       !check(observations.last_audio_hash != first_audio_hash,
              "tone phase changes the next audio batch") ||
       !check(api->retro_serialize(completed_state, sizeof(completed_state)),
              "serialize timer and random state") ||
       !check(read_u16_le(&completed_state[chip8_state_pc_offset]) == 0x214,
              "timer program reaches terminal loop") ||
       !check(read_u32_le(&completed_state[chip8_state_random_offset]) == UINT32_C(0x394B8BCA),
              "deterministic random sequence state") ||
       !check(completed_state[chip8_state_delay_offset] == 3 &&
                 completed_state[chip8_state_sound_offset] == 0,
              "timers tick once per completed frame") ||
       !check(read_u32_le(&completed_state[chip8_state_audio_phase_offset]) == 32000,
              "second tone frame advances audio phase"))
      return false;
   second_audio_hash = observations.last_audio_hash;

   if (!check(api->retro_unserialize(wait_state, sizeof(wait_state)),
              "restore key-wait state"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(replay_state, sizeof(replay_state)),
              "serialize replayed timer and random state") ||
       !check(memcmp(completed_state, replay_state, sizeof(completed_state)) == 0,
              "timer, random, and audio phase replay deterministically") ||
       !check(observations.last_audio_hash == second_audio_hash,
              "state restores deterministic audio batch"))
      return false;

   observations.input_mask = 0;
   api->retro_reset();
   api->retro_run();
   if (!check(api->retro_serialize(reset_state, sizeof(reset_state)),
              "serialize reset timer and random state") ||
       !check(memcmp(wait_state, reset_state, sizeof(wait_state)) == 0,
              "reset restores timers, random seed, and audio phase") ||
       !check(observations.last_audio_hash == first_audio_hash,
              "reset restores deterministic audio batch"))
      return false;

   for (frame = 0; frame < 4; frame++)
      api->retro_run();
   if (!check(api->retro_serialize(reset_state, sizeof(reset_state)),
              "serialize expired timer state") ||
       !check(reset_state[chip8_state_delay_offset] == 0 &&
                 reset_state[chip8_state_sound_offset] == 0,
              "timers expire while waiting for a key") ||
       !check(read_u32_le(&reset_state[chip8_state_audio_phase_offset]) == 32000,
              "silent frames preserve audio phase") ||
       !check(observations.last_audio_silent,
              "expired sound timer restores silent audio"))
      return false;

   api->retro_unload_game();
   return true;
}

static bool run_keypad_suite(const struct core_api *api)
{
   uint8_t *system_ram;
   size_t mapping_index;

   observations.input_mask = 0;
   if (!check(load_test_content(
                 api,
                 keypad_content,
                 sizeof(keypad_content),
                 "/corekit/keypad.ch8"),
              "keypad content load"))
      return false;

   system_ram = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (!check(system_ram != NULL, "keypad system RAM"))
      return false;

   for (mapping_index = 0;
        mapping_index < sizeof(keypad_mappings) / sizeof(keypad_mappings[0]);
        mapping_index++)
   {
      system_ram[0x300] = 0xFF;
      observations.input_mask = (uint16_t)(1U << keypad_mappings[mapping_index].retro_id);
      api->retro_run();
      if (!check(system_ram[0x300] == keypad_mappings[mapping_index].chip8_key,
                 keypad_mappings[mapping_index].description))
         return false;

      observations.input_mask = 0;
      api->retro_run();
   }

   api->retro_unload_game();
   return true;
}

static size_t count_set_display_pixels(const uint8_t *state)
{
   size_t count = 0;
   size_t pixel;

   for (pixel = 0; pixel < 64 * 32; pixel++)
   {
      if (state[chip8_state_display_offset + pixel] != 0)
         count++;
   }
   return count;
}

static bool run_quirk_suite(const struct core_api *api)
{
   static const uint8_t default_registers[] = {0x01, 0x08, 0x01, 0xFF, 0x0F, 0xAA};
   static const uint8_t alternative_registers[] = {0x04, 0x08, 0x00, 0xFF, 0x0F, 0x00};
   uint8_t default_state[chip8_state_size];
   uint8_t alternative_start[chip8_state_size];
   uint8_t alternative_state[chip8_state_size];
   uint8_t replay_state[chip8_state_size];
   uint8_t invalid_state[chip8_state_size];
   uint8_t *system_ram;

   observations.alternative_quirks = false;
   if (!check(load_test_content(
                 api,
                 shift_logic_transfer_content,
                 sizeof(shift_logic_transfer_content),
                 "/corekit/shift-logic-transfer.ch8"),
              "quirk content load"))
      return false;

   system_ram = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (!check(system_ram != NULL, "quirk system RAM"))
      return false;

   api->retro_run();
   if (!check(memcmp(&system_ram[0x300], default_registers,
                     sizeof(default_registers)) == 0,
              "default shift and logic behavior") ||
       !check(api->retro_serialize(default_state, sizeof(default_state)),
              "serialize default quirk state") ||
       !check(read_u16_le(&default_state[8]) == 0x300,
              "default transfer leaves index unchanged") ||
       !check(default_state[chip8_state_quirks_offset] == 0,
              "default quirk flags"))
      return false;

   observations.alternative_quirks = true;
   observations.option_update_pending = true;
   api->retro_reset();
   api->retro_run();
   if (!check(memcmp(&system_ram[0x300], alternative_registers,
                     sizeof(alternative_registers)) == 0,
              "updated shift and logic behavior") ||
       !check(api->retro_serialize(alternative_state, sizeof(alternative_state)),
              "serialize alternative quirk state") ||
       !check(read_u16_le(&alternative_state[8]) == 0x306,
              "updated transfer increments index") ||
       !check(alternative_state[chip8_state_quirks_offset] == chip8_all_quirks,
              "alternative quirk flags"))
      return false;

   api->retro_reset();
   if (!check(api->retro_serialize(alternative_start, sizeof(alternative_start)),
              "serialize alternative reset state"))
      return false;

   observations.alternative_quirks = false;
   observations.option_update_pending = true;
   api->retro_run();
   if (!check(memcmp(&system_ram[0x300], default_registers,
                     sizeof(default_registers)) == 0,
              "restored frontend defaults"))
      return false;

   if (!check(api->retro_unserialize(alternative_start, sizeof(alternative_start)),
              "restore alternative reset state"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(replay_state, sizeof(replay_state)),
              "serialize alternative replay") ||
       !check(memcmp(alternative_state, replay_state, sizeof(alternative_state)) == 0,
              "state restores quirk behavior deterministically"))
      return false;

   memcpy(invalid_state, alternative_state, sizeof(invalid_state));
   invalid_state[chip8_state_quirks_offset] = 0x80;
   if (!check(!api->retro_unserialize(invalid_state, sizeof(invalid_state)),
              "unknown quirk flag rejection") ||
       !check(api->retro_serialize(invalid_state, sizeof(invalid_state)),
              "serialize after rejected quirk flags") ||
       !check(memcmp(alternative_state, invalid_state, sizeof(alternative_state)) == 0,
              "rejected quirk flags are transactional"))
      return false;

   api->retro_unload_game();

   observations.alternative_quirks = false;
   if (!check(load_test_content(
                 api,
                 jump_content,
                 sizeof(jump_content),
                 "/corekit/jump-default.ch8"),
              "default jump content load"))
      return false;
   system_ram = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (system_ram != NULL)
      memcpy(&system_ram[0x2F2], default_jump_target, sizeof(default_jump_target));
   api->retro_run();
   if (!check(system_ram != NULL && system_ram[0x323] == 0xD0,
              "Bnnn uses V0 by default"))
      return false;
   api->retro_unload_game();

   observations.alternative_quirks = true;
   if (!check(load_test_content(
                 api,
                 jump_content,
                 sizeof(jump_content),
                 "/corekit/jump-vx.ch8"),
              "alternative jump content load"))
      return false;
   system_ram = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (system_ram != NULL)
      memcpy(&system_ram[0x110], alternative_jump_target, sizeof(alternative_jump_target));
   api->retro_run();
   if (!check(system_ram != NULL && system_ram[0x323] == 0xA0,
              "Bnnn is interpreted as Bxnn when configured"))
      return false;
   api->retro_unload_game();

   observations.alternative_quirks = false;
   if (!check(load_test_content(
                 api,
                 index_overflow_content,
                 sizeof(index_overflow_content),
                 "/corekit/index-overflow-default.ch8"),
              "default index-overflow content load"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(default_state, sizeof(default_state)),
              "serialize default index-overflow state") ||
       !check(default_state[chip8_state_register_offset + 0xF] == 0x7E,
              "Fx1E preserves VF by default"))
      return false;
   api->retro_unload_game();

   observations.alternative_quirks = true;
   if (!check(load_test_content(
                 api,
                 index_overflow_content,
                 sizeof(index_overflow_content),
                 "/corekit/index-overflow-set.ch8"),
              "alternative index-overflow content load"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(alternative_state, sizeof(alternative_state)),
              "serialize alternative index-overflow state") ||
       !check(alternative_state[chip8_state_register_offset + 0xF] == 1,
              "Fx1E sets VF on overflow when configured"))
      return false;
   api->retro_unload_game();

   observations.alternative_quirks = false;
   if (!check(load_test_content(
                 api,
                 sprite_edges_content,
                 sizeof(sprite_edges_content),
                 "/corekit/sprite-wrap.ch8"),
              "sprite-wrap content load"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(default_state, sizeof(default_state)),
              "serialize wrapped sprite state") ||
       !check(count_set_display_pixels(default_state) == 8,
              "sprites wrap by default"))
      return false;
   api->retro_unload_game();

   observations.alternative_quirks = true;
   if (!check(load_test_content(
                 api,
                 sprite_edges_content,
                 sizeof(sprite_edges_content),
                 "/corekit/sprite-clip.ch8"),
              "sprite-clip content load"))
      return false;
   api->retro_run();
   if (!check(api->retro_serialize(alternative_state, sizeof(alternative_state)),
              "serialize clipped sprite state") ||
       !check(count_set_display_pixels(alternative_state) == 1,
              "sprites clip when configured"))
      return false;
   api->retro_unload_game();

   observations.alternative_quirks = false;
   return true;
}

static bool check_malformed_case(
      bool condition, const struct malformed_program_case *scenario, const char *expectation)
{
   if (!condition)
      fprintf(stderr, "validation failed: %s: %s\n", scenario->name, expectation);
   return condition;
}

static void apply_malformed_patch(
      uint8_t *system_ram, const struct malformed_program_case *scenario)
{
   if (scenario->patch_size != 0)
      memcpy(&system_ram[scenario->patch_address], scenario->patch, scenario->patch_size);
}

static bool run_malformed_program_case(
      const struct core_api *api, const struct malformed_program_case *scenario)
{
   uint8_t initial_state[chip8_state_size];
   uint8_t halted_state[chip8_state_size];
   uint8_t stable_state[chip8_state_size];
   uint8_t replay_state[chip8_state_size];
   uint8_t *system_ram;
   unsigned frame;

   observations.input_mask = 0;
   observations.alternative_quirks = false;
   if (!check_malformed_case(
          load_test_content(api, scenario->content, scenario->content_size, scenario->path),
          scenario,
          "content load"))
      return false;

   system_ram = api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   if (!check_malformed_case(system_ram != NULL, scenario, "system RAM"))
      return false;
   apply_malformed_patch(system_ram, scenario);

   if (!check_malformed_case(
          api->retro_serialize(initial_state, sizeof(initial_state)),
          scenario,
          "serialize initial state"))
      return false;

   for (frame = 0; frame < scenario->frames_to_halt; frame++)
      api->retro_run();
   if (!check_malformed_case(
          api->retro_serialize(halted_state, sizeof(halted_state)),
          scenario,
          "serialize halted state") ||
       !check_malformed_case(
          halted_state[chip8_state_halted_offset] == 1,
          scenario,
          "virtual machine halted") ||
       !check_malformed_case(
          read_u16_le(&halted_state[chip8_state_pc_offset]) ==
             scenario->expected_program_counter,
          scenario,
          "program counter after halt") ||
       !check_malformed_case(
          halted_state[chip8_state_stack_pointer_offset] ==
             scenario->expected_stack_pointer,
          scenario,
          "stack pointer after halt") ||
       !check_malformed_case(
          count_set_display_pixels(halted_state) == 0,
          scenario,
          "display remains unchanged") ||
       !check_malformed_case(
          scenario->preserved_address == UINT16_MAX ||
             system_ram[scenario->preserved_address] == scenario->preserved_value,
          scenario,
          "failed memory operation is transactional"))
      return false;

   api->retro_run();
   if (!check_malformed_case(
          api->retro_serialize(stable_state, sizeof(stable_state)),
          scenario,
          "serialize stable halted state") ||
       !check_malformed_case(
          memcmp(halted_state, stable_state, sizeof(halted_state)) == 0,
          scenario,
          "halted frame is stable"))
      return false;

   if (!check_malformed_case(
          api->retro_unserialize(initial_state, sizeof(initial_state)),
          scenario,
          "restore initial state"))
      return false;
   for (frame = 0; frame < scenario->frames_to_halt; frame++)
      api->retro_run();
   if (!check_malformed_case(
          api->retro_serialize(replay_state, sizeof(replay_state)),
          scenario,
          "serialize state replay") ||
       !check_malformed_case(
          memcmp(halted_state, replay_state, sizeof(halted_state)) == 0,
          scenario,
          "state replay is deterministic"))
      return false;

   api->retro_reset();
   apply_malformed_patch(system_ram, scenario);
   for (frame = 0; frame < scenario->frames_to_halt; frame++)
      api->retro_run();
   if (!check_malformed_case(
          api->retro_serialize(replay_state, sizeof(replay_state)),
          scenario,
          "serialize reset replay") ||
       !check_malformed_case(
          memcmp(halted_state, replay_state, sizeof(halted_state)) == 0,
          scenario,
          "reset replay is deterministic"))
      return false;

   if (!check_malformed_case(
          api->retro_unserialize(halted_state, sizeof(halted_state)),
          scenario,
          "restore halted state") ||
       !check_malformed_case(
          api->retro_serialize(replay_state, sizeof(replay_state)),
          scenario,
          "serialize restored halted state") ||
       !check_malformed_case(
          memcmp(halted_state, replay_state, sizeof(halted_state)) == 0,
          scenario,
          "halted state round trip"))
      return false;
   api->retro_run();
   if (!check_malformed_case(
          api->retro_serialize(replay_state, sizeof(replay_state)),
          scenario,
          "serialize restored halted frame") ||
       !check_malformed_case(
          memcmp(halted_state, replay_state, sizeof(halted_state)) == 0,
          scenario,
          "restored halt remains stable"))
      return false;

   api->retro_unload_game();
   return true;
}

static bool run_malformed_program_suite(const struct core_api *api)
{
   size_t index;

   for (index = 0;
        index < sizeof(malformed_program_cases) / sizeof(malformed_program_cases[0]);
        index++)
   {
      if (!run_malformed_program_case(api, &malformed_program_cases[index]))
         return false;
   }

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
                 strcmp(system_info.library_version, "0.6.0-phase4") == 0,
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
       !check(observations.input_bitmask_calls == 1, "input bitmask negotiation") ||
       !check(observations.core_options_v2_calls == 1, "core-options registration"))
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
       !check(observations.controller_info_calls == 1, "controller metadata") ||
       !check(observations.variable_calls == 6, "initial core-option queries"))
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
       !check(observations.last_audio_silent, "zero sound timer emits silence") ||
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
       !check(memcmp(state, "C8S4", 4) == 0, "serialized state header"))
      goto cleanup;

   memcpy(current_state, state, expected_state_size);
   write_u16_le(&current_state[chip8_state_pc_offset], 0x1003);
   if (!check(!api->retro_unserialize(current_state, expected_state_size),
              "unreachable program-counter state rejection") ||
       !check(api->retro_serialize(current_state, expected_state_size),
              "serialize after rejected state") ||
       !check(memcmp(state, current_state, expected_state_size) == 0,
              "rejected state is transactional"))
      goto cleanup;

   memcpy(current_state, state, expected_state_size);
   current_state[chip8_state_stack_pointer_offset] = 1;
   write_u16_le(&current_state[chip8_state_stack_offset], 0x1001);
   if (!check(!api->retro_unserialize(current_state, expected_state_size),
              "unreachable return-address state rejection") ||
       !check(api->retro_serialize(current_state, expected_state_size),
              "serialize after rejected return address") ||
       !check(memcmp(state, current_state, expected_state_size) == 0,
              "rejected return address is transactional"))
      goto cleanup;

   memcpy(current_state, state, expected_state_size);
   memcpy(current_state, "C8S3\x03\x00", 6);
   if (!check(!api->retro_unserialize(current_state, expected_state_size),
              "version-3 state rejection") ||
       !check(api->retro_serialize(current_state, expected_state_size),
              "serialize after rejected version-3 state") ||
       !check(memcmp(state, current_state, expected_state_size) == 0,
              "rejected version-3 state is transactional"))
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

   memcpy(current_state, state, expected_state_size);
   write_u32_le(&current_state[chip8_state_audio_phase_offset], 48000);
   if (!check(!api->retro_unserialize(current_state, expected_state_size),
              "out-of-range audio phase rejection") ||
       !check(api->retro_serialize(current_state, expected_state_size),
              "serialize after rejected audio phase") ||
       !check(memcmp(state, current_state, expected_state_size) == 0,
              "rejected audio phase is transactional"))
      goto cleanup;

   observations.input_mask = (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   api->retro_reset();
   api->retro_run();
   if (!check(observations.right_sprite && !observations.left_sprite,
              "RetroPad changes program output") ||
       !check(observations.last_video_hash != no_input_hash,
              "input-sensitive video hash") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == system_ram_pointer,
              "system RAM pointer survives reset"))
      goto cleanup;

   observations.input_mask = 0;
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

   observations.input_mask = (uint16_t)(1U << RETRO_DEVICE_ID_JOYPAD_RIGHT);
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
       !run_keypad_suite(api) || !run_quirk_suite(api) ||
       !run_malformed_program_suite(api) ||
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

static bool write_result(
      const char *path,
      unsigned cycles,
      uint64_t video_hash,
      uint64_t audio_hash)
{
   FILE *stream = fopen(path, "w");
   bool written;

   if (stream == NULL)
      return false;
   written = fprintf(
      stream,
      "{\"result\":\"pass\",\"cycles\":%u,"
      "\"video_hash\":\"%016llx\",\"tone_audio_hash\":\"%016llx\"}\n",
      cycles,
      (unsigned long long)video_hash,
      (unsigned long long)audio_hash) >= 0;
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
   uint64_t audio_hash = 0;

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
      {
         video_hash = observations.last_video_hash;
         audio_hash = observations.tone_audio_hash;
      }
      if (dlclose(handle) != 0)
      {
         fprintf(stderr, "could not close CHIP-8 core: %s\n", dlerror());
         return EXIT_FAILURE;
      }
   }

   if (result_path != NULL && !write_result(result_path, cycles, video_hash, audio_hash))
   {
      fprintf(stderr, "could not write result file: %s\n", result_path);
      return EXIT_FAILURE;
   }

   printf("PASS: %u CHIP-8 content lifecycle cycles\n", cycles);
   return EXIT_SUCCESS;
}
