#include <stdio.h>

#include "../uxn.h"
#include "debug.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static void
inspect(Stack *s, char *name)
{
	Uint8 x, y;
	fprintf(stderr, "\n%s\n", name);
	for(y = 0; y < 0x04; y++) {
		for(x = 0; x < 0x08; x++) {
			Uint8 p = y * 0x08 + x;
			fprintf(stderr,
				p == s->ptr ? "[%02x]" : " %02x ",
				s->dat[p]);
		}
		fprintf(stderr, "\n");
	}
}

/* IO */

Uint8
debug_dei(Device *d, Uint8 port)
{
	DebugDevice *debug = (DebugDevice *)d;
	return d->dat[port];
}

void
debug_deo(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
	inspect(&d->u->wst, "Working-stack");
	inspect(&d->u->rst, "Return-stack");
}
