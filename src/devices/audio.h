/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

typedef signed int Sint32;

#define SAMPLE_FREQUENCY 44100
#define POLYPHONY 4

#define DEVPEEK16(o, x) \
	{ \
		(o) = (d->dat[(x)] << 8) + d->dat[(x) + 1]; \
	}
#define DEVPOKE16(x, y) \
	{ \
		d->dat[(x)] = (y) >> 8; \
		d->dat[(x) + 1] = (y); \
	}

typedef struct Device {
	struct Uxn *u;
	Uint8 dat[16];
	Uint8 (*dei)(struct Device *d, Uint8);
	void (*deo)(struct Device *d, Uint8);
} Device;

Uint8 audio_get_vu(int instance);
Uint16 audio_get_position(int instance);
int audio_render(int instance, Sint16 *sample, Sint16 *end);
void audio_start(int instance, Device *d);
void audio_finished_handler(int instance);
