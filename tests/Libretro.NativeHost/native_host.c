#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
typedef HMODULE core_handle;
#else
#include <dlfcn.h>
typedef void *core_handle;
#endif

#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

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
   unsigned support_no_game_calls;
   unsigned pixel_format_calls;
   unsigned core_options_v2_calls;
   unsigned input_descriptor_calls;
   unsigned controller_info_calls;
   unsigned input_bitmask_calls;
   unsigned system_directory_calls;
   unsigned save_directory_calls;
   unsigned core_assets_directory_calls;
   unsigned language_calls;
   unsigned message_version_calls;
   unsigned message_calls;
   unsigned message_extended_calls;
   unsigned log_interface_calls;
   unsigned log_calls;
   unsigned variable_update_calls;
   unsigned variable_calls;
   unsigned audio_video_enable_calls;
   unsigned fast_forwarding_calls;
   unsigned video_calls;
   unsigned audio_batch_calls;
   unsigned audio_sample_calls;
   unsigned input_poll_calls;
   unsigned input_state_calls;
   unsigned right_press_reports;
   unsigned a_press_reports;
   size_t audio_frames;
   uint64_t first_video_hash;
   uint64_t second_video_hash;
   uint64_t no_input_reset_video_hash;
   uint64_t last_video_hash;
   uint64_t first_audio_hash;
   uint64_t second_audio_hash;
   uint64_t no_input_reset_audio_hash;
   uint64_t last_audio_hash;
   bool reject_pixel_format;
   bool support_optional_interfaces;
   bool option_update_pending;
   bool tone_option_enabled;
   bool monochrome_option_enabled;
   bool provide_input;
   bool suppress_audio_video;
   bool saw_nonzero_audio;
   bool last_audio_silent;
   bool last_video_monochrome;
   bool callback_error;
   const struct retro_input_descriptor *input_descriptors;
   const struct retro_controller_info *controller_info;
};

static struct observations observations;

static void RETRO_CALLCONV log_callback(
      enum retro_log_level level, const char *format, ...)
{
   const char *message;
   va_list arguments;

   message = NULL;
   if (format != NULL && strcmp(format, "%s") == 0)
   {
      va_start(arguments, format);
      message = va_arg(arguments, const char *);
      va_end(arguments);
   }

   if (format == NULL || strcmp(format, "%s") != 0 || message == NULL ||
       strncmp(message, "CoreKit ", 8) != 0 ||
       (level != RETRO_LOG_DEBUG && level != RETRO_LOG_INFO))
      observations.callback_error = true;
   observations.log_calls++;
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

static void reset_observations(void)
{
   memset(&observations, 0, sizeof(observations));
   observations.tone_option_enabled = true;
}

static bool RETRO_CALLCONV environment_callback(unsigned command, void *data)
{
   static const char system_directory[] = "/corekit/system";
   static const char save_directory[] = "/corekit/save";
   static const char core_assets_directory[] = "/corekit/assets";
   static const char tone_on[] = "on";
   static const char tone_off[] = "off";
   static const char palette_color[] = "color";
   static const char palette_monochrome[] = "monochrome";

   if (command == RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME)
   {
      if (data == NULL || !*(const bool *)data)
         observations.callback_error = true;
      observations.support_no_game_calls++;
      return true;
   }

   if (command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
   {
      if (data == NULL || *(const enum retro_pixel_format *)data != RETRO_PIXEL_FORMAT_XRGB8888)
         observations.callback_error = true;
      observations.pixel_format_calls++;
      return !observations.reject_pixel_format;
   }

   if (command == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2)
   {
      const struct retro_core_options_v2 *options = data;
      if (options == NULL || options->categories == NULL || options->definitions == NULL ||
          options->categories[0].key == NULL ||
          strcmp(options->categories[0].key, "audio") != 0 ||
          options->categories[0].desc == NULL ||
          strcmp(options->categories[0].desc, "Audio") != 0 ||
          options->categories[1].key == NULL ||
          strcmp(options->categories[1].key, "video") != 0 ||
          options->categories[1].desc == NULL ||
          strcmp(options->categories[1].desc, "Video") != 0 ||
          options->categories[2].key != NULL ||
          options->definitions[0].key == NULL ||
          strcmp(options->definitions[0].key, "corekit_probe_tone") != 0 ||
          options->definitions[0].category_key == NULL ||
          strcmp(options->definitions[0].category_key, "audio") != 0 ||
          options->definitions[0].values[0].value == NULL ||
          strcmp(options->definitions[0].values[0].value, "off") != 0 ||
          options->definitions[0].values[1].value == NULL ||
          strcmp(options->definitions[0].values[1].value, "on") != 0 ||
          options->definitions[0].values[2].value != NULL ||
          options->definitions[0].default_value == NULL ||
          strcmp(options->definitions[0].default_value, "on") != 0 ||
          options->definitions[1].key == NULL ||
          strcmp(options->definitions[1].key, "corekit_probe_palette") != 0 ||
          options->definitions[1].category_key == NULL ||
          strcmp(options->definitions[1].category_key, "video") != 0 ||
          options->definitions[1].values[0].value == NULL ||
          strcmp(options->definitions[1].values[0].value, "color") != 0 ||
          options->definitions[1].values[1].value == NULL ||
          strcmp(options->definitions[1].values[1].value, "monochrome") != 0 ||
          options->definitions[1].values[2].value != NULL ||
          options->definitions[1].default_value == NULL ||
          strcmp(options->definitions[1].default_value, "color") != 0 ||
          options->definitions[2].key != NULL)
         observations.callback_error = true;
      observations.core_options_v2_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_LOG_INTERFACE)
   {
      struct retro_log_callback *logger = data;
      if (logger == NULL)
         observations.callback_error = true;
      else
         logger->log = log_callback;
      observations.log_interface_calls++;
#if defined(__linux__)
      return observations.support_optional_interfaces;
#else
      return false;
#endif
   }

   if (command == RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS)
   {
      const struct retro_input_descriptor *descriptors = data;
      if (descriptors == NULL || descriptors[0].description == NULL ||
          descriptors[0].id != RETRO_DEVICE_ID_JOYPAD_LEFT ||
          strcmp(descriptors[0].description, "Move left") != 0 ||
          descriptors[1].description == NULL ||
          descriptors[1].id != RETRO_DEVICE_ID_JOYPAD_RIGHT ||
          descriptors[2].description == NULL ||
          descriptors[2].id != RETRO_DEVICE_ID_JOYPAD_A ||
          descriptors[3].description != NULL)
         observations.callback_error = true;
      observations.input_descriptors = observations.support_optional_interfaces
         ? descriptors
         : NULL;
      observations.input_descriptor_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_SET_CONTROLLER_INFO)
   {
      const struct retro_controller_info *controllers = data;
      if (controllers == NULL || controllers[0].types == NULL ||
          controllers[0].num_types != 1 ||
          controllers[0].types[0].desc == NULL ||
          strcmp(controllers[0].types[0].desc, "RetroPad") != 0 ||
          controllers[0].types[0].id != RETRO_DEVICE_JOYPAD ||
          controllers[1].types != NULL || controllers[1].num_types != 0)
         observations.callback_error = true;
      observations.controller_info = observations.support_optional_interfaces
         ? controllers
         : NULL;
      observations.controller_info_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_INPUT_BITMASKS)
   {
      if (data != NULL)
         observations.callback_error = true;
      observations.input_bitmask_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY)
   {
      if (data == NULL)
         observations.callback_error = true;
      else if (observations.support_optional_interfaces)
         *(const char **)data = system_directory;
      observations.system_directory_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY)
   {
      if (data == NULL)
         observations.callback_error = true;
      else if (observations.support_optional_interfaces)
         *(const char **)data = save_directory;
      observations.save_directory_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY)
   {
      if (data == NULL)
         observations.callback_error = true;
      else if (observations.support_optional_interfaces)
         *(const char **)data = core_assets_directory;
      observations.core_assets_directory_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_LANGUAGE)
   {
      if (data == NULL)
         observations.callback_error = true;
      else if (observations.support_optional_interfaces)
         *(enum retro_language *)data = RETRO_LANGUAGE_THAI;
      observations.language_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION)
   {
      if (data == NULL)
         observations.callback_error = true;
      else
         *(unsigned *)data = 1;
      observations.message_version_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_SET_MESSAGE_EXT)
   {
      const struct retro_message_ext *message = data;
      if (message == NULL || message->msg == NULL ||
          strcmp(message->msg, "CoreKit probe ready") != 0 ||
          message->duration != 3000 || message->priority != 1 ||
          message->level != RETRO_LOG_INFO ||
          message->target != RETRO_MESSAGE_TARGET_ALL ||
          message->type != RETRO_MESSAGE_TYPE_NOTIFICATION ||
          message->progress != -1)
         observations.callback_error = true;
      observations.message_extended_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_SET_MESSAGE)
   {
      const struct retro_message *message = data;
      if (message == NULL || message->msg == NULL ||
          strcmp(message->msg, "CoreKit probe ready") != 0 || message->frames != 180)
         observations.callback_error = true;
      observations.message_calls++;
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
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_VARIABLE)
   {
      struct retro_variable *variable = data;
      if (variable == NULL || variable->key == NULL)
         observations.callback_error = true;
      else if (strcmp(variable->key, "corekit_probe_tone") == 0)
         variable->value = observations.tone_option_enabled ? tone_on : tone_off;
      else if (strcmp(variable->key, "corekit_probe_palette") == 0)
         variable->value = observations.monochrome_option_enabled
            ? palette_monochrome
            : palette_color;
      else
         observations.callback_error = true;
      observations.variable_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE)
   {
      if (data == NULL)
         observations.callback_error = true;
      else
         *(int *)data = observations.support_optional_interfaces &&
               !observations.suppress_audio_video
            ? RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_AUDIO
            : 0;
      observations.audio_video_enable_calls++;
      return observations.support_optional_interfaces;
   }

   if (command == RETRO_ENVIRONMENT_GET_FASTFORWARDING)
   {
      if (data == NULL)
         observations.callback_error = true;
      else
         *(bool *)data = true;
      observations.fast_forwarding_calls++;
      return observations.support_optional_interfaces;
   }

   return false;
}

static void RETRO_CALLCONV video_callback(
      const void *data, unsigned width, unsigned height, size_t pitch)
{
   if (data == NULL || width != 160 || height != 144 || pitch != 160 * sizeof(uint32_t))
   {
      observations.callback_error = true;
   }
   else
   {
      const uint32_t *pixels = data;
      size_t pixel_count = (pitch / sizeof(*pixels)) * height;
      size_t pixel_index;
      uint64_t hash = hash_bytes(data, pitch * height);
      if (observations.video_calls == 0)
         observations.first_video_hash = hash;
      else if (observations.video_calls == 1)
         observations.second_video_hash = hash;
      else if (observations.video_calls == 4)
         observations.no_input_reset_video_hash = hash;
      observations.last_video_hash = hash;
      observations.last_video_monochrome = true;
      for (pixel_index = 0; pixel_index < pixel_count; pixel_index++)
      {
         uint32_t pixel = pixels[pixel_index];
         uint32_t red = (pixel >> 16) & UINT32_C(0xFF);
         uint32_t green = (pixel >> 8) & UINT32_C(0xFF);
         uint32_t blue = pixel & UINT32_C(0xFF);
         if (red != green || green != blue)
         {
            observations.last_video_monochrome = false;
            break;
         }
      }
   }
   observations.video_calls++;
}

static void RETRO_CALLCONV audio_sample_callback(int16_t left, int16_t right)
{
   if (left != right)
      observations.callback_error = true;
   if (left != 0)
      observations.saw_nonzero_audio = true;
   observations.audio_sample_calls++;
}

static size_t RETRO_CALLCONV audio_batch_callback(const int16_t *data, size_t frames)
{
   size_t index;

   if (data == NULL || frames != 800)
   {
      observations.callback_error = true;
   }
   else
   {
      uint64_t hash = hash_bytes(data, frames * 2 * sizeof(*data));
      if (observations.audio_batch_calls == 0)
         observations.first_audio_hash = hash;
      else if (observations.audio_batch_calls == 1)
         observations.second_audio_hash = hash;
      else if (observations.audio_batch_calls == 4)
         observations.no_input_reset_audio_hash = hash;
      observations.last_audio_hash = hash;
   }

   observations.last_audio_silent = true;
   for (index = 0; data != NULL && index < frames; index++)
   {
      if (data[index * 2] != data[(index * 2) + 1])
         observations.callback_error = true;
      if (data[index * 2] != 0)
      {
         observations.saw_nonzero_audio = true;
         observations.last_audio_silent = false;
      }
   }

   observations.audio_batch_calls++;
   observations.audio_frames += frames;
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
      return 0;

   observations.input_state_calls++;
   if (!observations.provide_input)
      return 0;

   if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
   {
      observations.right_press_reports++;
      observations.a_press_reports++;
      return (int16_t)((1U << RETRO_DEVICE_ID_JOYPAD_RIGHT) |
                       (1U << RETRO_DEVICE_ID_JOYPAD_A));
   }

   if (id == RETRO_DEVICE_ID_JOYPAD_RIGHT)
   {
      observations.right_press_reports++;
      return 1;
   }
   if (id == RETRO_DEVICE_ID_JOYPAD_A)
   {
      observations.a_press_reports++;
      return 1;
   }
   return 0;
}

static core_handle open_core(const char *path)
{
#if defined(_WIN32)
   return LoadLibraryA(path);
#else
   return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void *find_symbol(core_handle handle, const char *name)
{
#if defined(_WIN32)
   return (void *)(uintptr_t)GetProcAddress(handle, name);
#else
   return dlsym(handle, name);
#endif
}

static bool close_core(core_handle handle)
{
#if defined(_WIN32)
   return FreeLibrary(handle) != 0;
#else
   return dlclose(handle) == 0;
#endif
}

static void print_loader_error(const char *operation)
{
#if defined(_WIN32)
   fprintf(stderr, "%s failed with Windows error %lu\n", operation, (unsigned long)GetLastError());
#else
   const char *message = dlerror();
   fprintf(stderr, "%s failed: %s\n", operation, message != NULL ? message : "unknown error");
#endif
}

static bool assign_symbol(
      core_handle handle, const char *name, void *destination, size_t destination_size)
{
   void *symbol = find_symbol(handle, name);
   if (symbol == NULL)
   {
      fprintf(stderr, "missing required export: %s\n", name);
      return false;
   }

   if (destination_size != sizeof(symbol))
   {
      fprintf(stderr, "function pointer size mismatch for: %s\n", name);
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

static bool load_api(core_handle handle, struct core_api *api)
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

static bool check(bool condition, const char *message)
{
   if (!condition)
      fprintf(stderr, "validation failed: %s\n", message);
   return condition;
}

static bool validate_abi_layout(void)
{
   bool valid =
      sizeof(void *) == 8 &&
      sizeof(bool) == 1 &&
      sizeof(struct retro_system_info) == 32 &&
      _Alignof(struct retro_system_info) == 8 &&
      offsetof(struct retro_system_info, library_name) == 0 &&
      offsetof(struct retro_system_info, library_version) == 8 &&
      offsetof(struct retro_system_info, valid_extensions) == 16 &&
      offsetof(struct retro_system_info, need_fullpath) == 24 &&
      offsetof(struct retro_system_info, block_extract) == 25 &&
      sizeof(struct retro_game_geometry) == 20 &&
      _Alignof(struct retro_game_geometry) == 4 &&
      offsetof(struct retro_game_geometry, base_width) == 0 &&
      offsetof(struct retro_game_geometry, base_height) == 4 &&
      offsetof(struct retro_game_geometry, max_width) == 8 &&
      offsetof(struct retro_game_geometry, max_height) == 12 &&
      offsetof(struct retro_game_geometry, aspect_ratio) == 16 &&
      sizeof(struct retro_system_timing) == 16 &&
      _Alignof(struct retro_system_timing) == 8 &&
      offsetof(struct retro_system_timing, fps) == 0 &&
      offsetof(struct retro_system_timing, sample_rate) == 8 &&
      sizeof(struct retro_system_av_info) == 40 &&
      _Alignof(struct retro_system_av_info) == 8 &&
      offsetof(struct retro_system_av_info, geometry) == 0 &&
      offsetof(struct retro_system_av_info, timing) == 24 &&
      sizeof(struct retro_game_info) == 32 &&
      _Alignof(struct retro_game_info) == 8 &&
      offsetof(struct retro_game_info, path) == 0 &&
      offsetof(struct retro_game_info, data) == 8 &&
      offsetof(struct retro_game_info, size) == 16 &&
      offsetof(struct retro_game_info, meta) == 24 &&
      sizeof(struct retro_input_descriptor) == 24 &&
      _Alignof(struct retro_input_descriptor) == 8 &&
      offsetof(struct retro_input_descriptor, port) == 0 &&
      offsetof(struct retro_input_descriptor, device) == 4 &&
      offsetof(struct retro_input_descriptor, index) == 8 &&
      offsetof(struct retro_input_descriptor, id) == 12 &&
      offsetof(struct retro_input_descriptor, description) == 16 &&
      sizeof(struct retro_controller_description) == 16 &&
      offsetof(struct retro_controller_description, desc) == 0 &&
      offsetof(struct retro_controller_description, id) == 8 &&
      sizeof(struct retro_controller_info) == 16 &&
      offsetof(struct retro_controller_info, types) == 0 &&
      offsetof(struct retro_controller_info, num_types) == 8 &&
      sizeof(struct retro_variable) == 16 &&
      offsetof(struct retro_variable, key) == 0 &&
      offsetof(struct retro_variable, value) == 8 &&
      sizeof(struct retro_message) == 16 &&
      offsetof(struct retro_message, msg) == 0 &&
      offsetof(struct retro_message, frames) == 8 &&
      sizeof(struct retro_message_ext) == 32 &&
      offsetof(struct retro_message_ext, msg) == 0 &&
      offsetof(struct retro_message_ext, duration) == 8 &&
      offsetof(struct retro_message_ext, priority) == 12 &&
      offsetof(struct retro_message_ext, level) == 16 &&
      offsetof(struct retro_message_ext, target) == 20 &&
      offsetof(struct retro_message_ext, type) == 24 &&
      offsetof(struct retro_message_ext, progress) == 28 &&
      sizeof(struct retro_log_callback) == 8 &&
      offsetof(struct retro_log_callback, log) == 0 &&
      sizeof(struct retro_core_option_value) == 16 &&
      sizeof(struct retro_core_option_v2_category) == 24 &&
      offsetof(struct retro_core_option_v2_category, key) == 0 &&
      offsetof(struct retro_core_option_v2_category, desc) == 8 &&
      offsetof(struct retro_core_option_v2_category, info) == 16 &&
      sizeof(struct retro_core_option_v2_definition) == 2104 &&
      offsetof(struct retro_core_option_v2_definition, key) == 0 &&
      offsetof(struct retro_core_option_v2_definition, desc) == 8 &&
      offsetof(struct retro_core_option_v2_definition, desc_categorized) == 16 &&
      offsetof(struct retro_core_option_v2_definition, info) == 24 &&
      offsetof(struct retro_core_option_v2_definition, info_categorized) == 32 &&
      offsetof(struct retro_core_option_v2_definition, category_key) == 40 &&
      offsetof(struct retro_core_option_v2_definition, values) == 48 &&
      offsetof(struct retro_core_option_v2_definition, default_value) == 2096 &&
      sizeof(struct retro_core_options_v2) == 16 &&
      offsetof(struct retro_core_options_v2, categories) == 0 &&
      offsetof(struct retro_core_options_v2, definitions) == 8 &&
      sizeof(enum retro_language) == 4 &&
      sizeof(enum retro_av_enable_flags) == 4 &&
      sizeof(enum retro_log_level) == 4 &&
      sizeof(enum retro_message_target) == 4 &&
      sizeof(enum retro_message_type) == 4;

   printf("ABI: pointer=%zu, bool=%zu, system_info=%zu/%zu, geometry=%zu/%zu, "
          "timing=%zu/%zu, av_info=%zu/%zu, game_info=%zu/%zu, "
          "input_descriptor=%zu/%zu, message_ext=%zu/%zu, option_v2=%zu/%zu\n",
          sizeof(void *), sizeof(bool),
          sizeof(struct retro_system_info), _Alignof(struct retro_system_info),
          sizeof(struct retro_game_geometry), _Alignof(struct retro_game_geometry),
          sizeof(struct retro_system_timing), _Alignof(struct retro_system_timing),
          sizeof(struct retro_system_av_info), _Alignof(struct retro_system_av_info),
          sizeof(struct retro_game_info), _Alignof(struct retro_game_info),
          sizeof(struct retro_input_descriptor), _Alignof(struct retro_input_descriptor),
          sizeof(struct retro_message_ext), _Alignof(struct retro_message_ext),
          sizeof(struct retro_core_option_v2_definition),
          _Alignof(struct retro_core_option_v2_definition));
   return check(valid, "C ABI layouts");
}

static bool validate_retained_input_descriptors(void)
{
   const struct retro_input_descriptor *descriptors = observations.input_descriptors;
   return descriptors != NULL && descriptors[0].description != NULL &&
          strcmp(descriptors[0].description, "Move left") == 0 &&
          descriptors[1].description != NULL &&
          strcmp(descriptors[1].description, "Move right") == 0 &&
          descriptors[2].description != NULL &&
          strcmp(descriptors[2].description, "Increase tone") == 0 &&
          descriptors[3].description == NULL;
}

static bool validate_retained_controller_info(void)
{
   const struct retro_controller_info *controllers = observations.controller_info;
   return controllers != NULL && controllers[0].types != NULL &&
          controllers[0].num_types == 1 &&
          controllers[0].types[0].desc != NULL &&
          strcmp(controllers[0].types[0].desc, "RetroPad") == 0 &&
          controllers[0].types[0].id == RETRO_DEVICE_JOYPAD &&
          controllers[1].types == NULL;
}

static bool validate_system_info(const struct retro_system_info *info)
{
   return info->library_name != NULL &&
          strcmp(info->library_name, "CoreKit NativeAOT Probe") == 0 &&
          info->library_version != NULL &&
          strcmp(info->library_version, "0.1.0-phase3") == 0 &&
          info->valid_extensions != NULL &&
          strcmp(info->valid_extensions, "") == 0 &&
          !info->need_fullpath &&
          !info->block_extract;
}

static bool validate_late_callback_registration(
      const struct core_api *api, bool support_optional_interfaces)
{
   reset_observations();
   observations.support_optional_interfaces = support_optional_interfaces;
   observations.option_update_pending = true;

   api->retro_set_environment(environment_callback);
   api->retro_init();
   if (!check(api->retro_load_game(NULL), "load before frame callback registration"))
      return false;

   api->retro_run();
   if (!check(observations.video_calls == 0, "missing callbacks produce no video") ||
       !check(observations.audio_batch_calls == 0, "missing callbacks produce no audio") ||
       !check(observations.input_poll_calls == 0, "missing callbacks produce no input poll"))
      return false;

   api->retro_set_video_refresh(video_callback);
   api->retro_set_audio_sample(audio_sample_callback);
   api->retro_set_audio_sample_batch(audio_batch_callback);
   api->retro_set_input_poll(input_poll_callback);
   api->retro_set_input_state(input_state_callback);
   api->retro_run();
   api->retro_reset();
   api->retro_run();
   api->retro_unload_game();
   api->retro_deinit();
   if (!check(!observations.callback_error, "late callback registration arguments") ||
       !check(observations.video_calls == 2, "video after late callback registration") ||
       !check(observations.audio_batch_calls == 2, "audio after late callback registration") ||
       !check(observations.input_poll_calls == 2, "input after late callback registration") ||
       !check(observations.controller_info_calls == 1,
              "controller info during preflight content load") ||
       !check(observations.first_video_hash == observations.second_video_hash,
              "missing callbacks do not advance video state") ||
       !check(observations.first_audio_hash == observations.second_audio_hash,
              "missing callbacks do not advance audio state") ||
       !check(observations.log_interface_calls == 1, "logging negotiation during preflight"))
      return false;

#if defined(__linux__)
   if (!check(observations.log_calls ==
                (support_optional_interfaces ? 5U : 0U),
              "logging during late callback registration"))
      return false;
#else
   if (!check(observations.log_calls == 0, "logging remains disabled outside Linux"))
      return false;
#endif

   return true;
}

static bool validate_state_and_memory(const struct core_api *api)
{
   const size_t expected_state_size = 88;
   const size_t expected_save_ram_size = 64;
   uint8_t *state;
   uint8_t *save_ram;
   void *save_ram_pointer;
   uint64_t expected_video_hash;
   uint64_t expected_audio_hash;
   bool valid = false;

   if (!check(api->retro_serialize_size() == expected_state_size,
              "serialized state size"))
      return false;

   save_ram_pointer = api->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
   save_ram = (uint8_t *)save_ram_pointer;
   if (!check(save_ram != NULL, "save RAM data") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) ==
                expected_save_ram_size,
              "save RAM size") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_RTC) == NULL,
              "unsupported RTC data") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_RTC) == 0,
              "unsupported RTC size") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == NULL,
              "unsupported system RAM data") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) == 0,
              "unsupported system RAM size") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_VIDEO_RAM) == NULL,
              "unsupported video RAM data") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_VIDEO_RAM) == 0,
              "unsupported video RAM size") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_ROM) == NULL,
              "unsupported ROM data") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_ROM) == 0,
              "unsupported ROM size"))
      return false;

   save_ram[7] = 0xA5;
   api->retro_reset();
   if (!check(api->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM) ==
                save_ram_pointer,
              "save RAM pointer survives reset") ||
       !check(save_ram[7] == 0xA5, "save RAM survives reset"))
      return false;

   state = (uint8_t *)malloc(expected_state_size);
   if (!check(state != NULL, "state buffer allocation"))
      return false;

   if (!check(!api->retro_serialize(NULL, expected_state_size),
              "serialize rejects null data") ||
       !check(!api->retro_serialize(state, expected_state_size - 1),
              "serialize rejects short buffer") ||
       !check(api->retro_serialize(state, expected_state_size),
              "serialize state") ||
       !check(!api->retro_unserialize(NULL, expected_state_size),
              "unserialize rejects null data") ||
       !check(!api->retro_unserialize(state, expected_state_size - 1),
              "unserialize rejects short buffer"))
      goto cleanup;

   state[0] ^= 0xFF;
   if (!check(!api->retro_unserialize(state, expected_state_size),
              "unserialize rejects invalid state") ||
       !check(save_ram[7] == 0xA5,
              "invalid state does not mutate save RAM"))
      goto cleanup;
   state[0] ^= 0xFF;

   api->retro_run();
   expected_video_hash = observations.last_video_hash;
   expected_audio_hash = observations.last_audio_hash;
   save_ram[7] = 0x5A;

   if (!check(api->retro_unserialize(state, expected_state_size),
              "unserialize state") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM) ==
                save_ram_pointer,
              "save RAM pointer survives state load") ||
       !check(save_ram[7] == 0xA5, "state restores save RAM"))
      goto cleanup;

   api->retro_run();
   if (!check(observations.last_video_hash == expected_video_hash,
              "state restores deterministic video") ||
       !check(observations.last_audio_hash == expected_audio_hash,
              "state restores deterministic audio"))
      goto cleanup;

   valid = true;

cleanup:
   free(state);
   return valid;
}

static bool run_session(const struct core_api *api, bool support_optional_interfaces)
{
   struct retro_system_info system_info;
   struct retro_system_info system_info_after_deinit;
   struct retro_system_av_info av_info;
   struct retro_game_info invalid_game_info;
   const char *library_name;
   const char *library_version;
   const char *valid_extensions;
   unsigned input_poll_before_device_change;
   unsigned input_state_before_device_change;
   unsigned video_before_device_change;
   unsigned audio_before_device_change;
   unsigned video_calls_before_unloaded_run;
   unsigned frame;

   if (!validate_late_callback_registration(api, support_optional_interfaces))
      return false;

   reset_observations();
   observations.support_optional_interfaces = support_optional_interfaces;
   observations.option_update_pending = true;
   memset(&system_info, 0, sizeof(system_info));
   api->retro_get_system_info(NULL);
   api->retro_get_system_av_info(NULL);
   api->retro_get_system_info(&system_info);

   if (!check(api->retro_api_version() == RETRO_API_VERSION, "API version") ||
       !check(validate_system_info(&system_info), "system metadata before initialization"))
      return false;
   library_name = system_info.library_name;
   library_version = system_info.library_version;
   valid_extensions = system_info.valid_extensions;

   api->retro_set_controller_port_device(0, RETRO_DEVICE_NONE);

   api->retro_set_environment(environment_callback);
   api->retro_set_video_refresh(video_callback);
   api->retro_set_audio_sample(audio_sample_callback);
   api->retro_set_audio_sample_batch(audio_batch_callback);
   api->retro_set_input_poll(input_poll_callback);
   api->retro_set_input_state(input_state_callback);

   if (!check(observations.support_no_game_calls == 1, "support-no-game negotiation") ||
       !check(observations.core_options_v2_calls == 1, "core-options-v2 registration") ||
       !check(observations.log_interface_calls == 1, "logging negotiation"))
      return false;

   memset(&av_info, 0xFF, sizeof(av_info));
   api->retro_get_system_av_info(&av_info);
   if (!check(av_info.geometry.base_width == 0, "AV info before load is zeroed") ||
       !check(!api->retro_load_game(NULL), "load before initialization is rejected"))
      return false;
   api->retro_run();
   if (!check(observations.video_calls == 0, "run before initialization is a no-op"))
      return false;

   api->retro_init();
   api->retro_init();
   if (!check(observations.input_descriptor_calls == 1, "input descriptors") ||
       !check(observations.input_bitmask_calls == 1, "input-bitmask negotiation") ||
       !check(observations.system_directory_calls == 1, "system directory query") ||
       !check(observations.save_directory_calls == 1, "save directory query") ||
       !check(observations.core_assets_directory_calls == 2,
              "content/core-assets directory alias queries") ||
       !check(observations.language_calls == 1, "language query") ||
       !check(observations.message_version_calls == 1, "message interface query"))
      return false;
   observations.reject_pixel_format = true;
   if (!check(!api->retro_load_game(NULL), "rejected XRGB8888 negotiation"))
      return false;
   observations.reject_pixel_format = false;

   memset(&invalid_game_info, 0, sizeof(invalid_game_info));
   invalid_game_info.size = 1;
   if (!check(!api->retro_load_game(&invalid_game_info),
              "content with a size but no data is rejected"))
      return false;

   if (!check(api->retro_load_game(NULL), "contentless load after environment rejection") ||
       !check(!api->retro_load_game(NULL), "duplicate content load is rejected") ||
       !check(observations.pixel_format_calls == 2, "XRGB8888 negotiation and retry") ||
       !check(observations.message_extended_calls ==
                 (support_optional_interfaces ? 1U : 0U),
              "extended message path") ||
       !check(observations.message_calls ==
                 (support_optional_interfaces ? 0U : 1U),
              "legacy message fallback") ||
       !check(observations.controller_info_calls == 1,
              "controller info registration"))
      return false;

   memset(&av_info, 0, sizeof(av_info));
   api->retro_get_system_av_info(&av_info);
   if (!check(av_info.geometry.base_width == 160, "base width") ||
       !check(av_info.geometry.base_height == 144, "base height") ||
       !check(av_info.geometry.max_width == 160, "maximum width") ||
       !check(av_info.geometry.max_height == 144, "maximum height") ||
       !check(av_info.timing.fps == 60.0, "frame rate") ||
       !check(av_info.timing.sample_rate == 48000.0, "sample rate"))
      return false;

   observations.provide_input = true;
   api->retro_run();
   observations.provide_input = false;
   for (frame = 1; frame < 4; frame++)
      api->retro_run();
   api->retro_reset();
   api->retro_run();
   api->retro_reset();
   observations.provide_input = true;
   api->retro_run();

   if (!check(!observations.callback_error, "callback arguments") ||
       !check(observations.video_calls == 6, "video callback count") ||
       !check(observations.audio_batch_calls == 6, "audio batch callback count") ||
       !check(observations.audio_sample_calls == 0, "batch audio preference") ||
       !check(observations.audio_frames == 6 * 800, "audio frame count") ||
       !check(observations.saw_nonzero_audio, "generated tone") ||
       !check(observations.input_poll_calls == 6, "input polling count") ||
       !check(observations.input_state_calls ==
                 (support_optional_interfaces ? 6U : 96U),
              "bitmask or single-button input query count") ||
       !check(observations.right_press_reports == 2, "direction input reports") ||
       !check(observations.a_press_reports == 2, "button input reports") ||
       !check(observations.first_video_hash != 0, "first video hash") ||
       !check(observations.second_video_hash != observations.first_video_hash,
              "moving video output") ||
       !check(observations.no_input_reset_video_hash != observations.first_video_hash,
              "input-sensitive video output") ||
       !check(observations.last_video_hash == observations.first_video_hash,
              "video reset determinism") ||
       !check(observations.first_audio_hash != 0, "first audio hash") ||
       !check(observations.second_audio_hash != observations.first_audio_hash,
              "moving audio output") ||
       !check(observations.no_input_reset_audio_hash != observations.first_audio_hash,
              "input-sensitive audio output") ||
       !check(observations.last_audio_hash == observations.first_audio_hash,
              "audio reset determinism") ||
       !check(observations.variable_update_calls == 6, "option-update polling") ||
       !check(observations.variable_calls ==
                 (support_optional_interfaces ? 4U : 2U),
              "initial and updated option queries") ||
       !check(observations.audio_video_enable_calls == 6, "audio/video enable queries") ||
       !check(observations.fast_forwarding_calls == 6, "fast-forward queries") ||
       !check(support_optional_interfaces
                 ? validate_retained_input_descriptors()
                 : observations.input_descriptors == NULL,
              "input descriptor lifetime through loaded session") ||
       !check(support_optional_interfaces
                 ? validate_retained_controller_info()
                 : observations.controller_info == NULL,
              "controller info lifetime through loaded session"))
      return false;

   observations.provide_input = false;
   observations.tone_option_enabled = false;
   observations.monochrome_option_enabled = true;
   observations.option_update_pending = true;
   api->retro_run();
   if (!check(support_optional_interfaces
                 ? observations.last_audio_silent
                 : !observations.last_audio_silent,
              "tone option changes audio output") ||
       !check(observations.last_video_monochrome == support_optional_interfaces,
              "palette option changes video output") ||
       !check(observations.variable_calls ==
                 (support_optional_interfaces ? 6U : 2U),
              "runtime option queries"))
      return false;

   observations.tone_option_enabled = true;
   observations.monochrome_option_enabled = false;
   observations.option_update_pending = true;
   api->retro_run();
   if (!check(!observations.last_audio_silent, "tone option restores audio output") ||
       !check(!observations.last_video_monochrome,
              "palette option restores color output") ||
       !check(observations.variable_calls ==
                 (support_optional_interfaces ? 8U : 2U),
              "restored runtime option queries"))
      return false;

   input_poll_before_device_change = observations.input_poll_calls;
   input_state_before_device_change = observations.input_state_calls;
   video_before_device_change = observations.video_calls;
   audio_before_device_change = observations.audio_batch_calls;
   observations.provide_input = true;
   api->retro_set_controller_port_device(0, RETRO_DEVICE_NONE);
   api->retro_run();
   observations.provide_input = false;
   if (!check(observations.input_poll_calls == input_poll_before_device_change,
              "disabled controller suppresses RetroPad polling") ||
       !check(observations.input_state_calls == input_state_before_device_change,
              "disabled controller suppresses RetroPad state reads") ||
       !check(observations.video_calls == video_before_device_change + 1,
              "disabled controller still emits video") ||
       !check(observations.audio_batch_calls == audio_before_device_change + 1,
              "disabled controller still emits audio"))
      return false;

   api->retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
   api->retro_run();
   if (!check(observations.input_poll_calls == input_poll_before_device_change + 1,
              "restored controller resumes RetroPad polling") ||
       !check(observations.input_state_calls == input_state_before_device_change +
                (support_optional_interfaces ? 1U : 16U),
              "restored controller resumes RetroPad state reads"))
      return false;

   api->retro_set_audio_sample_batch(NULL);
   api->retro_run();
   if (!check(observations.video_calls == 11, "video during sample-audio fallback") ||
       !check(observations.audio_batch_calls == 10, "batch audio disabled for fallback") ||
       !check(observations.audio_sample_calls == 800, "sample-audio fallback"))
      return false;
   api->retro_set_audio_sample_batch(audio_batch_callback);

   if (support_optional_interfaces)
   {
      unsigned video_calls_before_suppression = observations.video_calls;
      unsigned audio_calls_before_suppression = observations.audio_batch_calls;
      observations.suppress_audio_video = true;
      api->retro_run();
      observations.suppress_audio_video = false;
      if (!check(observations.video_calls == video_calls_before_suppression,
                 "frontend video suppression") ||
          !check(observations.audio_batch_calls == audio_calls_before_suppression,
                 "frontend audio suppression"))
         return false;
   }

   if (!validate_state_and_memory(api) ||
       !check(!api->retro_load_game_special(0, NULL, 0), "unsupported special load") ||
       !check(api->retro_get_region() == RETRO_REGION_NTSC, "region"))
      return false;

   api->retro_cheat_reset();
   api->retro_cheat_set(0, false, NULL);
   api->retro_unload_game();

   if (!check(api->retro_serialize_size() == 0,
              "serialized state unavailable after unload") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SAVE_RAM) == NULL,
              "save RAM data unavailable after unload") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) == 0,
              "save RAM size unavailable after unload"))
      return false;

   video_calls_before_unloaded_run = observations.video_calls;
   api->retro_run();
   if (!check(observations.video_calls == video_calls_before_unloaded_run, "run after unload is a no-op"))
      return false;

   api->retro_deinit();
   api->retro_deinit();
   api->retro_run();
   api->retro_set_controller_port_device(0, RETRO_DEVICE_NONE);
   memset(&system_info_after_deinit, 0, sizeof(system_info_after_deinit));
   api->retro_get_system_info(&system_info_after_deinit);
   if (!check(!api->retro_load_game(NULL), "load after deinitialization is rejected") ||
       !check(validate_system_info(&system_info_after_deinit),
              "system metadata after logical teardown") ||
       !check(system_info_after_deinit.library_name == library_name,
              "library name process-lifetime pointer") ||
       !check(system_info_after_deinit.library_version == library_version,
              "library version process-lifetime pointer") ||
       !check(system_info_after_deinit.valid_extensions == valid_extensions,
              "valid extensions process-lifetime pointer"))
      return false;

#if defined(__linux__)
   if (!check(observations.log_calls ==
                (support_optional_interfaces ? 11U : 0U),
              "audited logging bridge lifecycle"))
      return false;
#else
   if (!check(observations.log_calls == 0, "logging remains disabled outside Linux"))
      return false;
#endif

   return true;
}

static size_t resident_bytes(void)
{
#if defined(_WIN32)
   PROCESS_MEMORY_COUNTERS counters;
   memset(&counters, 0, sizeof(counters));
   counters.cb = sizeof(counters);
   if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
      return 0;
   return (size_t)counters.WorkingSetSize;
#elif defined(__linux__)
   FILE *status = fopen("/proc/self/statm", "r");
   unsigned long total_pages;
   unsigned long resident_pages;
   long page_size;

   if (status == NULL)
      return 0;
   if (fscanf(status, "%lu %lu", &total_pages, &resident_pages) != 2)
   {
      fclose(status);
      return 0;
   }
   fclose(status);
   (void)total_pages;

   page_size = sysconf(_SC_PAGESIZE);
   if (page_size <= 0)
      return 0;
   return (size_t)resident_pages * (size_t)page_size;
#elif defined(__APPLE__)
   mach_task_basic_info_data_t info;
   mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
   kern_return_t result = task_info(
         mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count);
   if (result != KERN_SUCCESS)
      return 0;
   return (size_t)info.resident_size;
#else
   return 0;
#endif
}

static bool parse_iterations(const char *text, unsigned *iterations)
{
   char *end;
   unsigned long parsed;

   errno = 0;
   parsed = strtoul(text, &end, 10);
   if (errno != 0 || *text == '\0' || *end != '\0' || parsed == 0 || parsed > 10000)
      return false;

   *iterations = (unsigned)parsed;
   return true;
}

static bool parse_rss_limit(const char *text, double *limit_mib)
{
   char *end;
   double parsed;

   errno = 0;
   parsed = strtod(text, &end);
   if (errno != 0 || *text == '\0' || *end != '\0' || parsed != parsed ||
       parsed < 0.0 || parsed > 1000000.0)
      return false;

   *limit_mib = parsed;
   return true;
}

static bool print_result(
      FILE *stream,
      unsigned iterations,
      unsigned sessions_per_load,
      size_t rss_after_first_session,
      size_t rss_at_end)
{
   long long growth = (long long)rss_at_end - (long long)rss_after_first_session;
   return fprintf(
      stream,
      "{\"result\":\"pass\",\"pointer_bits\":%zu,\"load_cycles\":%u,"
      "\"sessions_per_load\":%u,\"managed_sessions\":%u,"
      "\"rss_after_first_session_bytes\":%llu,\"rss_final_bytes\":%llu,"
      "\"rss_growth_bytes\":%lld,\"video_first_hash\":\"%016llx\","
      "\"audio_first_hash\":\"%016llx\"}\n",
      sizeof(void *) * 8,
      iterations,
      sessions_per_load,
      iterations * sessions_per_load,
      (unsigned long long)rss_after_first_session,
      (unsigned long long)rss_at_end,
      growth,
      (unsigned long long)observations.first_video_hash,
      (unsigned long long)observations.first_audio_hash) >= 0;
}

static bool write_result(
      const char *path,
      unsigned iterations,
      unsigned sessions_per_load,
      size_t rss_after_first_session,
      size_t rss_at_end)
{
   FILE *stream;
   bool written;

#if defined(_WIN32)
   if (fopen_s(&stream, path, "w") != 0)
      stream = NULL;
#else
   stream = fopen(path, "w");
#endif
   if (stream == NULL)
   {
      fprintf(stderr, "could not write result file: %s\n", path);
      return false;
   }

   written = print_result(
      stream, iterations, sessions_per_load, rss_after_first_session, rss_at_end);
   if (fclose(stream) != 0)
      written = false;
   return written;
}

int main(int argc, char **argv)
{
   const char *core_path;
   const char *result_path = NULL;
   unsigned iterations = 25;
   unsigned sessions_per_load = 2;
   double max_rss_growth_mib = -1.0;
   unsigned iteration;
   size_t rss_after_first_session = 0;
   size_t rss_at_end;

   if (argc < 2 || argc > 6)
   {
      fprintf(stderr,
              "usage: %s CORE_PATH [LOAD_CYCLES] [SESSIONS_PER_LOAD] "
              "[MAX_RSS_GROWTH_MIB] [RESULT_PATH]\n",
              argv[0]);
      return EXIT_FAILURE;
   }

   core_path = argv[1];
   if (argc >= 3 && !parse_iterations(argv[2], &iterations))
   {
      fprintf(stderr, "invalid iteration count: %s\n", argv[2]);
      return EXIT_FAILURE;
   }
   if (argc >= 4 && !parse_iterations(argv[3], &sessions_per_load))
   {
      fprintf(stderr, "invalid sessions-per-load count: %s\n", argv[3]);
      return EXIT_FAILURE;
   }
   if (argc >= 5 && !parse_rss_limit(argv[4], &max_rss_growth_mib))
   {
      fprintf(stderr, "invalid maximum RSS growth: %s\n", argv[4]);
      return EXIT_FAILURE;
   }
   if (argc == 6)
      result_path = argv[5];

   if (!validate_abi_layout())
      return EXIT_FAILURE;

   for (iteration = 0; iteration < iterations; iteration++)
   {
      core_handle handle = open_core(core_path);
      struct core_api api;
      unsigned session;

      if (handle == NULL)
      {
         print_loader_error("opening core");
         return EXIT_FAILURE;
      }

      if (!load_api(handle, &api))
      {
         (void)close_core(handle);
         fprintf(stderr, "export loading failed on iteration %u\n", iteration + 1);
         return EXIT_FAILURE;
      }

      for (session = 0; session < sessions_per_load; session++)
      {
         if (!run_session(&api, (session % 2U) == 0))
         {
            (void)close_core(handle);
            fprintf(stderr, "lifecycle iteration %u, session %u failed\n",
                    iteration + 1, session + 1);
            return EXIT_FAILURE;
         }

         if (iteration == 0 && session == 0)
            rss_after_first_session = resident_bytes();
      }

      if (!close_core(handle))
      {
         print_loader_error("closing core");
         return EXIT_FAILURE;
      }

   }

   rss_at_end = resident_bytes();
   printf("PASS: %u load/unload cycles, %u managed sessions\n",
          iterations, iterations * sessions_per_load);
   if (rss_after_first_session != 0 && rss_at_end != 0)
   {
      long long growth = (long long)rss_at_end - (long long)rss_after_first_session;
      printf("RSS after first session: %.2f MiB; final: %.2f MiB; growth: %.2f MiB\n",
             (double)rss_after_first_session / (1024.0 * 1024.0),
             (double)rss_at_end / (1024.0 * 1024.0),
             (double)growth / (1024.0 * 1024.0));

      if (max_rss_growth_mib >= 0.0 &&
          (double)growth > max_rss_growth_mib * 1024.0 * 1024.0)
      {
         fprintf(stderr, "RSS growth exceeded the %.2f MiB limit\n", max_rss_growth_mib);
         return EXIT_FAILURE;
      }
   }
   else if (max_rss_growth_mib >= 0.0)
   {
      fprintf(stderr, "RSS measurement is unavailable on this platform\n");
      return EXIT_FAILURE;
   }

   printf("RESULT: ");
   if (!print_result(stdout, iterations, sessions_per_load, rss_after_first_session, rss_at_end))
      return EXIT_FAILURE;
   if (result_path != NULL &&
       !write_result(result_path, iterations, sessions_per_load, rss_after_first_session, rss_at_end))
      return EXIT_FAILURE;
   return EXIT_SUCCESS;
}
