#include "uxn.h"

/*
Copyright (u) 2022-2023 Devine Lu Linvega, Andrew Alderwick, Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/* Registers

[ . ][ . ][ . ][ L ][ N ][ T ] <
[ . ][ . ][ . ][   H2   ][ T ] <
[   L2   ][   N2   ][   T2   ] <

*/

#define T s->dat[s->ptr-1]
#define N s->dat[s->ptr-2]
#define L s->dat[s->ptr-3]
#define H2 PEEK16(s->dat+s->ptr-3)
#define T2 PEEK16(s->dat+s->ptr-2)
#define N2 PEEK16(s->dat+s->ptr-4)
#define L2 PEEK16(s->dat+s->ptr-6)

#define HALT(c) { return uxn_halt(u, ins, (c), pc - 1); }
#define INC(mul, add) { if(mul > s->ptr) HALT(1) s->ptr += k * mul + add; if(s->ptr > 254) HALT(2) }
#define DEC(mul, sub) { if(mul > s->ptr) HALT(1) s->ptr -= !k * mul - sub; if(s->ptr > 254) HALT(2) }
#define PUT(o, v) { s->dat[s->ptr - o - 1] = (v); }
#define PUT2(o, v) { tmp = (v); s->dat[s->ptr - o - 2] = tmp >> 8; s->dat[s->ptr - o - 1] = tmp; }
#define PUSH(stack, v) { if(s->ptr > 254) HALT(2) stack->dat[stack->ptr++] = (v); }
#define PUSH2(stack, v) { if(s->ptr > 253) HALT(2) tmp = (v); stack->dat[stack->ptr] = (v) >> 8; stack->dat[stack->ptr + 1] = (v); stack->ptr += 2; }
#define SEND(a, b) { u->dev[a] = b; if((send_events[(a) >> 4] >> ((a) & 0xf)) & 0x1) u->deo(u, a); }
#define LISTEN(a, b) { PUT(a, ((receive_events[(b) >> 4] >> ((b) & 0xf)) & 0x1) ? u->dei(u, b) : u->dev[b])  }

static 
Uint16 send_events[] = {
	0x6a08, 0x0300, 0xc028, 0x8000, 0x8000, 0x8000, 0x8000, 0x0000,
	0x0000, 0x0000, 0xa260, 0xa260, 0x0000, 0x0000, 0x0000, 0x0000 };
static
Uint16 receive_events[] = {
	0x0000, 0x0000, 0x0014, 0x0014, 0x0014, 0x0014, 0x0014, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x07fd, 0x0000, 0x0000, 0x0000 };

int
uxn_eval(Uxn *u, Uint16 pc)
{
	Uint8 ins, opc, k;
	Uint16 t, n, l, tmp;
	Stack *s;
	if(!pc || u->dev[0x0f]) return 0;
	for(;;) {
		ins = u->ram[pc++];
		k = !!(ins & 0x80);
		s = ins & 0x40 ? u->rst : u->wst;
		opc = !(ins & 0x1f) ? 0 - (ins >> 5) : ins & 0x3f;
		switch(opc) {
			/* IMM */
			case 0x00: /* BRK   */ return 1;
			case 0xff: /* JCI   */ pc += !!s->dat[--s->ptr] * PEEK16(u->ram + pc) + 2; break;
			case 0xfe: /* JMI   */ pc += PEEK16(u->ram + pc) + 2; break;
			case 0xfd: /* JSI   */ PUSH2(u->rst, pc + 2); pc += PEEK16(u->ram + pc) + 2; break;
			case 0xfc: /* LIT   */ PUSH(u->wst, u->ram[pc++]); break;
			case 0xfb: /* LIT2  */ PUSH2(u->wst, PEEK16(u->ram + pc)); pc += 2; break;
			case 0xfa: /* LITr  */ PUSH(u->rst, u->ram[pc++]); break;
			case 0xf9: /* LIT2r */ PUSH2(u->rst, PEEK16(u->ram + pc)); pc += 2; break;
			/* ALU */
			case 0x21: /* INC2 */ t=T2;           INC(2, 0) PUT2(0, t + 1) break;
			case 0x01: /* INC  */ t=T;            INC(1, 0) PUT(0, t + 1); break;
			case 0x22: /* POP2 */                 DEC(2, 0) break;
			case 0x02: /* POP  */                 DEC(1, 0) break;
			case 0x23: /* NIP2 */ t=T2;           DEC(2, 0) PUT2(0, t) break;
			case 0x03: /* NIP  */ t=T;            DEC(1, 0) PUT(0, t) break;
			case 0x24: /* SWP2 */ t=T2;n=N2;      INC(4, 0) PUT2(2, t) PUT2(0, n); break;
			case 0x04: /* SWP  */ t=T;n=N;        INC(2, 0) PUT(0, n) PUT(1, t); break;
			case 0x25: /* ROT2 */ t=T2;n=N2;l=L2; INC(6, 0) PUT2(0, l) PUT2(2, t) PUT2(4, n) break;
			case 0x05: /* ROT  */ t=T;n=N;l=L;    INC(3, 0) PUT(0, l) PUT(1, t) PUT(2, n) break;
			case 0x26: /* DUP2 */ t=T2;           INC(2, 2) PUT2(0, t) PUT2(2, t) break;
			case 0x06: /* DUP  */ t=T;            INC(1, 1) PUT(0, t) PUT(1, t) break;
			case 0x27: /* OVR2 */ t=T2;n=N2;      INC(4, 2) PUT2(0, n) PUT2(2, t) PUT2(4, n) break;
			case 0x07: /* OVR  */ t=T;n=N;        INC(2, 1) PUT(0, n) PUT(1, t) PUT(2, n) break;
			case 0x28: /* EQU2 */ t=T2;n=N2;      INC(4,-3) PUT(0, n == t) break;
			case 0x08: /* EQU  */ t=T;n=N;        INC(2,-1) PUT(0, n == t) break;
			case 0x29: /* NEQ2 */ t=T2;n=N2;      INC(4,-3) PUT(0, n != t) break;
			case 0x09: /* NEQ  */ t=T;n=N;        INC(2,-1) PUT(0, n != t) break;
			case 0x2a: /* GTH2 */ t=T2;n=N2;      INC(4,-3) PUT(0, n > t)  break;
			case 0x0a: /* GTH  */ t=T;n=N;        INC(2,-1) PUT(0, n > t) break;
			case 0x2b: /* LTH2 */ t=T2;n=N2;      INC(4,-3) PUT(0, n < t)  break;
			case 0x0b: /* LTH  */ t=T;n=N;        INC(2,-1) PUT(0, n < t) break;
			case 0x2c: /* JMP2 */ t=T2;           DEC(2, 0) pc = t; break;
			case 0x0c: /* JMP  */ t=T;            DEC(1, 0) pc += (Sint8)t;  break;
			case 0x2d: /* JCN2 */ t=T2;n=L;       DEC(3, 0) if(n) { pc = t; } break;
			case 0x0d: /* JCN  */ t=T;n=N;        DEC(2, 0) pc += !!n * (Sint8)t; break;
			case 0x2e: /* JSR2 */ t=T2;           DEC(2, 0) PUSH2(u->rst, pc) pc = t; break;
			case 0x0e: /* JSR  */ t=T;            DEC(1, 0) PUSH2(u->rst, pc) pc += (Sint8)t; break;
			case 0x2f: /* STH2 */ t=T2; if(ins & 0x40) { u->rst->ptr -= !k * 2; PUSH2(u->wst, t); } else{ u->wst->ptr -= !k * 2; PUSH2(u->rst, t); } break;
			case 0x0f: /* STH  */ t=T;  if(ins & 0x40) { u->rst->ptr -= !k; PUSH(u->wst, t); } else{ u->wst->ptr -= !k; PUSH(u->rst, t); } break;
			case 0x30: /* LDZ2 */ t=T;            INC(1, 1) PUT2(0, PEEK16(u->ram + t)) break;
			case 0x10: /* LDZ  */ t=T;            INC(1, 0) PUT(0, u->ram[t]) break;
			case 0x31: /* STZ2 */ t=T;n=H2;       DEC(3, 0) POKE16(u->ram + t, n) break;
			case 0x11: /* STZ  */ t=T;n=N;        DEC(2, 0) u->ram[t] = n; break;
			case 0x32: /* LDR2 */ t=T;            INC(1, 1) PUT2(0, PEEK16(u->ram + pc + (Sint8)t)) break;
			case 0x12: /* LDR  */ t=T;            INC(1, 0) PUT(0, u->ram[pc + (Sint8)t]) break;
			case 0x33: /* STR2 */ t=T;n=H2;       DEC(3, 0) POKE16(u->ram + pc + (Sint8)t, n) break;
			case 0x13: /* STR  */ t=T;n=N;        DEC(2, 0) u->ram[pc + (Sint8)t] = n; break;
			case 0x34: /* LDA2 */ t=T2;           INC(2, 0) PUT2(0, PEEK16(u->ram + t)) break;
			case 0x14: /* LDA  */ t=T2;           INC(2,-1) PUT(0, u->ram[t]) break;
			case 0x35: /* STA2 */ t=T2;n=N2;      DEC(4, 0) POKE16(u->ram + t, n) break;
			case 0x15: /* STA  */ t=T2;n=L;       DEC(3, 0) u->ram[t] = n; break;
			case 0x36: /* DEI2 */ t=T;            INC(1, 1) LISTEN(1, t) LISTEN(0, t + 1) break;
			case 0x16: /* DEI  */ t=T;            INC(1, 0) LISTEN(0, t) break;
			case 0x37: /* DEO2 */ t=T;n=N;l=L;    DEC(3, 0) SEND(t, l) SEND(t + 1, n) break;
			case 0x17: /* DEO  */ t=T;n=N;        DEC(2, 0) SEND(t, n) break;
			case 0x38: /* ADD2 */ t=T2;n=N2;      INC(4,-2) PUT2(0, n + t) break;
			case 0x18: /* ADD  */ t=T;n=N;        INC(2,-1) PUT(0, n + t) break;
			case 0x39: /* SUB2 */ t=T2;n=N2;      INC(4,-2) PUT2(0, n - t) break;
			case 0x19: /* SUB  */ t=T;n=N;        INC(2,-1) PUT(0, n - t) break;
			case 0x3a: /* MUL2 */ t=T2;n=N2;      INC(4,-2) PUT2(0, n * t) break;
			case 0x1a: /* MUL  */ t=T;n=N;        INC(2,-1) PUT(0, n * t) break;
			case 0x3b: /* DIV2 */ t=T2;n=N2;      INC(4,-2) PUT2(0, n / t) break;
			case 0x1b: /* DIV  */ t=T;n=N;        INC(2,-1) PUT(0, n / t) break;
			case 0x3c: /* AND2 */ t=T2;n=N2;      INC(4,-2) PUT2(0, n & t) break;
			case 0x1c: /* AND  */ t=T;n=N;        INC(2,-1) PUT(0, n & t) break;
			case 0x3d: /* ORA2 */ t=T2;n=N2;      INC(4,-2) PUT2(0, n | t) break;
			case 0x1d: /* ORA  */ t=T;n=N;        INC(2,-1) PUT(0, n | t) break;
			case 0x3e: /* EOR2 */ t=T2;n=N2;      INC(4,-2) PUT2(0, n ^ t) break;
			case 0x1e: /* EOR  */ t=T;n=N;        INC(2,-1) PUT(0, n ^ t) break;
			case 0x3f: /* SFT2 */ t=T;n=H2;       INC(3,-1) PUT2(0, n >> (t & 0x0f) << ((t & 0xf0) >> 4)) break;
			case 0x1f: /* SFT  */ t=T;n=N;        INC(2,-1) PUT(0, n >> (t & 0x0f) << ((t & 0xf0) >> 4)) break;
		}
	}		
}

int
uxn_boot(Uxn *u, Uint8 *ram, Dei *dei, Deo *deo)
{
	Uint32 i;
	char *cptr = (char *)u;
	for(i = 0; i < sizeof(*u); i++)
		cptr[i] = 0x00;
	u->wst = (Stack *)(ram + 0xf0000);
	u->rst = (Stack *)(ram + 0xf0100);
	u->dev = (Uint8 *)(ram + 0xf0200);
	u->ram = ram;
	u->dei = dei;
	u->deo = deo;
	return 1;
}
