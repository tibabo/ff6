cmake_minimum_required(VERSION 3.20)
project(ff6_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Game data and field rules, one source per file of the disassembly (src/field, src/event, src/data): no graphics library.
add_library(ff6_core STATIC
  src/data/json.cpp
  src/data/game_rules.cpp
  src/data/battle_data.cpp
  src/data/battle_gfx.cpp
  src/data/attack_anim.cpp
  src/data/pack.cpp
  src/data/save.cpp
  src/field/anim.cpp
  src/field/bg.cpp
  src/field/color.cpp
  src/field/entrance.cpp
  src/field/event.cpp
  src/field/event_cmd.cpp
  src/field/map.cpp
  src/field/obj.cpp
  src/field/obj_party.cpp
  src/field/obj_script.cpp
  src/field/player.cpp
  src/field/scroll.cpp
  src/field/sprite_data.cpp
  src/field/text.cpp
  src/field/window.cpp
  src/menu/character_menu.cpp
  src/menu/config_menu.cpp
  src/menu/field_menu.cpp
  src/menu/item_menu.cpp
  src/menu/menu_data.cpp
  src/menu/menu_screen.cpp
  src/menu/shop_menu.cpp
  src/menu/window_style.cpp
  src/state/char_stats.cpp
  src/battle/battle.cpp
  src/battle/action.cpp
  src/battle/ai.cpp
  src/battle/attack_anim_player.cpp
  src/battle/battle_screen.cpp
  src/battle/encounters.cpp
  src/state/party_ops.cpp)
target_include_directories(ff6_core PUBLIC src)

# Game state and drawing through the abstract ff6::Renderer: still no graphics library.
add_library(ff6_game STATIC src/game.cpp)
target_link_libraries(ff6_game PUBLIC ff6_core)

# Sound: BRR samples, the voices of the sound chip and the music (src/sound). No sound library either.
add_library(ff6_audio STATIC
  src/audio/akao.cpp
  src/audio/akao_script.cpp
  src/audio/audio_data.cpp
  src/audio/brr.cpp
  src/audio/dsp_voice.cpp
  src/audio/sound_program.cpp
  src/audio/synth.cpp)
target_link_libraries(ff6_audio PUBLIC ff6_core)

foreach(target ff6_core ff6_game ff6_audio)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
  # One test file per source file; FF6_PACK (a pack built by `node ff6.mjs build-pack`) enables the data tests.
  add_executable(ff6_tests
    tests/main.cpp
    tests/game_tests.cpp
    tests/data/pack_tests.cpp
    tests/data/save_tests.cpp
    tests/data/battle_data_tests.cpp
    tests/data/battle_gfx_tests.cpp
    tests/data/attack_anim_tests.cpp
    tests/battle/attack_anim_player_tests.cpp
    tests/battle/battle_tests.cpp
    tests/battle/action_tests.cpp
    tests/audio/audio_data_tests.cpp
    tests/audio/akao_tests.cpp
    tests/audio/brr_tests.cpp
    tests/audio/dsp_voice_tests.cpp
    tests/audio/sound_program_tests.cpp
    tests/field/anim_tests.cpp
    tests/field/bg_tests.cpp
    tests/field/color_tests.cpp
    tests/field/entrance_tests.cpp
    tests/field/event_tests.cpp
    tests/field/event_cmd_tests.cpp
    tests/field/map_tests.cpp
    tests/field/obj_tests.cpp
    tests/field/obj_script_tests.cpp
    tests/field/obj_party_tests.cpp
    tests/field/player_tests.cpp
    tests/field/scroll_tests.cpp
    tests/field/sprite_data_tests.cpp
    tests/field/text_tests.cpp
    tests/menu/menu_tests.cpp
    tests/state/party_tests.cpp)
  target_link_libraries(ff6_tests PRIVATE ff6_game ff6_audio)
  add_test(NAME ff6_tests COMMAND ff6_tests)
endif()

# Renders instrument samples to a WAV file (no sound device needed).
add_executable(ff6_audio_tool src/audio_tool.cpp)
target_link_libraries(ff6_audio_tool PRIVATE ff6_audio)
set_target_properties(ff6_audio_tool PROPERTIES OUTPUT_NAME ff6_audio)
if(MSVC)
  target_compile_options(ff6_audio_tool PRIVATE /W4 /permissive-)
else()
  target_compile_options(ff6_audio_tool PRIVATE -Wall -Wextra)
endif()

# SDL2 backend (window, renderer, input) and entry point. To use another graphics library,
# implement ff6::Renderer and an input source for ff6::InputState, then replace these files.
option(FF6_BUILD_GAME "Build the SDL2 executable" ON)
if(FF6_BUILD_GAME)
  find_package(SDL2 CONFIG REQUIRED)
  add_executable(ff6 src/main.cpp src/sdl/sdl_audio.cpp src/sdl/sdl_renderer.cpp src/sdl/sdl_input.cpp)
  if(TARGET SDL2::SDL2main)
    target_link_libraries(ff6 PRIVATE SDL2::SDL2main)
  endif()
  target_link_libraries(ff6 PRIVATE ff6_game ff6_audio SDL2::SDL2)
  if(MSVC)
    target_compile_options(ff6 PRIVATE /W4 /permissive-)
  else()
    target_compile_options(ff6 PRIVATE -Wall -Wextra)
  endif()
endif()
