cmake_minimum_required(VERSION 3.16)

if(APPLE)
    project(BokkenEngine LANGUAGES CXX C OBJC)
else()
    project(BokkenEngine LANGUAGES CXX C)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Versioning logic
string(TIMESTAMP CURRENT_YEAR  "%Y")
string(TIMESTAMP CURRENT_MONTH "%m")
string(TIMESTAMP CURRENT_DAY   "%d")
set(BOKKEN_VERSION  "${CURRENT_YEAR}.${CURRENT_MONTH}.${CURRENT_DAY}")
set(BOKKEN_SOVERSION "${CURRENT_YEAR}")

message(STATUS "Configuring ${PROJECT_NAME} - Version: ${BOKKEN_VERSION}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party")

# Dependencies

# SDL3
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)

# SDL3_ttf
FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
# CRITICAL: Tell SDL_ttf to build its own FreeType/HarfBuzz
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE) 
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)

# QuickJS-NG
FetchContent_Declare(
  quickjs_ng
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE
)
set(BUILD_TESTING    OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_LIBC   OFF CACHE BOOL "" FORCE)

# nlohmann/json
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

# PhysFS
FetchContent_Declare(
  physfs
  GIT_REPOSITORY https://github.com/icculus/physfs.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
set(PHYSFS_BUILD_SHARED       OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_STATIC       ON  CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST         OFF CACHE BOOL "" FORCE)
set(PHYSFS_TARGETNAME_STATIC  "physfs-static" CACHE STRING "" FORCE)

# miniz
FetchContent_Declare(
  miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG        2.2.0
  GIT_SHALLOW    TRUE
)
set(MINIZ_BUILD_SHARED OFF CACHE BOOL "" FORCE)

# GLM (OpenGL Mathematics)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE
)
# GLM is header-only, so we don't need to build it as a library
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLTTF_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL3TTF_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# POPULATE EVERYTHING
FetchContent_MakeAvailable(SDL3 SDL3_ttf quickjs_ng json physfs miniz glm)

# BokkenEngine Library

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION                  ${BOKKEN_VERSION}
    SOVERSION                ${BOKKEN_SOVERSION}
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

# Link SDL3 and SDL3_ttf
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3-static
    SDL3_ttf::SDL3_ttf-static
    miniz
    qjs
    nlohmann_json::nlohmann_json
    physfs-static
    glm::glm
)

target_include_directories(${PROJECT_NAME}
    PUBLIC  ${physfs_SOURCE_DIR}/src
    PRIVATE "${CMAKE_SOURCE_DIR}/src"
)

# Platform specific system libs
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 imm32 setupapi version winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl)
endif()

# Tools

add_executable(Packer tools/packer/main.cpp)
target_link_libraries(Packer PRIVATE qjs miniz)

add_executable(Compiler tools/compiler/main.cpp)
target_link_libraries(Compiler PRIVATE
    SDL3::SDL3-static
    SDL3_ttf::SDL3_ttf-static
    miniz
    qjs
    nlohmann_json::nlohmann_json
    physfs-static
    glm::glm
)