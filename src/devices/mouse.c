#include "../uxn.h"
#include "mouse.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

void
mouse_xy(Device *d, Uint16 x, Uint16 y)
{
	poke16(d->dat, 0x2, x);
	poke16(d->dat, 0x4, y);
	uxn_eval(d->u, d->vector);
}

void
mouse_z(Device *d, Uint8 z)
{
	d->dat[7] = z;
	uxn_eval(d->u, d->vector);
	d->dat[7] = 0x00;
}

void
mouse_down(Device *d, Uint8 mask)
{
	d->dat[6] |= mask;
	uxn_eval(d->u, d->vector);
}

void
mouse_up(Device *d, Uint8 mask)
{
	d->dat[6] &= (~mask);
	uxn_eval(d->u, d->vector);
}