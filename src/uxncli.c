#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "uxn.h"

Uint8 *supervisor_memory, *memory;

#include "devices/system.h"
#include "devices/file.h"
#include "devices/datetime.h"

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#pragma mark - Core

static Device *devsystem, *devconsole;

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

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

#pragma mark - Devices

void
system_deo_special(Device *d, Uint8 port)
{
	if(port == 0xe) {
		inspect(d->u->wst, "Working-stack");
		inspect(d->u->rst, "Return-stack");
	}
}

static void
console_deo(Device *d, Uint8 port)
{
	if(port > 0x7)
		write(port - 0x7, (char *)&d->dat[port], 1);
}

static Uint8
nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void
nil_deo(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
}

#pragma mark - Generics

static int
console_input(Uxn *u, char c)
{
	devconsole->dat[0x2] = c;
	return uxn_eval(u, GETVECTOR(devconsole));
}

static void
run(Uxn *u)
{
	Uint16 vec;
	Device *d = devconsole;
	while((!u->dev[0].dat[0xf]) && (read(0, &devconsole->dat[0x2], 1) > 0)) {
		DEVPEEK16(vec, 0);
		uxn_eval(u, vec);
	}
}

static int
load(Uxn *u, char *filepath)
{
	FILE *f;
	int r;
	if(!(f = fopen(filepath, "rb"))) return 0;
	r = fread(u->ram + PAGE_PROGRAM, 1, 0x10000 - PAGE_PROGRAM, f);
	fclose(f);
	if(r < 1) return 0;
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
}

int
main(int argc, char **argv)
{
	Uxn u;
	int i, loaded = 0;

	supervisor_memory = (Uint8 *)calloc(0x10000, sizeof(Uint8));
	memory = (Uint8 *)calloc(0x10000, sizeof(Uint8));
	if(!uxn_boot(&u, memory, supervisor_memory + PAGE_DEV, (Stack *)(supervisor_memory + PAGE_WST), (Stack *)(supervisor_memory + PAGE_RST)))
		return error("Boot", "Failed");

	/* system   */ devsystem = uxn_port(&u, 0x0, system_dei, system_deo);
	/* console  */ devconsole = uxn_port(&u, 0x1, nil_dei, console_deo);
	/* empty    */ uxn_port(&u, 0x2, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x7, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x9, nil_dei, nil_deo);
	/* file     */ uxn_port(&u, 0xa, nil_dei, file_deo);
	/* datetime */ uxn_port(&u, 0xb, datetime_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xc, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xf, nil_dei, nil_deo);

	for(i = 1; i < argc; i++) {
		if(!loaded++) {
			if(!load(&u, argv[i]))
				return error("Load", "Failed");
			if(!uxn_eval(&u, PAGE_PROGRAM))
				return error("Init", "Failed");
		} else {
			char *p = argv[i];
			while(*p) console_input(&u, *p++);
			console_input(&u, '\n');
		}
	}
	if(!loaded)
		return error("Input", "Missing");

	run(&u);

	return 0;
}
