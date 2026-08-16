#include <errno.h>
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
   bool provide_input;
   bool saw_nonzero_audio;
   bool callback_error;
};

static struct observations observations;

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
}

static bool RETRO_CALLCONV environment_callback(unsigned command, void *data)
{
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
      return true;
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
      uint64_t hash = hash_bytes(data, pitch * height);
      if (observations.video_calls == 0)
         observations.first_video_hash = hash;
      else if (observations.video_calls == 1)
         observations.second_video_hash = hash;
      else if (observations.video_calls == 4)
         observations.no_input_reset_video_hash = hash;
      observations.last_video_hash = hash;
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
   size_t samples_to_check;

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

   samples_to_check = frames < 32 ? frames : 32;
   for (index = 0; data != NULL && index < samples_to_check; index++)
   {
      if (data[index * 2] != data[(index * 2) + 1])
         observations.callback_error = true;
      if (data[index * 2] != 0)
         observations.saw_nonzero_audio = true;
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
      offsetof(struct retro_game_info, meta) == 24;

   printf("ABI: pointer=%zu, bool=%zu, system_info=%zu/%zu, geometry=%zu/%zu, "
          "timing=%zu/%zu, av_info=%zu/%zu, game_info=%zu/%zu\n",
          sizeof(void *), sizeof(bool),
          sizeof(struct retro_system_info), _Alignof(struct retro_system_info),
          sizeof(struct retro_game_geometry), _Alignof(struct retro_game_geometry),
          sizeof(struct retro_system_timing), _Alignof(struct retro_system_timing),
          sizeof(struct retro_system_av_info), _Alignof(struct retro_system_av_info),
          sizeof(struct retro_game_info), _Alignof(struct retro_game_info));
   return check(valid, "C ABI layouts");
}

static bool run_session(const struct core_api *api)
{
   struct retro_system_info system_info;
   struct retro_system_av_info av_info;
   unsigned video_calls_before_unloaded_run;
   uint8_t scratch = 0;
   unsigned frame;

   reset_observations();
   memset(&system_info, 0, sizeof(system_info));
   api->retro_get_system_info(NULL);
   api->retro_get_system_av_info(NULL);
   api->retro_get_system_info(&system_info);

   if (!check(api->retro_api_version() == RETRO_API_VERSION, "API version") ||
       !check(system_info.library_name != NULL, "library name pointer") ||
       !check(strcmp(system_info.library_name, "CoreKit NativeAOT Probe") == 0, "library name") ||
       !check(system_info.library_version != NULL, "library version pointer") ||
       !check(system_info.valid_extensions != NULL, "valid extensions pointer") ||
       !check(strcmp(system_info.valid_extensions, "") == 0, "contentless extensions") ||
       !check(!system_info.need_fullpath, "need_fullpath") ||
       !check(!system_info.block_extract, "block_extract"))
      return false;

   api->retro_set_environment(environment_callback);
   api->retro_set_video_refresh(video_callback);
   api->retro_set_audio_sample(audio_sample_callback);
   api->retro_set_audio_sample_batch(audio_batch_callback);
   api->retro_set_input_poll(input_poll_callback);
   api->retro_set_input_state(input_state_callback);

   if (!check(observations.support_no_game_calls == 1, "support-no-game negotiation"))
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
   if (!check(api->retro_load_game(NULL), "contentless load") ||
       !check(observations.pixel_format_calls == 1, "XRGB8888 negotiation"))
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

   api->retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
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
       !check(observations.input_state_calls == 18, "input query count") ||
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
              "audio reset determinism"))
      return false;

   if (!check(api->retro_serialize_size() == 0, "unsupported serialize size") ||
       !check(!api->retro_serialize(&scratch, sizeof(scratch)), "unsupported serialize") ||
       !check(!api->retro_unserialize(&scratch, sizeof(scratch)), "unsupported unserialize") ||
       !check(!api->retro_load_game_special(0, NULL, 0), "unsupported special load") ||
       !check(api->retro_get_region() == RETRO_REGION_NTSC, "region") ||
       !check(api->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == NULL, "unsupported memory data") ||
       !check(api->retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) == 0, "unsupported memory size"))
      return false;

   api->retro_cheat_reset();
   api->retro_cheat_set(0, false, NULL);
   api->retro_unload_game();

   video_calls_before_unloaded_run = observations.video_calls;
   api->retro_run();
   if (!check(observations.video_calls == video_calls_before_unloaded_run, "run after unload is a no-op"))
      return false;

   api->retro_deinit();
   api->retro_deinit();
   api->retro_run();
   if (!check(!api->retro_load_game(NULL), "load after deinitialization is rejected"))
      return false;
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
         if (!run_session(&api))
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
