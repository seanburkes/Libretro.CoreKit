cmake_minimum_required(VERSION 3.20)

project(LibretroNativeHost LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(libretro_native_host native_host.c)
target_include_directories(
  libretro_native_host
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../eng/libretro"
)

if(MSVC)
  target_compile_options(libretro_native_host PRIVATE /W4 /WX)
else()
  target_compile_options(libretro_native_host PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_libraries(libretro_native_host PRIVATE ${CMAKE_DL_LIBS})
endif()
if(DEFINED CORE_PATH)
  if(NOT DEFINED CORE_CYCLES)
    set(CORE_CYCLES 25)
  endif()

  enable_testing()
  add_test(
    NAME phase_0_native_lifecycle
    COMMAND libretro_native_host "${CORE_PATH}" "${CORE_CYCLES}"
  )
endif()
