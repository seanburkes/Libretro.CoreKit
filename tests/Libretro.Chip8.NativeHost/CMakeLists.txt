cmake_minimum_required(VERSION 3.20)

project(LibretroChip8NativeHost LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(CORE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_executable(libretro_chip8_native_host chip8_host.c)
target_include_directories(
  libretro_chip8_native_host
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../eng/libretro"
)
target_compile_options(
  libretro_chip8_native_host
  PRIVATE
  -Wall -Wextra -Wpedantic -Werror
)
target_link_libraries(libretro_chip8_native_host PRIVATE ${CMAKE_DL_LIBS})

if(CORE_ENABLE_SANITIZERS)
  target_compile_options(
    libretro_chip8_native_host
    PRIVATE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
  )
  target_link_options(libretro_chip8_native_host PRIVATE -fsanitize=address,undefined)
endif()

if(DEFINED CORE_PATH)
  if(NOT DEFINED CORE_CYCLES)
    set(CORE_CYCLES 25)
  endif()

  set(chip8_test_arguments "${CORE_PATH}" "${CORE_CYCLES}")
  if(DEFINED CORE_RESULT_PATH)
    list(APPEND chip8_test_arguments "${CORE_RESULT_PATH}")
  endif()

  enable_testing()
  add_test(
    NAME phase_4_chip8
    COMMAND libretro_chip8_native_host ${chip8_test_arguments}
  )
  set_tests_properties(phase_4_chip8 PROPERTIES TIMEOUT 300)
endif()
