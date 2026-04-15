cmake_minimum_required(VERSION 3.16)

# OBJC is required on Apple because PhysFS pulls in Objective-C platform glue.
if(APPLE)
    project(NeonKatana LANGUAGES CXX C OBJC)
else()
    project(NeonKatana LANGUAGES CXX C)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine paths
set(ENGINE_ROOT    "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(ENGINE_SRC     "${ENGINE_ROOT}/src")
set(ENGINE_BUILD   "${ENGINE_ROOT}/build")
set(ENGINE_BIN     "${ENGINE_BUILD}/bin")
set(THIRD_PARTY    "${ENGINE_ROOT}/third_party")

# Locate the pre-built engine library
if(APPLE)
    set(ENGINE_LIB "${ENGINE_BIN}/libBokkenEngine.dylib")
elseif(WIN32)
    set(ENGINE_LIB "${ENGINE_BIN}/BokkenEngine.lib")
else()
    set(ENGINE_LIB "${ENGINE_BIN}/libBokkenEngine.so")
endif()

if(NOT EXISTS "${ENGINE_LIB}")
    message(FATAL_ERROR
        "Engine library not found at: ${ENGINE_LIB}\n"
        "Build the engine first from the repo root with: make build")
endif()

# Source files
file(GLOB_RECURSE COMMON_SOURCES "src/*.cpp")

if(APPLE)
    file(GLOB PLATFORM_SOURCES "darwin/*.cpp")
elseif(WIN32)
    file(GLOB PLATFORM_SOURCES "windows/*.cpp")
else()
    file(GLOB PLATFORM_SOURCES "linux/*.cpp")
endif()

# Executable
# Use WIN32 flag on Windows to allow for WinMain entry if needed, 
# though your main.cpp currently uses the Console subsystem.
if(WIN32)
    add_executable(${PROJECT_NAME} ${COMMON_SOURCES} ${PLATFORM_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${COMMON_SOURCES} ${PLATFORM_SOURCES})
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ENGINE_SRC}"
    "${THIRD_PARTY}/json-src/include"
    "${THIRD_PARTY}/physfs-src/src"
    "${THIRD_PARTY}/sdl3-src/include"
    "${THIRD_PARTY}/sdl3-build/include-revision"
    "${THIRD_PARTY}/sdl3_ttf-src/include"
    "${THIRD_PARTY}/quickjs_ng-src"
)

# Link Section

target_link_libraries(${PROJECT_NAME} PRIVATE "${ENGINE_LIB}")

if(APPLE)
    # Fixes the Undefined Symbols error for CFBundle/CFURL calls
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
    
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${ENGINE_BIN}"
        INSTALL_RPATH "@executable_path"
    )
elseif(WIN32)
    # Standard Windows system libs often required when bypassing SDL_main
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 user32 gdi32)
elseif(UNIX)
    # Linux RPATH and math/thread libs
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${ENGINE_BIN}"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# Build Steps

if(NOT DEFINED SCRIPTS_DIR)
    message(FATAL_ERROR "SCRIPTS_DIR is not set.")
endif()

set(COMPILED_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/compiled")

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND "${ENGINE_BIN}/Compiler" --source "${SCRIPTS_DIR}" --output "${COMPILED_SCRIPTS_DIR}"
    COMMENT "Compiler: compiling scripts to bytecode"
    VERBATIM
)

set(ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assets")
if(EXISTS "${ASSET_ROOT}")
    file(GLOB ASSET_GROUPS RELATIVE "${ASSET_ROOT}" "${ASSET_ROOT}/*")
    foreach(GROUP ${ASSET_GROUPS})
        if(IS_DIRECTORY "${ASSET_ROOT}/${GROUP}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${ENGINE_BIN}/Packer" "${ASSET_ROOT}/${GROUP}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/${GROUP}"
                COMMENT "Packer: packing '${GROUP}'"
                VERBATIM
            )
        endif()
    endforeach()
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${ENGINE_BIN}/Packer" "${COMPILED_SCRIPTS_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/scripts"
    COMMENT "Packer: packing compiled scripts"
    VERBATIM
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/project.bokken"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/project.bokken"
    COMMENT "Deploying project.bokken"
    VERBATIM
)