#ifndef SOUND_H
#define SOUND_H

/* Navigation click, reusing the click Onion already ships
 * (/mnt/SDCARD/miyoo/app/sound/change.wav) so nothing extra is bundled and
 * it matches the rest of the system.
 *
 * Every step degrades to silence rather than failing: if the audio device
 * can't be opened, the wav is missing, or a conversion fails, sound_click()
 * simply does nothing and the app is otherwise unaffected. */
void sound_init(void);

/* Plays the click. Cheap and safe to call on every keypress. */
void sound_click(void);

#endif
