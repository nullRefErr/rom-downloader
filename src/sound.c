#include "sound.h"
#include <SDL2/SDL.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define CLICK_WAV "/mnt/SDCARD/miyoo/app/sound/change.wav"

/* This device's SDL2 audio backend paces itself with
 *   usleep((samples * 1000 / rate - 10) * 1000)
 * in UNSIGNED arithmetic, so any buffer shorter than 10ms of audio
 * underflows into a ~71 minute sleep and wedges the audio thread. At 44100
 * the smallest safe size is 486 samples; 512 is the next power of two and
 * yields a 1ms sleep. Do not lower this. */
#define AUDIO_RATE    44100
#define AUDIO_SAMPLES 512

static SDL_AudioDeviceID g_dev;
static Uint8 *g_click;
static Uint32 g_click_len;

/* Opening the audio device makes the driver force the hardware mixer to 0dB,
 * overriding whatever volume the user set — and since that's the shared
 * output device, it would stay wrong for whatever they launch next. Save the
 * level first and put it back afterwards. Best effort: if any of it fails
 * the app just carries on, and the ioctl layer simply isn't there when this
 * runs anywhere other than the handheld. */
#define MI_AO_SETVOLUME 0x4008690b
#define MI_AO_GETVOLUME 0xc008690c

static int mi_ao_volume(int set, int value) {
    int fd = open("/dev/mi_ao", O_RDWR);
    if (fd < 0) return INT32_MIN;
    int payload[2] = {0, value};
    uint64_t req[2] = {(uint64_t)sizeof(payload), (uint64_t)(uintptr_t)payload};
    int rc = ioctl(fd, set ? MI_AO_SETVOLUME : MI_AO_GETVOLUME, req);
    close(fd);
    if (rc < 0) return INT32_MIN;
    return payload[1];
}

void sound_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;

    SDL_AudioSpec wav_spec;
    Uint8 *wav_buf = NULL;
    Uint32 wav_len = 0;
    if (!SDL_LoadWAV(CLICK_WAV, &wav_spec, &wav_buf, &wav_len)) return;

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = AUDIO_RATE;
    want.format = AUDIO_S16SYS; /* the driver is 16-bit only */
    want.channels = 2;          /* stereo is the path every emulator here uses */
    want.samples = AUDIO_SAMPLES;
    want.callback = NULL;       /* queue-based; no callback to keep in sync */

    int saved_db = mi_ao_volume(0, 0);

    /* allowed_changes 0: this backend never renegotiates, so accepting a
     * different spec silently would just produce garbage. */
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);

    if (saved_db != INT32_MIN) mi_ao_volume(1, saved_db);

    if (g_dev == 0) {
        SDL_FreeWAV(wav_buf);
        return;
    }

    /* Convert once at startup rather than per click. The shipped click is
     * mono, and a theme may replace it with a different rate or channel
     * count, which is why this goes through the CVT instead of a hand-rolled
     * mono-to-stereo copy. */
    SDL_AudioCVT cvt;
    int need = SDL_BuildAudioCVT(&cvt, wav_spec.format, wav_spec.channels, wav_spec.freq,
                                 want.format, want.channels, want.freq);
    if (need < 0) {
        SDL_FreeWAV(wav_buf);
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
        return;
    }

    if (need == 0) {
        g_click = SDL_malloc(wav_len);
        if (g_click) {
            SDL_memcpy(g_click, wav_buf, wav_len);
            g_click_len = wav_len;
        }
        SDL_FreeWAV(wav_buf);
    } else {
        cvt.len = (int)wav_len;
        cvt.buf = SDL_malloc((size_t)cvt.len * cvt.len_mult);
        if (!cvt.buf) {
            SDL_FreeWAV(wav_buf);
            SDL_CloseAudioDevice(g_dev);
            g_dev = 0;
            return;
        }
        SDL_memcpy(cvt.buf, wav_buf, wav_len);
        SDL_FreeWAV(wav_buf);
        if (SDL_ConvertAudio(&cvt) != 0) {
            SDL_free(cvt.buf);
            SDL_CloseAudioDevice(g_dev);
            g_dev = 0;
            return;
        }
        g_click = cvt.buf;
        g_click_len = (Uint32)cvt.len_cvt;
    }

    if (!g_click || g_click_len == 0) {
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
        return;
    }

    SDL_PauseAudioDevice(g_dev, 0);
}

void sound_click(void) {
    if (!g_dev || !g_click) return;
    /* A held d-pad repeats faster than the ~70ms clip, and without this the
     * queue backs up and the click smears into a drone. */
    if (SDL_GetQueuedAudioSize(g_dev) >= g_click_len) return;
    SDL_QueueAudio(g_dev, g_click, g_click_len);
}
