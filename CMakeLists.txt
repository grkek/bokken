cmake_minimum_required(VERSION 3.16)

if(APPLE)
    project(BokkenEngine LANGUAGES CXX C OBJC)
else()
    project(BokkenEngine LANGUAGES CXX C)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Versioning logic — date-based.
string(TIMESTAMP CURRENT_YEAR  "%Y")
string(TIMESTAMP CURRENT_MONTH "%m")
string(TIMESTAMP CURRENT_DAY   "%d")
set(BOKKEN_VERSION  "${CURRENT_YEAR}.${CURRENT_MONTH}.${CURRENT_DAY}")
set(BOKKEN_SOVERSION "${CURRENT_YEAR}")

message(STATUS "Configuring ${PROJECT_NAME} - Version: ${BOKKEN_VERSION}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Everything is vendored at configure time and built from source. No system
# packages required — first `cmake -B build` will clone these into
# third_party/ and subsequent builds reuse the cache.
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party")

# SDL3 — windowing, input, GL context, audio device.
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)

# SDL3_ttf — TTF rasterization (consumed by GlyphCache).
FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
# CRITICAL: tell SDL_ttf to build its own FreeType/HarfBuzz so we don't
# depend on system installations of those libs.
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES  OFF CACHE BOOL "" FORCE)

# QuickJS-NG — scripting runtime.
FetchContent_Declare(
  quickjs_ng
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE
)
set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)

# nlohmann/json — project.bokken parsing.
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

# PhysFS — virtual filesystem for asset packs.
FetchContent_Declare(
  physfs
  GIT_REPOSITORY https://github.com/icculus/physfs.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
set(PHYSFS_BUILD_SHARED      OFF             CACHE BOOL   "" FORCE)
set(PHYSFS_BUILD_STATIC      ON              CACHE BOOL   "" FORCE)
set(PHYSFS_BUILD_TEST        OFF             CACHE BOOL   "" FORCE)
set(PHYSFS_TARGETNAME_STATIC "physfs-static" CACHE STRING "" FORCE)

# miniz — zip/deflate for the asset packer.
FetchContent_Declare(
  miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG        2.2.0
  GIT_SHALLOW    TRUE
)
set(MINIZ_BUILD_SHARED OFF CACHE BOOL "" FORCE)

# GLM — math (header-only).
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE
)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)

# dr_libs — single-header WAV/MP3/FLAC decoders for the audio engine.
FetchContent_Declare(
  dr_libs
  GIT_REPOSITORY https://github.com/mackron/dr_libs.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE
)

# glad — OpenGL loader. Generated source, no runtime DLL/.so dependency.
# We pulled in glad after a previous attempt with GLEW broke on modern
# macOS SDKs (the GLEW CMake build pulls in `-framework AGL` which Apple
# removed from the SDK around macOS 14). glad has no such baggage —
# it ships pure C source, calls our supplied loader (SDL_GL_GetProcAddress)
# at runtime, and links only against the system OpenGL framework.
#
# Configure-time requirement: Python 3 + Jinja2. The glad CMake helpers
# run the generator from CMake. Most macOS / Linux dev boxes have these;
# install with: `pip3 install --user jinja2`. Windows: install Python
# from python.org, then the same pip command.
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG        v2.0.8
  GIT_SHALLOW    TRUE
  # glad's project-level CMakeLists is in cmake/, not the repo root.
  SOURCE_SUBDIR  cmake
)

# Globally prefer static libs.
set(BUILD_SHARED_LIBS         OFF CACHE BOOL "" FORCE)
set(SDLTTF_BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)
set(SDL3TTF_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Populate everything. First run: clones into third_party/ and configures
# each subproject. Subsequent runs: reuses the cached source.
FetchContent_MakeAvailable(SDL3 SDL3_ttf quickjs_ng json physfs miniz glm dr_libs glad)

# Generate the actual glad library. The flags:
#   STATIC       — link statically into the engine, same as our other deps.
#   REPRODUCIBLE — pin the generated code to glad's bundled Khronos specs
#                  rather than fetching latest, so all machines/builds
#                  produce byte-identical output.
#   LOADER       — emit `gladLoadGL(loader_fn)` so we can plug in
#                  SDL_GL_GetProcAddress. Without this flag glad only
#                  ships function pointer tables and you have to write
#                  the loader yourself.
#   API gl:core=3.3 — matches the context profile we request in
#                  Renderer::Base::init.
glad_add_library(glad_gl_core_33 STATIC REPRODUCIBLE LOADER API gl:core=3.3)

# dr_libs is header-only — we need exactly one .c file that defines the
# IMPLEMENTATION macro so the symbols (drwav_init, drwav_read_pcm_frames_*,
# etc.) end up in the build. Only WAV is enabled by default; flip on
# DR_MP3_IMPLEMENTATION / DR_FLAC_IMPLEMENTATION here when you need them.
set(DR_LIBS_IMPL_C "${CMAKE_CURRENT_BINARY_DIR}/dr_libs_impl.c")
file(WRITE "${DR_LIBS_IMPL_C}"
    "#define DR_WAV_IMPLEMENTATION\n"
    "#include <dr_wav.h>\n")
add_library(bokken_dr_libs STATIC "${DR_LIBS_IMPL_C}")
target_include_directories(bokken_dr_libs PUBLIC "${dr_libs_SOURCE_DIR}")

# System OpenGL framework — glad's runtime functions resolve through the
# loader we hand to gladLoadGL (SDL_GL_GetProcAddress), but we still link
# the OS-provided GL library so the symbols glad's loader queries actually
# exist at link/runtime. find_package(OpenGL) just resolves the system
# framework — no install required, it ships with the OS.
find_package(OpenGL REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION                    ${BOKKEN_VERSION}
    SOVERSION                  ${BOKKEN_SOVERSION}
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3-static
    SDL3_ttf::SDL3_ttf-static
    miniz
    qjs
    nlohmann_json::nlohmann_json
    physfs-static
    glm::glm
    glad_gl_core_33     # OpenGL loader (replaces hand-rolled GL surface)
    OpenGL::GL          # System GL — glad's loader resolves into this
    bokken_dr_libs      # audio engine
)

# Public/private include split.
#
# PUBLIC — propagates to anything that links the engine. Only Bokken.hpp
#          (the slim entry-point header) lives in include/. Consumers see
#          this directory; they do NOT see anything under src/ or any of
#          the third-party trees. That's the whole point.
#
# PRIVATE — only the engine's own .cpp files compile against these. The
#           third-party include dirs (PhysFS, etc.) used to be PUBLIC,
#           which forced every consumer to reproduce them — that's how
#           the example's CMakeLists ended up listing 8 third_party paths.
#           Demoted to PRIVATE now that nothing in include/Bokken.hpp
#           references them.
target_include_directories(${PROJECT_NAME}
    PUBLIC  "${CMAKE_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_SOURCE_DIR}/src"
            "${physfs_SOURCE_DIR}/src"
)

# Platform-specific system libs.
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 imm32 setupapi version winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl)
endif()

# Only build these if their source files actually exist in the tree —
# keeps `cmake` happy on a partial checkout.
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/packer/main.cpp")
    add_executable(Packer tools/packer/main.cpp)
    target_link_libraries(Packer PRIVATE qjs miniz)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tools/compiler/main.cpp")
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
endif()