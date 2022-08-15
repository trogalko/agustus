#ifndef SOUND_MUSIC_H
#define SOUND_MUSIC_H

void sound_music_set_volume(int percentage);

void sound_music_play_intro(void);

void sound_music_play_editor(void);

void sound_music_update(int force);

void sound_music_stop(void);

#endif // SOUND_MUSIC_H
