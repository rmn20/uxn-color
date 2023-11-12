/*
Copyright (c) 2021 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define SCREEN_VERSION 1
#define SCREEN_DEIMASK 0x003c
#define SCREEN_DEOMASK 0xc028
#define SCREEN2_DEIMASK 0x0000
#define SCREEN2_DEOMASK 0x0007

typedef struct UxnScreen {
	int width, height, x1, y1, x2, y2;
	int workPalette; /*0 - 7*/
	int workPaletteLayer; /*0 - 1*/
	Uint32 palette[64], *pixels;
	Uint8 *fg, *bg;
} UxnScreen;

extern UxnScreen uxn_screen;
extern int emu_resize(int width, int height);

void screen_fill(Uint8 *layer, int color);
void screen_rect(Uint8 *layer, int x1, int y1, int x2, int y2, int color);
void screen_palette(Uint8 *addr, Uint8 palId);
void screen_resize(Uint16 width, Uint16 height);
void screen_change(Uint16 x1, Uint16 y1, Uint16 x2, Uint16 y2);
void screen_redraw(Uxn *u);

Uint8 screen_dei(Uxn *u, Uint8 addr);
void screen_deo(Uint8 *ram, Uint8 *d, Uint8 port);
void screen2_deo(Uint8 *ram, Uint8 *d, Uint8 port);
