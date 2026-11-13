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

foreach(dep SDL/CMakeLists.txt volk/volk.c vulkan-headers/include/vulkan/vulkan.h enet/host.c)
  if(NOT EXISTS ${RF_TP}/${dep})
    message(FATAL_ERROR "third_party/${dep} is missing - run scripts/fetch-deps.ps1")
  endif()
endforeach()

# User rule: builds use at most 4 parallel jobs. Ninja job pools hold even when the caller does not
# pass -j (the Android Gradle plugin runs Ninja with its default of every core). Set before SDL so its
# targets join the pool too; scripts/env.ps1 also confines the whole process tree to 4 cores.
set(RF_MAX_JOBS 4 CACHE STRING "Parallel compile, link and custom-command jobs (Ninja job pool)")
set_property(GLOBAL APPEND PROPERTY JOB_POOLS rf_jobs=${RF_MAX_JOBS})
set(CMAKE_JOB_POOL_COMPILE rf_jobs)
set(CMAKE_JOB_POOL_LINK rf_jobs)
set(CMAKE_JOB_POOL_PRECOMPILE_HEADER rf_jobs)

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
# name|source|extra glslc arguments (TECH_SPEC 2: SPIR-V 1.3 for Vulkan 1.1, embedded as C arrays)
set(RF_SHADERS
  "capsule.vert|capsule.vert|"
  "capsule.frag|capsule.frag|"
  "glyph.vert|glyph.vert|"
  "particle.vert|particle.vert|"
  "afterglow.frag|afterglow.frag|"
  "distort.frag|distort.frag|"
  "background.frag|background.frag|"
  "bloom_final.frag|bloom_final.frag|"
  "glow.vert|glow.vert|"
  "glow.frag|glow.frag|"
  "tri.vert|tri.vert|"
  "tri.frag|tri.frag|"
  "fullscreen.vert|fullscreen.vert|"
  "bloom_down.frag|bloom_down.frag|"
  "bloom_up.frag|bloom_up.frag|"
  "composite.frag|composite.frag|")
foreach(entry ${RF_SHADERS})
  string(REPLACE "|" ";" parts "${entry}")
  list(GET parts 0 name)
  list(GET parts 1 source)
  list(LENGTH parts nparts)
  set(extra "")
  if(nparts GREATER 2)
    list(GET parts 2 extra)
  endif()
  set(out ${RF_GEN}/${name}.inc)
  if(source MATCHES "vert$")
    set(stage vert)
  else()
    set(stage frag)
  endif()
  add_custom_command(OUTPUT ${out}
    COMMAND ${RF_GLSLC} --target-env=vulkan1.1 -O -Werror -fshader-stage=${stage} ${extra} -mfmt=c -o ${out} ${RF_ROOT}/shaders/${source}
    DEPENDS ${RF_ROOT}/shaders/${source}
    COMMENT "glslc ${name}"
    JOB_POOL rf_jobs
    VERBATIM)
  list(APPEND RF_GENERATED ${out})
endforeach()

# Design data is source data from design/: embedded verbatim and parsed at start-up (never retyped).
foreach(data vector_font.txt look.json look_variants.json palettes.json glyphs.json icons.json)
  add_custom_command(OUTPUT ${RF_GEN}/${data}.inc
    COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/${data} -DOUT=${RF_GEN}/${data}.inc
            -P ${RF_ROOT}/cmake/EmbedText.cmake
    DEPENDS ${RF_ROOT}/design/${data} ${RF_ROOT}/cmake/EmbedText.cmake
    COMMENT "embed ${data}"
    JOB_POOL rf_jobs
    VERBATIM)
  list(APPEND RF_GENERATED ${RF_GEN}/${data}.inc)
endforeach()

# M2 audio begin
# The music (design/music/*.json) and the engine's loudness calibration (src/audio/), embedded like the
# design data above. raveform_audio is the band (engine, synthesis, effects, music data): no SDL, no
# platform code, so the headless renderer and the tests link it too. Its DSP is optimised in every
# configuration, so a Debug build still plays in real time.
set(RF_AUDIO_GENERATED)
foreach(data patches variants conductor track01 track02 track03 track04 track05 track06 track07 track08 track09 track10)
  add_custom_command(OUTPUT ${RF_GEN}/music_${data}.json.inc
    COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/music/${data}.json -DOUT=${RF_GEN}/music_${data}.json.inc
            -P ${RF_ROOT}/cmake/EmbedText.cmake
    DEPENDS ${RF_ROOT}/design/music/${data}.json ${RF_ROOT}/cmake/EmbedText.cmake
    COMMENT "embed music/${data}.json"
    JOB_POOL rf_jobs
    VERBATIM)
  list(APPEND RF_AUDIO_GENERATED ${RF_GEN}/music_${data}.json.inc)
endforeach()
add_custom_command(OUTPUT ${RF_GEN}/audio_loudness_calibration.json.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/src/audio/loudness_calibration.json
          -DOUT=${RF_GEN}/audio_loudness_calibration.json.inc -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/src/audio/loudness_calibration.json ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed audio/loudness_calibration.json"
  JOB_POOL rf_jobs
  VERBATIM)
list(APPEND RF_AUDIO_GENERATED ${RF_GEN}/audio_loudness_calibration.json.inc)
add_library(raveform_audio STATIC
  src/audio/bake.cpp
  src/audio/content_sound.cpp
  src/audio/engine.cpp
  src/audio/fft.cpp
  src/audio/music_data.cpp
  src/audio/reverb.cpp
  src/audio/tempo_map.cpp
  src/audio/wav.cpp
  ${RF_AUDIO_GENERATED})
target_include_directories(raveform_audio PUBLIC ${RF_ROOT}/src ${RF_GEN})
target_compile_options(raveform_audio PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -fno-math-errno)
target_compile_options(raveform_audio PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Werror $<$<CONFIG:Debug>:-O2>)
set_target_properties(raveform_audio PROPERTIES POSITION_INDEPENDENT_CODE ON)
# M2 audio end

# M3 sim begin
# The simulation (TECH_SPEC 7): world, pools, grid, enemies, spawner, player controller and bot. No SDL,
# no Vulkan, no audio: the game, the headless runs and the tests link it. design/species.json is embedded
# like the other design data. Optimised in every configuration, like the band, so a Debug build keeps up.
add_custom_command(OUTPUT ${RF_GEN}/species.json.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/species.json -DOUT=${RF_GEN}/species.json.inc
          -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/design/species.json ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed species.json"
  JOB_POOL rf_jobs
  VERBATIM)
# M5: every boss page is embedded the same way.
add_custom_command(OUTPUT ${RF_GEN}/kaleidos.json.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/bosses/kaleidos.json -DOUT=${RF_GEN}/kaleidos.json.inc
          -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/design/bosses/kaleidos.json ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed kaleidos.json"
  JOB_POOL rf_jobs
  VERBATIM)
add_custom_command(OUTPUT ${RF_GEN}/longwave.json.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/bosses/longwave.json -DOUT=${RF_GEN}/longwave.json.inc
          -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/design/bosses/longwave.json ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed longwave.json"
  JOB_POOL rf_jobs
  VERBATIM)
add_custom_command(OUTPUT ${RF_GEN}/horizon.json.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/bosses/horizon.json -DOUT=${RF_GEN}/horizon.json.inc
          -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/design/bosses/horizon.json ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed horizon.json"
  JOB_POOL rf_jobs
  VERBATIM)
add_custom_command(OUTPUT ${RF_GEN}/choir.json.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/bosses/choir.json -DOUT=${RF_GEN}/choir.json.inc
          -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/design/bosses/choir.json ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed choir.json"
  JOB_POOL rf_jobs
  VERBATIM)
add_custom_command(OUTPUT ${RF_GEN}/minibosses.json.inc
  COMMAND ${CMAKE_COMMAND} -DIN=${RF_ROOT}/design/bosses/minibosses.json -DOUT=${RF_GEN}/minibosses.json.inc
          -P ${RF_ROOT}/cmake/EmbedText.cmake
  DEPENDS ${RF_ROOT}/design/bosses/minibosses.json ${RF_ROOT}/cmake/EmbedText.cmake
  COMMENT "embed minibosses.json"
  JOB_POOL rf_jobs
  VERBATIM)
add_library(raveform_sim STATIC
  src/sim/boss.cpp
  src/sim/bot.cpp
  src/sim/director.cpp
  src/sim/grid.cpp
  src/sim/player_controller.cpp
  src/sim/sim_runner.cpp
  src/sim/species.cpp
  src/sim/upgrades.cpp
  src/sim/world.cpp
  src/sim/world_boss.cpp
  src/sim/world_choir.cpp
  src/sim/world_enemies.cpp
  src/sim/world_families.cpp
  src/sim/world_families4.cpp
  src/sim/world_horizon.cpp
  src/sim/world_longwave.cpp
  src/sim/world_miniboss.cpp
  src/sim/world_net.cpp
  src/sim/world_spawner.cpp
  src/sim/world_upgrades.cpp
  src/sim/world_upgrades_m5.cpp
  ${RF_GEN}/species.json.inc
  ${RF_GEN}/kaleidos.json.inc
  ${RF_GEN}/longwave.json.inc
  ${RF_GEN}/choir.json.inc
  ${RF_GEN}/horizon.json.inc
  ${RF_GEN}/minibosses.json.inc)
target_include_directories(raveform_sim PUBLIC ${RF_ROOT}/src ${RF_GEN})
target_compile_options(raveform_sim PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -fno-math-errno)
target_compile_options(raveform_sim PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Werror $<$<CONFIG:Debug>:-O2>)
set_target_properties(raveform_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)
# M3 sim end

# M4 net begin
# ENet 1.3.18 (scripts/fetch-deps.ps1, pinned by commit). Its protocol sources are compiled with a forced
# include that sends their socket and clock calls to RAVEFORM's datagram layer (src/net/wire.cpp: the
# condition simulator, the loopback network, the simulated clock); its platform file keeps the real ones.
set(RF_ENET ${RF_TP}/enet)
set(RF_ENET_CORE ${RF_ENET}/callbacks.c ${RF_ENET}/compress.c ${RF_ENET}/host.c ${RF_ENET}/list.c ${RF_ENET}/packet.c
                 ${RF_ENET}/peer.c ${RF_ENET}/protocol.c)
if(WIN32)
  set(RF_ENET_PLATFORM ${RF_ENET}/win32.c)
else()
  set(RF_ENET_PLATFORM ${RF_ENET}/unix.c)
endif()
add_library(rf_enet STATIC ${RF_ENET_CORE} ${RF_ENET_PLATFORM})
target_include_directories(rf_enet SYSTEM PUBLIC ${RF_ENET}/include)
set_source_files_properties(${RF_ENET_CORE} PROPERTIES COMPILE_OPTIONS "-include;${RF_ROOT}/src/net/enet_remap.h")
target_compile_options(rf_enet PRIVATE -w $<$<CONFIG:Debug>:-O2>)
if(WIN32)
  target_link_libraries(rf_enet PUBLIC ws2_32 winmm)
else()
  # What ENet's own configure step finds on Linux and Android (bionic has all of it).
  target_compile_definitions(rf_enet PRIVATE HAS_FCNTL=1 HAS_POLL=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1 HAS_GETHOSTBYNAME_R=1
                             HAS_GETHOSTBYADDR_R=1 HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
endif()
set_target_properties(rf_enet PROPERTIES POSITION_INDEPENDENT_CODE ON)
# The co-op network (TECH_SPEC 8): transport, protocol, time and song sync, replication, headless peers. No
# SDL, Vulkan or audio: the game, raveform_headless and the tests link it.
add_library(raveform_net STATIC
  src/net/bots.cpp
  src/net/link.cpp
  src/net/lobby.cpp
  src/net/peer.cpp
  src/net/protocol.cpp
  src/net/session.cpp
  src/net/sync.cpp
  src/net/wire.cpp)
target_include_directories(raveform_net PUBLIC ${RF_ROOT}/src ${RF_GEN})
target_link_libraries(raveform_net PUBLIC raveform_sim rf_enet)
if(WIN32)
  target_link_libraries(raveform_net PUBLIC iphlpapi)  # GetAdaptersAddresses (the lobby's room code and broadcasts)
endif()
target_compile_options(raveform_net PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -fno-math-errno)
target_compile_options(raveform_net PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Werror $<$<CONFIG:Debug>:-O2>)
set_target_properties(raveform_net PROPERTIES POSITION_INDEPENDENT_CODE ON)
# M4 net end

# ---- raveform_core: everything except the entry point ----------------------------------------
set(RF_CORE_SOURCES
  src/core/alloc_counter.cpp
  src/core/args.cpp
  src/core/json_doc.cpp
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
  src/content/glyphs.cpp
  src/content/look.cpp
  src/content/palette.cpp
  src/game/glyph_sheet.cpp
  src/game/lattice.cpp
  src/game/m0_scene.cpp
  src/game/rfscene.cpp
  src/game/stage.cpp
  src/game/tiers.cpp
  src/tools/bench.cpp
  src/tools/dev_overlay.cpp
  src/tools/frame_probe.cpp
  src/platform/app.cpp
  src/platform/cpu_affinity.cpp
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

# M2 audio begin
target_link_libraries(raveform_core PUBLIC raveform_audio)
# The band's main-thread side (device clock, heard events, haptics) and the audio scenes.
target_sources(raveform_core PRIVATE
  src/audio/audio_args.cpp
  src/audio/band.cpp
  src/audio/platform_audio.cpp
  src/game/audio_scene.cpp)
# M3: the game (the fight of stage A) on the simulation.
target_link_libraries(raveform_core PUBLIC raveform_sim)
target_sources(raveform_core PRIVATE
  src/content/icons.cpp
  src/game/music_clock.cpp
  src/game/play.cpp
  src/game/play_hud.cpp
  src/game/play_pictures.cpp
  src/game/play_pictures_m5.cpp
  src/game/play_view.cpp
  src/game/play_view_b.cpp
  src/game/play_view_m5.cpp
  src/game/play_view_m5b.cpp
  src/game/pending_sounds.cpp
  src/game/upgrade_sounds.cpp
  src/game/play_view_upgrades.cpp
  src/game/play_view_shots.cpp
  src/game/screens.cpp
  src/game/settings.cpp)
# M4: co-op in the game (the network library; the game hosts or joins from the command line).
target_link_libraries(raveform_core PUBLIC raveform_net)
target_sources(raveform_core PRIVATE src/game/play_net.cpp src/game/play_view_coop.cpp src/game/play_coop.cpp src/game/screens_coop.cpp)
if(NOT ANDROID)
  # The band without a window: offline renders, the loudness calibration, the unit tests.
  set(RF_HEADLESS_CORE src/core/json_doc.cpp src/core/log.cpp src/core/alloc_counter.cpp)
  add_executable(raveform_headless src/tools/audio_render.cpp src/tools/sim_headless.cpp src/tools/net_headless.cpp ${RF_HEADLESS_CORE})
  target_link_libraries(raveform_headless PRIVATE raveform_sim raveform_audio raveform_net)  # M3: --sim runs; M4: --net
  target_compile_options(raveform_headless PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Werror)
  target_link_options(raveform_headless PRIVATE -static)
  # M5: the palettes too (one for each of the ten Tracks).
  add_executable(raveform_tests tests/audio_tests.cpp src/content/palette.cpp ${RF_GEN}/palettes.json.inc ${RF_HEADLESS_CORE})
  target_link_libraries(raveform_tests PRIVATE raveform_audio)
  target_compile_options(raveform_tests PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Werror)
  target_link_options(raveform_tests PRIVATE -static)
  # M3: the simulation's unit tests.
  add_executable(raveform_sim_tests tests/sim_tests.cpp ${RF_HEADLESS_CORE})
  target_link_libraries(raveform_sim_tests PRIVATE raveform_sim)
  target_compile_options(raveform_sim_tests PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Werror)
  target_link_options(raveform_sim_tests PRIVATE -static)
  # M4: the network's unit tests (packet parsing fuzz, the wire and its condition simulator, clock and song sync,
  # a short loopback session).
  add_executable(raveform_net_tests tests/net_tests.cpp ${RF_HEADLESS_CORE})
  target_link_libraries(raveform_net_tests PRIVATE raveform_net)
  target_compile_options(raveform_net_tests PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Werror)
  target_link_options(raveform_net_tests PRIVATE -static)
endif()
# M2 audio end

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
