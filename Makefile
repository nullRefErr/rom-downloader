APP_DIR := package/App/RomDownloader
BUILD_DIR := build
SRC := $(wildcard src/*.c) $(wildcard third_party/fonts/*.c) third_party/cjson/cJSON.c $(shell find third_party/lvgl/src -name '*.c' -not -path '*/drivers/*')
INC := -Isrc -Ithird_party/lvgl -Ithird_party -Ithird_party/cjson

.PHONY: host device clean

# Native build on the dev machine for fast iteration (uses system SDL2).
host: $(BUILD_DIR)/romdownloader-host
$(BUILD_DIR)/romdownloader-host: $(SRC)
	mkdir -p $(BUILD_DIR)
	cc -Wall -Wextra -O0 -g $(INC) $(shell pkg-config --cflags sdl2) $(SRC) \
		$(shell pkg-config --libs sdl2) -lm -o $@

# Cross-compiled ARM binary for the Miyoo Mini Plus, built inside the
# union-miyoomini-toolchain Docker image (see toolchain/build-device.sh),
# dropped straight into the Onion app package.
device: $(APP_DIR)/romdownloader
$(APP_DIR)/romdownloader: $(SRC)
	./toolchain/build-device.sh

clean:
	rm -rf $(BUILD_DIR) $(APP_DIR)/romdownloader
