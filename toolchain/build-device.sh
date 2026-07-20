#!/bin/sh
# Cross-compiles src/*.c + vendored LVGL into
# package/App/RomDownloader/romdownloader using the
# shauninman/union-miyoomini-toolchain Docker image.
#
# One-time setup (not run by this script):
#   git clone https://github.com/shauninman/union-miyoomini-toolchain.git
#   cd union-miyoomini-toolchain && docker build -t miyoomini-toolchain .
set -e
cd "$(dirname "$0")/.."

GCC=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc
STRIP=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-strip

docker run --rm -v "$(pwd)":/root/workspace -w /root/workspace miyoomini-toolchain /bin/bash -c "
	set -e
	SRC=\"\$(find src -name '*.c') third_party/cjson/cJSON.c \$(find third_party/lvgl/src -name '*.c' -not -path '*/drivers/*')\"
	$GCC -Wall -Wextra -O2 \
		-Isrc -Ithird_party/lvgl -Ithird_party -Ithird_party/cjson \
		-I third_party/SDL2-headers/include \
		\$SRC \
		third_party/parasyte-libSDL2-2.0.so.0 \
		-Wl,--allow-shlib-undefined \
		-lm -lpthread -ldl -lrt \
		-o package/App/RomDownloader/romdownloader
	$STRIP package/App/RomDownloader/romdownloader
"
echo "built package/App/RomDownloader/romdownloader"
file package/App/RomDownloader/romdownloader 2>/dev/null || true
