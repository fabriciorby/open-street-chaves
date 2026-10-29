cmake_minimum_required(VERSION 3.24)
project(open_street_chaves C)

option(OSC_FETCH_SDL3 "Download and build SDL3 with FetchContent" ON)

if(OSC_FETCH_SDL3)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3)
else()
  find_package(SDL3 REQUIRED CONFIG)
endif()

add_executable(open-street-chaves
  src/main.c
  src/data/bmp.c
  src/data/strip.c
  src/data/wav.c
  src/game.c
  src/data/assets.c
  src/fight/effects.c
  src/fight/fight.c
  src/render/fight_draw.c
  src/render/hud.c
  src/fight/fighter.c
  src/fight/stage.c
  src/fight/combat.c
  src/fight/commands.c
  src/fight/selector.c
  src/fight/cpu.c
  src/gen/tables_gen.c
  src/data/exetext.c
  src/data/exeimg.c
  src/screens/flow.c
  src/screens/screen_select.c
  src/screens/screen_vs.c
  src/screens/screen_continue.c
  src/screens/screen_attract.c
  src/screens/screen_records.c
  src/screens/screen_name.c
  src/screens/screen_fight.c
  src/render/text.c
  src/screens/screen_menu.c
  src/core/config.c
  src/core/unlocks.c
  src/screens/arcade.c
  src/render/font.c
  src/screens/screen_endings.c
  src/screens/movelist.c
)
# Only our own code is strict C17; SDL (and Emscripten's EM_ASM) need GNU extensions.
set_target_properties(open-street-chaves PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(open-street-chaves PRIVATE src third_party)
# stb_truetype is compiled with -Wall; silence its known unused-function noise
set_source_files_properties(src/render/font.c PROPERTIES COMPILE_OPTIONS "$<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-function>")
# bundled fallback font (SIL OFL) next to the executable
add_custom_command(TARGET open-street-chaves POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:open-street-chaves>/assets)
target_link_libraries(open-street-chaves PRIVATE SDL3::SDL3)

if(MSVC)
  target_compile_options(open-street-chaves PRIVATE /W4)
  target_compile_definitions(open-street-chaves PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(open-street-chaves PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(EMSCRIPTEN)
  set_target_properties(open-street-chaves PROPERTIES SUFFIX ".html")
  target_link_options(open-street-chaves PRIVATE
    -sALLOW_MEMORY_GROWTH=1
    -sEXPORTED_RUNTIME_METHODS=ccall,FS
    -sEXPORTED_FUNCTIONS=_main,_osc_web_data_ready
    -lidbfs.js
    -sINITIAL_MEMORY=256MB
    -sMINIFY_HTML=0
    --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
    --embed-file ${CMAKE_SOURCE_DIR}/assets/fonts@/fonts)
endif()

if(APPLE)
  set_target_properties(open-street-chaves PROPERTIES MACOSX_BUNDLE OFF)
endif()

install(TARGETS open-street-chaves RUNTIME DESTINATION .)
