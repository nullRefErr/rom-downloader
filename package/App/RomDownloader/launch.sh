#!/bin/sh
cd $(dirname "$0")

export HOME=$(dirname "$0")
export LD_LIBRARY_PATH=/mnt/SDCARD/.tmp_update/lib/parasyte:$LD_LIBRARY_PATH
export SDL_VIDEODRIVER=mmiyoo
export SDL_AUDIODRIVER=mmiyoo
export EGL_VIDEODRIVER=mmiyoo

# Self-update swap loop. The app downloads a new build to romdownloader.new
# and exits (it can't replace itself in place — the SD card is FAT32, which
# has no inode indirection, so renaming over the running executable fails).
# Here, before running, nothing is executing romdownloader, so the swap is
# safe. If the app stages another update on exit, loop and swap again;
# otherwise a normal exit breaks out.
while : ; do
    if [ -f romdownloader.new ]; then
        mv -f romdownloader.new romdownloader
        chmod +x romdownloader
    fi
    ./romdownloader 2>>crash.txt
    [ -f romdownloader.new ] || break
done
