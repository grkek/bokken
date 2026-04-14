BINARY_NAME = BokkenEngine
BUILD_DIR = build
WINDOWS_BUILD_DIR = build_windows
TOOLCHAIN_FILE = mingw-w64.cmake

NINJA_FOUND := $(shell command -v ninja 2> /dev/null)

ifdef NINJA_FOUND
    GENERATOR = -G Ninja
else
    GENERATOR = 
endif

# OS Detection and Extension Setup
ifeq ($(OS),Windows_NT)
    LIB_EXT = .dll
    LIB_PREFIX = 
    P_SEP = \\
    CLEAN_CMD = rmdir /s /q $(BUILD_DIR) $(WINDOWS_BUILD_DIR) 2>nul || exit 0
    TOOLCHAIN_PATH = $(TOOLCHAIN_FILE)
else
    DETECTED_OS = $(shell uname -s)
    P_SEP = /
    CLEAN_CMD = rm -rf $(BUILD_DIR) $(WINDOWS_BUILD_DIR)
    TOOLCHAIN_PATH = $(shell pwd)/$(TOOLCHAIN_FILE)
    ifeq ($(DETECTED_OS),Darwin)
        LIB_EXT = .dylib
        LIB_PREFIX = lib
    else
        LIB_EXT = .so
        LIB_PREFIX = lib
    endif
endif

all: build

setup:
	cmake -S . -B $(BUILD_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=Debug

build: setup
	cmake --build $(BUILD_DIR)

# Run shows you where the library is
run: build
	@echo "Target: $(LIB_PREFIX)$(BINARY_NAME)$(LIB_EXT)"
	@ls -lh $(BUILD_DIR)$(P_SEP)bin$(P_SEP)$(LIB_PREFIX)$(BINARY_NAME)*$(LIB_EXT) || true

windows:
	@cmake -E make_directory $(WINDOWS_BUILD_DIR)
	cmake -S . -B $(WINDOWS_BUILD_DIR) \
		$(GENERATOR) \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_PATH) \
		-DCMAKE_BUILD_TYPE=Release
	cmake --build $(WINDOWS_BUILD_DIR)

clean:
	$(CLEAN_CMD)

.PHONY: all setup build run windows clean