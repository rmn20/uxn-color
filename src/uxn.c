#include "uxn.h"

/*
Copyright (u) 2022-2023 Devine Lu Linvega, Andrew Alderwick, Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define HALT(c)    { return emu_halt(u, ins, (c), pc - 1); }
#define FLIP       { s = (ins & 0x40) ? &u->wst : &u->rst; }
#define JUMP(x)    { if(m2) pc = (x); else pc += (Sint8)(x); }
#define POKE(x, y) { if(m2) { POKE2(ram + x, y) } else { ram[(x)] = (y); } }
#define PEEK(o, x) { if(m2) { o = PEEK2(ram + x); } else o = ram[(x)]; }
#define PUSH1(y)   { if(s->ptr == 0xff) HALT(2) s->dat[s->ptr++] = (y); }
#define PUSH2(y)   { if((tsp = s->ptr) >= 0xfe) HALT(2) t = (y); POKE2(&s->dat[tsp], t); s->ptr = tsp + 2; }
#define PUSH(y)    { if(m2) { PUSH2(y) } else { PUSH1(y) } }
#define POP1(o)    { if(*sp == 0x00) HALT(1) o = s->dat[--*sp]; }
#define POP2(o)    { if((tsp = *sp) <= 0x01) HALT(1) o = PEEK2(&s->dat[tsp - 2]); *sp = tsp - 2; }
#define POP(o)     { if(m2) { POP2(o) } else { POP1(o) } }
#define DEVW(p, y) { if(m2) { DEO(p, y >> 8) DEO((p + 1), y) } else { DEO(p, y) } }
#define DEVR(o, p) { if(m2) { o = ((DEI(p) << 8) + DEI(p + 1)); } else { o = DEI(p); } }

int
uxn_eval(Uxn *u, Uint16 pc)
{
	Uint8 ins, opc, m2, ksp, tsp, *sp, *ram = u->ram;
	Uint16 a, b, c, t;
	Stack *s;
	if(!pc || u->dev[0x0f]) return 0;
	for(;;) {
		ins = ram[pc++];
		/* modes */
		opc = ins & 0x1f;
		m2 = ins & 0x20;
		s = ins & 0x40 ? &u->rst : &u->wst;
		if(ins & 0x80) { ksp = s->ptr; sp = &ksp; } else sp = &s->ptr;
		/* Opcodes */
		switch(opc - (!opc * (ins >> 5))) {
		/* Immediate */
		case -0x0: /* BRK   */ return 1;
		case -0x1: /* JCI   */ POP1(b) if(!b) { pc += 2; break; } /* else fallthrough */
		case -0x2: /* JMI   */ pc += PEEK2(ram + pc) + 2; break;
		case -0x3: /* JSI   */ PUSH2(pc + 2) pc += PEEK2(ram + pc) + 2; break;
		case -0x4: /* LIT   */
		case -0x6: /* LITr  */ a = ram[pc++]; PUSH1(a) break;
		case -0x5: /* LIT2  */
		case -0x7: /* LIT2r */ PUSH2(PEEK2(ram + pc)) pc += 2; break;
		/* ALU */
		case 0x01: /* INC */ POP(a) PUSH(a + 1) break;
		case 0x02: /* POP */ POP(a) break;
		case 0x03: /* NIP */ POP(a) POP(b) PUSH(a) break;
		case 0x04: /* SWP */ POP(a) POP(b) PUSH(a) PUSH(b) break;
		case 0x05: /* ROT */ POP(a) POP(b) POP(c) PUSH(b) PUSH(a) PUSH(c) break;
		case 0x06: /* DUP */ POP(a) PUSH(a) PUSH(a) break;
		case 0x07: /* OVR */ POP(a) POP(b) PUSH(b) PUSH(a) PUSH(b) break;
		case 0x08: /* EQU */ POP(a) POP(b) PUSH1(b == a) break;
		case 0x09: /* NEQ */ POP(a) POP(b) PUSH1(b != a) break;
		case 0x0a: /* GTH */ POP(a) POP(b) PUSH1(b > a) break;
		case 0x0b: /* LTH */ POP(a) POP(b) PUSH1(b < a) break;
		case 0x0c: /* JMP */ POP(a) JUMP(a) break;
		case 0x0d: /* JCN */ POP(a) POP1(b) if(b) JUMP(a) break;
		case 0x0e: /* JSR */ POP(a) FLIP PUSH2(pc) JUMP(a) break;
		case 0x0f: /* STH */ POP(a) FLIP PUSH(a) break;
		case 0x10: /* LDZ */ POP1(a) PEEK(b, a) PUSH(b) break;
		case 0x11: /* STZ */ POP1(a) POP(b) POKE(a, b) break;
		case 0x12: /* LDR */ POP1(a) PEEK(b, pc + (Sint8)a) PUSH(b) break;
		case 0x13: /* STR */ POP1(a) POP(b) POKE(pc + (Sint8)a, b) break;
		case 0x14: /* LDA */ POP2(a) PEEK(b, a) PUSH(b) break;
		case 0x15: /* STA */ POP2(a) POP(b) POKE(a, b) break;
		case 0x16: /* DEI */ POP1(a) DEVR(b, a) PUSH(b) break;
		case 0x17: /* DEO */ POP1(a) POP(b) DEVW(a, b) break;
		case 0x18: /* ADD */ POP(a) POP(b) PUSH(b + a) break;
		case 0x19: /* SUB */ POP(a) POP(b) PUSH(b - a) break;
		case 0x1a: /* MUL */ POP(a) POP(b) PUSH((Uint32)b * a) break;
		case 0x1b: /* DIV */ POP(a) POP(b) if(!a) HALT(3) PUSH(b / a) break;
		case 0x1c: /* AND */ POP(a) POP(b) PUSH(b & a) break;
		case 0x1d: /* ORA */ POP(a) POP(b) PUSH(b | a) break;
		case 0x1e: /* EOR */ POP(a) POP(b) PUSH(b ^ a) break;
		case 0x1f: /* SFT */ POP1(a) POP(b) PUSH(b >> (a & 0xf) << (a >> 4)) break;
		}
	}
}

int
uxn_boot(Uxn *u, Uint8 *ram)
{
	Uint32 i;
	char *cptr = (char *)u;
	for(i = 0; i < sizeof(*u); i++)
		cptr[i] = 0;
	u->ram = ram;
	return 1;
}
