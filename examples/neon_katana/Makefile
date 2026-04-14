APP_NAME = NeonKatana

# OS & Paths
ifeq ($(OS),Windows_NT)
    PLATFORM_DIR = windows
    P_SEP = \\
    CLEAN_CMD = rmdir /s /q build 2>nul || exit 0
else
    PLATFORM_DIR = $(shell uname -s | tr '[:upper:]' '[:lower:]')
    P_SEP = /
    CLEAN_CMD = rm -rf build
endif

BUILD_TEMP = build$(P_SEP)$(PLATFORM_DIR)

# TypeScript Config
SCRIPTS_OUT = $(BUILD_TEMP)$(P_SEP)transpiled

all: build

setup:
	cmake -S . -B $(BUILD_TEMP) -DCMAKE_BUILD_TYPE=Debug -DSCRIPTS_DIR="$(abspath $(SCRIPTS_OUT))"

# TypeScript must be compiled before setup so CMake can glob the .js files
build: typescript setup
	cmake --build $(BUILD_TEMP)
	@echo "Build complete. Executable & Assets deployed."

typescript:
	@echo "Compiling TypeScript..."
	@mkdir -p $(SCRIPTS_OUT)
	npx tsc --project tsconfig.json --outDir $(SCRIPTS_OUT)

run: build
	.$(P_SEP)$(BUILD_TEMP)$(P_SEP)$(APP_NAME)

clean:
	$(CLEAN_CMD)

.PHONY: all setup build run clean typescript