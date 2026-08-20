#include "libretro.h"

#if defined(__GNUC__) || defined(__clang__)
#define COREKIT_HIDDEN __attribute__((visibility("hidden")))
#else
#define COREKIT_HIDDEN
#endif

COREKIT_HIDDEN void libretro_core_log_message(
      retro_log_printf_t callback,
      enum retro_log_level level,
      const char *message)
{
   if (callback == NULL || message == NULL)
      return;

   callback(level, "%s", message);
}
