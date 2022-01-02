#include "uxn.h"

/*
Copyright (u) 2021 Devine Lu Linvega
Copyright (u) 2021 Andrew Alderwick
Copyright (u) 2022 Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/* clang-format off */

#define PUSH8(s, v) { if(s->ptr == 0xff) { errcode = 2; goto err; } s->dat[s->ptr++] = (v); }
#define PUSH16(s, v) { if((tmp = s->ptr) >= 0xfe) { errcode = 2; goto err; } s->dat[tmp] = (v) >> 8; s->dat[tmp + 1] = (v); s->ptr = tmp + 2; }
#define PUSH(s, v) { if(bs) { PUSH16(s, (v)) } else { PUSH8(s, (v)) } }
#define POP8(o) { if(*pptr == 0) { errcode = 1; goto err; } o = (Uint16)src->dat[--(*pptr)]; }
#define POP16(o) { if((tmp = *pptr) <= 1) { errcode = 1; goto err; } o = src->dat[tmp - 1]; o += src->dat[tmp - 2] << 8; *pptr = tmp - 2; }
#define POP(o) { if(bs) { POP16(o) } else { POP8(o) } }
#define POKE(x, y) { if(bs) { u->ram.dat[(x)] = (y) >> 8; u->ram.dat[(x) + 1] = (y); } else { u->ram.dat[(x)] = y; }}
#define PEEK16(o, x) { o = (u->ram.dat[(x)] << 8) + u->ram.dat[(x) + 1]; }
#define PEEK(o, x) { if(bs) { PEEK16(o, x) } else { o = u->ram.dat[(x)]; }}
#define DEVR(o, d, v) { dev = (d); o = dev->dei(dev, (v) & 0x0f); if(bs) { o = (o << 8) + dev->dei(dev, ((v) + 1) & 0x0f); } }
#define DEVW8(x, y) { dev->dat[(x) & 0xf] = y; dev->deo(dev, (x) & 0x0f); }
#define DEVW(d, x, y) { dev = (d); if(bs) { DEVW8((x), (y) >> 8); DEVW8((x) + 1, (y)); } else { DEVW8((x), (y)) } }
#define WARP(x) { if(bs) u->ram.ptr = (x); else u->ram.ptr += (Sint8)(x); }

void   poke16(Uint8 *m, Uint16 a, Uint16 b) { m[a] = b >> 8; m[a + 1] = b; }
Uint16 peek16(Uint8 *m, Uint16 a) { Uint16 r = m[a] << 8; return r + m[a + 1]; }

int
uxn_eval(Uxn *u, Uint16 vec)
{
	Uint8 instr, errcode, kptr, *pptr;
	Uint16 a,b,c, discard;
	int bs, tmp;
	Device *dev;
	if(!vec || u->dev[0].dat[0xf])
		return 0;
	u->ram.ptr = vec;
	if(u->wst.ptr > 0xf8) u->wst.ptr = 0xf8;
	while((instr = u->ram.dat[u->ram.ptr++])) {
		Stack *src, *dst;
		/* Return Mode */
		if(instr & 0x40) {
			src = &u->rst; dst = &u->wst;
		} else {
			src = &u->wst; dst = &u->rst;
		}
		/* Keep Mode */
		if(instr & 0x80) {
			kptr = src->ptr;
			pptr = &kptr;
		} else {
			pptr = &src->ptr;
		}
		/* Short Mode */
		bs = instr & 0x20 ? 1 : 0;
		switch(instr & 0x1f){
			/* Stack */
			case 0x00: /* LIT */ if(bs) { PEEK16(a, u->ram.ptr); PUSH16(src, a); u->ram.ptr += 2; }
			                     else   { a = u->ram.dat[u->ram.ptr++]; PUSH8(src, a); } break;
			case 0x01: /* INC */ POP(a); PUSH(src, a + 1); break;
			case 0x02: /* POP */ POP(discard); break;
			case 0x03: /* DUP */ POP(a); PUSH(src, a); PUSH(src, a); break;
			case 0x04: /* NIP */ POP(a); POP(discard); PUSH(src, a); break;
			case 0x05: /* SWP */ POP(a); POP(b); PUSH(src, a); PUSH(src, b); break;
			case 0x06: /* OVR */ POP(a); POP(b); PUSH(src, b); PUSH(src, a); PUSH(src, b); break;
			case 0x07: /* ROT */ POP(a); POP(b); POP(c); PUSH(src, b); PUSH(src, a); PUSH(src, c); break;
			/* Logic */
			case 0x08: /* EQU */ POP(a); POP(b); PUSH8(src, b == a); break;
			case 0x09: /* NEQ */ POP(a); POP(b); PUSH8(src, b != a); break;
			case 0x0a: /* GTH */ POP(a); POP(b); PUSH8(src, b > a); break;
			case 0x0b: /* LTH */ POP(a); POP(b); PUSH8(src, b < a); break;
			case 0x0c: /* JMP */ POP(a); WARP(a); break;
			case 0x0d: /* JCN */ POP(a); POP8(b); if(b) WARP(a); break;
			case 0x0e: /* JSR */ POP(a); PUSH16(dst, u->ram.ptr); WARP(a); break;
			case 0x0f: /* STH */ POP(a); PUSH(dst, a); break;
			/* Memory */
			case 0x10: /* LDZ */ POP8(a); PEEK(b, a); PUSH(src, b); break;
			case 0x11: /* STZ */ POP8(a); POP(b); POKE(a, b); break;
			case 0x12: /* LDR */ POP8(a); PEEK(b, u->ram.ptr + (Sint8)a); PUSH(src, b); break;
			case 0x13: /* STR */ POP8(a); POP(b); c = u->ram.ptr + (Sint8)a; POKE(c, b); break;
			case 0x14: /* LDA */ POP16(a); PEEK(b, a); PUSH(src, b); break;
			case 0x15: /* STA */ POP16(a); POP(b); POKE(a, b); break;
			case 0x16: /* DEI */ POP8(a); DEVR(b, &u->dev[a >> 4], a); PUSH(src, b); break;
			case 0x17: /* DEO */ POP8(a); POP(b); DEVW(&u->dev[a >> 4], a, b); break;
			/* Arithmetic */
			case 0x18: /* ADD */ POP(a); POP(b); PUSH(src, b + a); break;
			case 0x19: /* SUB */ POP(a); POP(b); PUSH(src, b - a); break;
			case 0x1a: /* MUL */ POP(a); POP(b); PUSH(src, (Uint32)b * a); break;
			case 0x1b: /* DIV */ POP(a); POP(b); if(a == 0) { errcode = 3; goto err; } PUSH(src, b / a); break;
			case 0x1c: /* AND */ POP(a); POP(b); PUSH(src, b & a); break;
			case 0x1d: /* ORA */ POP(a); POP(b); PUSH(src, b | a); break;
			case 0x1e: /* EOR */ POP(a); POP(b); PUSH(src, b ^ a); break;
			case 0x1f: /* SFT */ POP8(a); POP(b); c = b >> (a & 0x0f) << ((a & 0xf0) >> 4); PUSH(src, c); break;
		}
	}
	return 1;

err:
	return uxn_halt(u, errcode, "Stack", instr);
}

/* clang-format on */

int
uxn_boot(Uxn *u, Uint8 *memory)
{
	Uint32 i;
	char *cptr = (char *)u;
	for(i = 0; i < sizeof(*u); ++i)
		cptr[i] = 0x00;
	u->ram.dat = memory;
	return 1;
}

Device *
uxn_port(Uxn *u, Uint8 id, Uint8 (*deifn)(Device *d, Uint8 port), void (*deofn)(Device *d, Uint8 port))
{
	Device *d = &u->dev[id];
	d->addr = id * 0x10;
	d->u = u;
	d->mem = u->ram.dat;
	d->dei = deifn;
	d->deo = deofn;
	return d;
}
