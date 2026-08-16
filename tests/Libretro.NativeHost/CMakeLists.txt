cmake_minimum_required(VERSION 3.20)

project(LibretroNativeHost LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(CORE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_executable(libretro_native_host native_host.c)
target_include_directories(
  libretro_native_host
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../eng/libretro"
)

if(MSVC)
  target_compile_options(libretro_native_host PRIVATE /W4 /WX)
  target_link_libraries(libretro_native_host PRIVATE Psapi)
else()
  target_compile_options(libretro_native_host PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_libraries(libretro_native_host PRIVATE ${CMAKE_DL_LIBS})
endif()

if(CORE_ENABLE_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR "CORE_ENABLE_SANITIZERS is supported only with Clang or GCC")
  endif()
  target_compile_options(
    libretro_native_host
    PRIVATE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
  )
  target_link_options(libretro_native_host PRIVATE -fsanitize=address,undefined)
endif()

if(DEFINED CORE_PATH)
  if(NOT DEFINED CORE_CYCLES)
    set(CORE_CYCLES 25)
  endif()

  set(phase_0_test_arguments "${CORE_PATH}" "${CORE_CYCLES}")
  if(DEFINED CORE_MAX_RSS_GROWTH_MIB)
    list(APPEND phase_0_test_arguments 2 "${CORE_MAX_RSS_GROWTH_MIB}")
  endif()
  if(DEFINED CORE_RESULT_PATH)
    if(NOT DEFINED CORE_MAX_RSS_GROWTH_MIB)
      message(FATAL_ERROR "CORE_RESULT_PATH requires CORE_MAX_RSS_GROWTH_MIB")
    endif()
    list(APPEND phase_0_test_arguments "${CORE_RESULT_PATH}")
  endif()

  enable_testing()
  add_test(
    NAME phase_0_native_lifecycle
    COMMAND libretro_native_host ${phase_0_test_arguments}
  )
  set_tests_properties(phase_0_native_lifecycle PROPERTIES TIMEOUT 300)
endif()
