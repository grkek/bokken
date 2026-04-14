cmake_minimum_required(VERSION 3.16)

if(APPLE)
    project(BokkenEngine LANGUAGES CXX C OBJC)
else()
    project(BokkenEngine LANGUAGES CXX C)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Versioning logic based on build date
string(TIMESTAMP CURRENT_YEAR  "%Y")
string(TIMESTAMP CURRENT_MONTH "%m")
string(TIMESTAMP CURRENT_DAY   "%d")
set(BOKKEN_VERSION  "${CURRENT_YEAR}.${CURRENT_MONTH}.${CURRENT_DAY}")
set(BOKKEN_SOVERSION "${CURRENT_YEAR}")

message(STATUS "Configuring ${PROJECT_NAME} - Version: ${BOKKEN_VERSION}")

# Ensure binaries and libs land in the same bin folder
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

# miniz 2.2.0
FetchContent_Declare(
  miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG        2.2.0
  GIT_SHALLOW    TRUE
)
set(MINIZ_BUILD_SHARED OFF CACHE BOOL "" FORCE)

# Populate all dependencies
FetchContent_MakeAvailable(SDL3 quickjs_ng json physfs miniz)

# Compiler Options

# Silence warnings from third-party targets
set(THIRD_PARTY_TARGETS qjs physfs-static miniz)
foreach(TARGET ${THIRD_PARTY_TARGETS})
    if(TARGET ${TARGET})
        if(MSVC)
            target_compile_options(${TARGET} PRIVATE /W0)
        else()
            target_compile_options(${TARGET} PRIVATE -w)
        endif()
    endif()
endforeach()

# BokkenEngine Library

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION                  ${BOKKEN_VERSION}
    SOVERSION                ${BOKKEN_SOVERSION}
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3-static
    miniz            # Linked from FetchContent
    qjs
    nlohmann_json::nlohmann_json
    physfs-static
)

target_include_directories(${PROJECT_NAME}
    # Expose PhysFS headers to anything that links BokkenEngine
    PUBLIC  ${physfs_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        shell32 imm32 setupapi version winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    BOKKEN_VERSION="${BOKKEN_VERSION}")

# Tools

# Packer tool
add_executable(Packer tools/packer/main.cpp)
target_link_libraries(Packer PRIVATE 
    qjs 
    miniz
)

# Compiler tool
add_executable(Compiler tools/compiler/main.cpp)
target_link_libraries(Compiler PRIVATE 
    ${PROJECT_NAME} 
    SDL3::SDL3-static
    qjs
    physfs-static
)

target_include_directories(Compiler PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)