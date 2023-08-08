#include <stdio.h>
#include <stdlib.h>

#include "../uxn.h"
#include "system.h"

/*
Copyright (c) 2022-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static const char *errors[] = {
	"underflow",
	"overflow",
	"division by zero"};

static void
system_print(Stack *s, char *name)
{
	Uint8 i;
	fprintf(stderr, "<%s>", name);
	for(i = 0; i < s->ptr; i++)
		fprintf(stderr, " %02x", s->dat[i]);
	if(!i)
		fprintf(stderr, " empty");
	fprintf(stderr, "\n");
}

static void
system_cmd(Uint8 *ram, Uint16 addr)
{
	if(ram[addr] == 0x1) {
		Uint16 i, length = PEEK2(ram + addr + 1);
		Uint16 a_page = PEEK2(ram + addr + 1 + 2), a_addr = PEEK2(ram + addr + 1 + 4);
		Uint16 b_page = PEEK2(ram + addr + 1 + 6), b_addr = PEEK2(ram + addr + 1 + 8);
		int src = (a_page % RAM_PAGES) * 0x10000, dst = (b_page % RAM_PAGES) * 0x10000;
		for(i = 0; i < length; i++)
			ram[dst + (Uint16)(b_addr + i)] = ram[src + (Uint16)(a_addr + i)];
	}
}

int
system_error(char *msg, const char *err)
{
	fprintf(stderr, "%s: %s\n", msg, err);
	fflush(stderr);
	return 0;
}

int
system_load(Uxn *u, char *filename)
{
	int l, i = 0;
	FILE *f = fopen(filename, "rb");
	if(!f)
		return 0;
	l = fread(&u->ram[PAGE_PROGRAM], 0x10000 - PAGE_PROGRAM, 1, f);
	while(l && ++i < RAM_PAGES)
		l = fread(u->ram + 0x10000 * i, 0x10000, 1, f);
	fclose(f);
	return 1;
}

void
system_inspect(Uxn *u)
{
	system_print(&u->wst, "wst");
	system_print(&u->rst, "rst");
}

void
system_connect(Uint8 device, Uint8 ver, Uint16 dei, Uint16 deo)
{
	dev_vers[device] = ver;
	dei_mask[device] = dei;
	deo_mask[device] = deo;
}

int
system_version(char *name, char *date)
{
	int i;
	printf("%s, %s.\n", name, date);
	printf("Device Version Dei  Deo\n");
	for(i = 0; i < 0x10; i++)
		if(dev_vers[i])
			printf("%6x %7d %04x %04x\n", i, dev_vers[i], dei_mask[i], deo_mask[i]);
	return 0;
}

/* IO */

void
system_deo(Uxn *u, Uint8 *d, Uint8 port)
{
	switch(port) {
	case 0x3:
		system_cmd(u->ram, PEEK2(d + 2));
		break;
	case 0x5:
		if(PEEK2(d + 4)) {
			Uxn friend;
			uxn_boot(&friend, u->ram);
			uxn_eval(&friend, PEEK2(d + 4));
		}
		break;
	case 0xe:
		system_inspect(u);
		break;
	}
}

/* Errors */

int
emu_halt(Uxn *u, Uint8 instr, Uint8 err, Uint16 addr)
{
	Uint8 *d = &u->dev[0];
	Uint16 handler = PEEK2(d);
	if(handler) {
		u->wst.ptr = 4;
		u->wst.dat[0] = addr >> 0x8;
		u->wst.dat[1] = addr & 0xff;
		u->wst.dat[2] = instr;
		u->wst.dat[3] = err;
		return uxn_eval(u, handler);
	} else {
		system_inspect(u);
		fprintf(stderr, "%s %s, by %02x at 0x%04x.\n", (instr & 0x40) ? "Return-stack" : "Working-stack", errors[err - 1], instr, addr);
	}
	return 0;
}
