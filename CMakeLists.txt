# RAVEFORM - one CMake tree for the Windows dev build and the Android libmain.so.
# Desktop: scripts/build-desktop.ps1 (native CMake, Ninja, llvm-mingw clang).
# Android: android/ Gradle project points externalNativeBuild here (SDK CMake 3.22.1, NDK r29).

cmake_minimum_required(VERSION 3.22)
project(raveform VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(RF_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(RF_TP ${RF_ROOT}/third_party)
set(RF_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${RF_GEN})

foreach(dep SDL/CMakeLists.txt volk/volk.c vulkan-headers/include/vulkan/vulkan.h)
  if(NOT EXISTS ${RF_TP}/${dep})
    message(FATAL_ERROR "third_party/${dep} is missing - run scripts/fetch-deps.ps1")
  endif()
endforeach()

# ---- SDL3 (static on Windows, shared on Android) ---------------------------------------------
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
if(ANDROID)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(${RF_TP}/SDL ${CMAKE_CURRENT_BINARY_DIR}/SDL)

# ---- volk + the pinned Vulkan headers (same copy on both platforms) ---------------------------
add_library(rf_volk STATIC ${RF_TP}/volk/volk.c)
target_include_directories(rf_volk SYSTEM PUBLIC ${RF_TP}/vulkan-headers/include ${RF_TP}/volk)
target_compile_definitions(rf_volk PUBLIC VK_NO_PROTOTYPES)
if(ANDROID)
  # The surface is created from our own ANativeWindow reference (vkCreateAndroidSurfaceKHR).
  target_compile_definitions(rf_volk PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
  target_link_libraries(rf_volk PUBLIC dl)
endif()

# ---- Shaders: GLSL -> SPIR-V 1.3 (Vulkan 1.1) embedded as C arrays ---------------------------
if(ANDROID)
  find_program(RF_GLSLC glslc
    HINTS ${ANDROID_NDK}/shader-tools/windows-x86_64 ${ANDROID_NDK}/shader-tools/linux-x86_64
          ${ANDROID_NDK}/shader-tools/darwin-x86_64 ${CMAKE_ANDROID_NDK}/shader-tools/windows-x86_64
    NO_DEFAULT_PATH)
else()
  find_program(RF_GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)
endif()
if(NOT RF_GLSLC)
  message(FATAL_ERROR "glslc not found (desktop: VULKAN_SDK, Android: NDK shader-tools)")
endif()

set(RF_GENERATED)
foreach(shader capsule.vert capsule.frag)
  set(out ${RF_GEN}/${shader}.inc)
  add_custom_command(OUTPUT ${out}
    COMMAND ${RF_GLSLC} --target-env=vulkan1.1 -O -Werror -mfmt=c -o ${out} ${RF_ROOT}/shaders/${shader}
    DEPENDS ${RF_ROOT}/shaders/${shader}
    COMMENT "glslc ${shader}"
    VERBATIM)
  list(APPEND RF_GENERATED ${out})
endforeach()

# The stroke font is source data from design/; embed it verbatim and parse it at start-up.
add_custom_command(OUTPUT ${RF_GEN}/vector_font.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/vector_font.txt -DOUT=${RF_GEN}/vector_font.inc
          -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/design/vector_font.txt ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed vector_font.txt"
  VERBATIM)
list(APPEND RF_GENERATED ${RF_GEN}/vector_font.inc)

# ---- raveform_core: everything except the entry point ----------------------------------------
set(RF_CORE_SOURCES
  src/core/alloc_counter.cpp
  src/core/args.cpp
  src/core/log.cpp
  src/core/png.cpp
  src/core/stats.cpp
  src/gfx/capsule.cpp
  src/gfx/frame_pacer.cpp
  src/gfx/gpu_context.cpp
  src/gfx/renderer.cpp
  src/gfx/swapchain.cpp
  src/gfx/vector_font.cpp
  src/audio/test_tone.cpp
  src/game/m0_scene.cpp
  src/tools/bench.cpp
  src/tools/dev_overlay.cpp
  src/platform/app.cpp
)
if(ANDROID)
  list(APPEND RF_CORE_SOURCES src/audio/audio_device_oboe.cpp src/platform/android/android_glue.cpp)
else()
  list(APPEND RF_CORE_SOURCES src/audio/audio_device_sdl.cpp)
endif()

add_library(raveform_core OBJECT ${RF_CORE_SOURCES} ${RF_GENERATED})
target_include_directories(raveform_core PUBLIC ${RF_ROOT}/src ${RF_GEN})
target_link_libraries(raveform_core PUBLIC rf_volk SDL3::Headers)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(RF_BUILD_NAME debug)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(RF_BUILD_NAME release)
else()
  set(RF_BUILD_NAME dev)
endif()
target_compile_definitions(raveform_core PUBLIC
  RF_BUILD_NAME="${RF_BUILD_NAME}"
  $<$<CONFIG:Debug>:RF_DEBUG=1>)
target_compile_options(raveform_core PUBLIC
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
  -fno-math-errno)
target_compile_options(raveform_core PRIVATE
  -Wall -Wextra -Wno-missing-field-initializers -Werror)

if(ANDROID)
  find_package(oboe REQUIRED CONFIG)
  find_package(games-frame-pacing REQUIRED CONFIG)
  target_link_libraries(raveform_core PUBLIC oboe::oboe games-frame-pacing::swappy_static android log)

  # libmain.so is loaded by SDLActivity and its SDL_main() is the entry point.
  add_library(main SHARED src/platform/main.cpp)
  target_link_libraries(main PRIVATE raveform_core SDL3::SDL3-shared)
  target_compile_options(main PRIVATE -Wall -Wextra -Werror)
  target_link_options(main PRIVATE -Wl,--no-undefined)
else()
  add_executable(raveform_desktop src/platform/main.cpp)
  set_target_properties(raveform_desktop PROPERTIES OUTPUT_NAME raveform)
  target_link_libraries(raveform_desktop PRIVATE raveform_core SDL3::SDL3-static)
  target_compile_options(raveform_desktop PRIVATE -Wall -Wextra -Werror)
  # llvm-mingw: link libc++, libunwind and winpthreads statically so the exe runs on its own.
  target_link_options(raveform_desktop PRIVATE -static)
endif()
