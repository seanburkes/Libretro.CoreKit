#include "libretro.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FRAME_WIDTH 160u
#define FRAME_HEIGHT 144u
#define AUDIO_FRAMES 800u
#define SAVE_RAM_SIZE 64u

static retro_environment_t environment_callback;
static retro_video_refresh_t video_callback;
static retro_audio_sample_batch_t audio_batch_callback;
static retro_input_poll_t input_poll_callback;
static uint32_t frame[FRAME_WIDTH * FRAME_HEIGHT];
static int16_t audio[AUDIO_FRAMES * 2u];
static uint8_t save_ram[SAVE_RAM_SIZE];
static bool content_loaded;

void retro_set_environment(retro_environment_t callback)
{
   bool supports_no_game = true;
   environment_callback = callback;
   if (callback)
      callback(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &supports_no_game);
}

void retro_set_video_refresh(retro_video_refresh_t callback)
{
   video_callback = callback;
}

void retro_set_audio_sample(retro_audio_sample_t callback)
{
   (void)callback;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback)
{
   audio_batch_callback = callback;
}

void retro_set_input_poll(retro_input_poll_t callback)
{
   input_poll_callback = callback;
}

void retro_set_input_state(retro_input_state_t callback)
{
   (void)callback;
}

void retro_init(void)
{
   memset(frame, 0x20, sizeof(frame));
}

void retro_deinit(void)
{
   environment_callback = NULL;
   video_callback = NULL;
   audio_batch_callback = NULL;
   input_poll_callback = NULL;
   content_loaded = false;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "CoreKit Conventional Control";
   info->library_version = "0.1.0";
   info->valid_extensions = "";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width = FRAME_WIDTH;
   info->geometry.base_height = FRAME_HEIGHT;
   info->geometry.max_width = FRAME_WIDTH;
   info->geometry.max_height = FRAME_HEIGHT;
   info->geometry.aspect_ratio = (float)FRAME_WIDTH / FRAME_HEIGHT;
   info->timing.fps = 60.0;
   info->timing.sample_rate = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_reset(void)
{
}

void retro_run(void)
{
   if (!content_loaded)
      return;
   if (input_poll_callback)
      input_poll_callback();
   if (audio_batch_callback)
      audio_batch_callback(audio, AUDIO_FRAMES);
   if (video_callback)
      video_callback(frame, FRAME_WIDTH, FRAME_HEIGHT,
            FRAME_WIDTH * sizeof(uint32_t));
   save_ram[0]++;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   if (!environment_callback ||
       !environment_callback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
      return false;
   memset(save_ram, 0, sizeof(save_ram));
   content_loaded = true;
   return true;
}

bool retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return false;
}

void retro_unload_game(void)
{
   content_loaded = false;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
   return content_loaded && id == RETRO_MEMORY_SAVE_RAM ? save_ram : NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   return content_loaded && id == RETRO_MEMORY_SAVE_RAM ? sizeof(save_ram) : 0;
}
