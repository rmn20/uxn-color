#include "../uxn.h"
#include "screen.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

Screen screen;

static Uint8 blending[5][16] = {
	{0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
	{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
	{1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
	{2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2},
	{1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}};

static Uint8 font[][8] = {
	{0x00, 0x7c, 0x82, 0x82, 0x82, 0x82, 0x82, 0x7c},
	{0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
	{0x00, 0x7c, 0x82, 0x02, 0x7c, 0x80, 0x80, 0xfe},
	{0x00, 0x7c, 0x82, 0x02, 0x1c, 0x02, 0x82, 0x7c},
	{0x00, 0x0c, 0x14, 0x24, 0x44, 0x84, 0xfe, 0x04},
	{0x00, 0xfe, 0x80, 0x80, 0x7c, 0x02, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x80, 0xfc, 0x82, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x02, 0x1e, 0x02, 0x02, 0x02},
	{0x00, 0x7c, 0x82, 0x82, 0x7c, 0x82, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x82, 0x7e, 0x02, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x02, 0x7e, 0x82, 0x82, 0x7e},
	{0x00, 0xfc, 0x82, 0x82, 0xfc, 0x82, 0x82, 0xfc},
	{0x00, 0x7c, 0x82, 0x80, 0x80, 0x80, 0x82, 0x7c},
	{0x00, 0xfc, 0x82, 0x82, 0x82, 0x82, 0x82, 0xfc},
	{0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x82, 0x7c},
	{0x00, 0x7c, 0x82, 0x80, 0xf0, 0x80, 0x80, 0x80}};

static void
screen_write(Screen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 color)
{
	if(x < p->width && y < p->height) {
		Uint32 i = x + y * p->width;
		Uint8 prev = layer->pixels[i];
		if(color != prev) {
			layer->pixels[i] = color;
			layer->changed = 1;
		}
	}
}

static void
screen_blit(Screen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy, Uint8 twobpp)
{
	int v, h, opaque = blending[4][color];
	for(v = 0; v < 8; ++v) {
		Uint16 c = sprite[v] | (twobpp ? sprite[v + 8] : 0) << 8;
		for(h = 7; h >= 0; --h, c >>= 1) {
			Uint8 ch = (c & 1) | ((c >> 7) & 2);
			if(opaque || ch)
				screen_write(p,
					layer,
					x + (flipx ? 7 - h : h),
					y + (flipy ? 7 - v : v),
					blending[ch][color]);
		}
	}
}

void
screen_palette(Screen *p, Uint8 *addr)
{
	int i, shift;
	for(i = 0, shift = 4; i < 4; ++i, shift ^= 4) {
		Uint8
			r = (addr[0 + i / 2] >> shift) & 0x0f,
			g = (addr[2 + i / 2] >> shift) & 0x0f,
			b = (addr[4 + i / 2] >> shift) & 0x0f;
		p->palette[i] = 0x0f000000 | r << 16 | g << 8 | b;
		p->palette[i] |= p->palette[i] << 4;
	}
	p->fg.changed = p->bg.changed = 1;
}

void
screen_resize(Screen *p, Uint16 width, Uint16 height)
{
	Uint8
		*bg = realloc(p->bg.pixels, width * height),
		*fg = realloc(p->fg.pixels, width * height);
	Uint32
		*pixels = realloc(p->pixels, width * height * sizeof(Uint32));
	if(bg) p->bg.pixels = bg;
	if(fg) p->fg.pixels = fg;
	if(pixels) p->pixels = pixels;
	if(bg && fg && pixels) {
		p->width = width;
		p->height = height;
		p->pixels = pixels;
		screen_clear(p, &p->bg);
		screen_clear(p, &p->fg);
	}
}

void
screen_clear(Screen *p, Layer *layer)
{
	Uint32 i, size = p->width * p->height;
	for(i = 0; i < size; ++i)
		layer->pixels[i] = 0x00;
	layer->changed = 1;
}

void
screen_redraw(Screen *p, Uint32 *pixels)
{
	Uint32 i, size = p->width * p->height, palette[16];
	for(i = 0; i < 16; ++i)
		palette[i] = p->palette[(i >> 2) ? (i >> 2) : (i & 3)];
	for(i = 0; i < size; ++i)
		pixels[i] = palette[p->fg.pixels[i] << 2 | p->bg.pixels[i]];
	p->fg.changed = p->bg.changed = 0;
}

void
screen_debug(Screen *p, Uint8 *stack, Uint8 wptr, Uint8 rptr, Uint8 *memory)
{
	Uint8 i, x, y, b;
	for(i = 0; i < 0x20; ++i) {
		x = ((i % 8) * 3 + 1) * 8, y = (i / 8 + 1) * 8, b = stack[i];
		/* working stack */
		screen_blit(p, &p->fg, x, y, font[(b >> 4) & 0xf], 1 + (wptr == i) * 0x7, 0, 0, 0);
		screen_blit(p, &p->fg, x + 8, y, font[b & 0xf], 1 + (wptr == i) * 0x7, 0, 0, 0);
		y = 0x28 + (i / 8 + 1) * 8;
		b = memory[i];
		/* return stack */
		screen_blit(p, &p->fg, x, y, font[(b >> 4) & 0xf], 3, 0, 0, 0);
		screen_blit(p, &p->fg, x + 8, y, font[b & 0xf], 3, 0, 0, 0);
	}
	/* return pointer */
	screen_blit(p, &p->fg, 0x8, y + 0x10, font[(rptr >> 4) & 0xf], 0x2, 0, 0, 0);
	screen_blit(p, &p->fg, 0x10, y + 0x10, font[rptr & 0xf], 0x2, 0, 0, 0);
	/* guides */
	for(x = 0; x < 0x10; ++x) {
		screen_write(p, &p->fg, x, p->height / 2, 2);
		screen_write(p, &p->fg, p->width - x, p->height / 2, 2);
		screen_write(p, &p->fg, p->width / 2, p->height - x, 2);
		screen_write(p, &p->fg, p->width / 2, x, 2);
		screen_write(p, &p->fg, p->width / 2 - 0x10 / 2 + x, p->height / 2, 2);
		screen_write(p, &p->fg, p->width / 2, p->height / 2 - 0x10 / 2 + x, 2);
	}
}

/* APIs */

Uint8
screen_dei(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2: return screen.width >> 8;
	case 0x3: return screen.width;
	case 0x4: return screen.height >> 8;
	case 0x5: return screen.height;
	default: return d->dat[port];
	}
}

void
screen_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x1: d->vector = peek16(d->dat, 0x0); break;
	case 0x5:
		/* TODO: if(!FIXED_SIZE) set_size(peek16(d->dat, 0x2), peek16(d->dat, 0x4), 1); */
		break;
	case 0xe: {
		Uint16 x = peek16(d->dat, 0x8);
		Uint16 y = peek16(d->dat, 0xa);
		Uint8 layer = d->dat[0xe] & 0x40;
		screen_write(&screen, layer ? &screen.fg : &screen.bg, x, y, d->dat[0xe] & 0x3);
		if(d->dat[0x6] & 0x01) poke16(d->dat, 0x8, x + 1); /* auto x+1 */
		if(d->dat[0x6] & 0x02) poke16(d->dat, 0xa, y + 1); /* auto y+1 */
		break;
	}
	case 0xf: {
		Uint16 x = peek16(d->dat, 0x8);
		Uint16 y = peek16(d->dat, 0xa);
		Layer *layer = (d->dat[0xf] & 0x40) ? &screen.fg : &screen.bg;
		Uint8 *addr = &d->mem[peek16(d->dat, 0xc)];
		Uint8 twobpp = !!(d->dat[0xf] & 0x80);
		screen_blit(&screen, layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] & 0x10, d->dat[0xf] & 0x20, twobpp);
		if(d->dat[0x6] & 0x04) poke16(d->dat, 0xc, peek16(d->dat, 0xc) + 8 + twobpp * 8); /* auto addr+8 / auto addr+16 */
		if(d->dat[0x6] & 0x01) poke16(d->dat, 0x8, x + 8);                                /* auto x+8 */
		if(d->dat[0x6] & 0x02) poke16(d->dat, 0xa, y + 8);                                /* auto y+8 */
		break;
	}
	}
}