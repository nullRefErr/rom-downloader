#!/bin/sh
cd $(dirname "$0")

export HOME=$(dirname "$0")
export LD_LIBRARY_PATH=/mnt/SDCARD/.tmp_update/lib/parasyte:$LD_LIBRARY_PATH
export SDL_VIDEODRIVER=mmiyoo
export SDL_AUDIODRIVER=mmiyoo
export EGL_VIDEODRIVER=mmiyoo

# Onion starts an audioserver daemon before every app launch and it holds
# /dev/mi_ao open, so SDL cannot open the audio device while it is running.
# Every audio-using Onion app stops it first. Deliberately a narrow killall
# rather than sourcing Onion's stop_audioserver.sh, which also bounces
# wpa_supplicant/udhcpc — restarting Wi-Fi inside a downloader would be a
# worse bug than the one being fixed.
killall -9 audioserver audioserver.mod 2>/dev/null

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
