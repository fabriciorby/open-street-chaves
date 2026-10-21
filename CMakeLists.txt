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
  src/bmp.c
  src/strip.c
  src/wav.c
  src/game.c
  src/tables_gen.c
  src/exetext.c
)
# Only our own code is strict C17; SDL (and Emscripten's EM_ASM) need GNU extensions.
set_target_properties(open-street-chaves PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(open-street-chaves PRIVATE src)
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
    -sEXPORTED_RUNTIME_METHODS=ccall
    --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html)
endif()

if(APPLE)
  set_target_properties(open-street-chaves PROPERTIES MACOSX_BUNDLE OFF)
endif()

install(TARGETS open-street-chaves RUNTIME DESTINATION .)
