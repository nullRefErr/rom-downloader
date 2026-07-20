#!/bin/sh
cd $(dirname "$0")

export HOME=$(dirname "$0")
export LD_LIBRARY_PATH=/mnt/SDCARD/.tmp_update/lib/parasyte:$LD_LIBRARY_PATH
export SDL_VIDEODRIVER=mmiyoo
export SDL_AUDIODRIVER=mmiyoo
export EGL_VIDEODRIVER=mmiyoo

./romdownloader 2>>crash.txt
