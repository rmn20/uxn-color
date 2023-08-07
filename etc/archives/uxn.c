#include <stdio.h>
#include "uxn.h"

/*
Copyright (u) 2022-2023 Devine Lu Linvega, Andrew Alderwick, Andrew Richards

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define HALT(c) { return emu_halt(u, instr, (c), pc - 1); }
#define JUMP(x) { if(m2) pc = (x); else pc += (Sint8)(x); }
#define PUSH8(x) { if(s->ptr == 0xff) HALT(2) s->dat[s->ptr++] = (x); }
#define PUSH16(x) { if((tsp = s->ptr) >= 0xfe) HALT(2) t = (x); POKE2(&s->dat[tsp], t); s->ptr = tsp + 2; }
#define PUSH(x) { if(m2) { PUSH16(x) } else { PUSH8(x) } }
#define POP8(o) { if(*sp == 0x00) HALT(1) o = s->dat[--*sp]; }
#define POP16(o) { if((tsp = *sp) <= 0x01) HALT(1) o = PEEK2(&s->dat[tsp - 2]); *sp = tsp - 2; }
#define POP(o) { if(m2) { POP16(o) } else { POP8(o) } }
#define POKE(x, y) { if(m2) { t = (y); POKE2(u->ram + x, t) } else { u->ram[(x)] = (y); } }
#define PEEK(o, x) { if(m2) { o = PEEK2(u->ram + x); } else o = u->ram[(x)]; }
#define DEI(port) {  }
#define DEO(port, value) { u->dev[(port)] = (value); if((deo_mask[(port) >> 4] >> ((port) & 0xf)) & 0x1) emu_deo(u, (port)); }
#define DEVR(dest, port) { if(m2) { dest = (emu_dei(u, (port)) << 8) + emu_dei(u, (port+1)); } else { dest = emu_dei(u, (port)); } }
#define DEVW(port, value) { if(m2) { DEO((port),(value >> 8)) DEO((port+1),(value)) } else { DEO((port),(value)) } }

int
uxn_eval(Uxn *u, Uint16 pc)
{
	Uint8 instr, opcode, m2, ksp, tsp, *sp;
	Uint16 a, b, c, t;
	Stack *s;
	if(!pc || u->dev[0x0f]) return 0;
	for(;;) {
		instr = u->ram[pc++];
		/* printf("%02x\n", instr); */
		opcode = instr & 0x1f;
		/* Short Mode */
		m2 = instr & 0x20;
		/* Return Mode */
		s = instr & 0x40 ? &u->rst : &u->wst;
		/* Keep Mode */
		if(instr & 0x80) { ksp = s->ptr; sp = &ksp; }
		else sp = &s->ptr;
		/* Opcodes */
		switch(opcode - (!opcode * (instr >> 5))) {
		/* Immediate */
		case -0x0: /* BRK */ return 1;
		case -0x1: /* JCI */ POP8(b) if(!b) { pc += 2; break; } /* else fallthrough */
		case -0x2: /* JMI */ pc += PEEK2(u->ram + pc) + 2; break;
		case -0x3: /* JSI */ s = &u->rst; PUSH16(pc + 2) pc += PEEK2(u->ram + pc) + 2; break;
		case -0x4: /* LIT */
		case -0x6: /* LITr */ a = u->ram[pc++]; PUSH8(a) break;
		case -0x5: /* LIT2 */
		case -0x7: /* LIT2r */ PUSH16(PEEK2(u->ram + pc)) pc += 2; break;
		/* ALU */
		case 0x01: /* INC */ POP(a) PUSH(a + 1) break;
		case 0x02: /* POP */ POP(a) break;
		case 0x03: /* NIP */ POP(a) POP(b) PUSH(a) break;
		case 0x04: /* SWP */ POP(a) POP(b) PUSH(a) PUSH(b) break;
		case 0x05: /* ROT */ POP(a) POP(b) POP(c) PUSH(b) PUSH(a) PUSH(c) break;
		case 0x06: /* DUP */ POP(a) PUSH(a) PUSH(a) break;
		case 0x07: /* OVR */ POP(a) POP(b) PUSH(b) PUSH(a) PUSH(b) break;
		case 0x08: /* EQU */ POP(a) POP(b) PUSH8(b == a) break;
		case 0x09: /* NEQ */ POP(a) POP(b) PUSH8(b != a) break;
		case 0x0a: /* GTH */ POP(a) POP(b) PUSH8(b > a) break;
		case 0x0b: /* LTH */ POP(a) POP(b) PUSH8(b < a) break;
		case 0x0c: /* JMP */ POP(a) JUMP(a) break;
		case 0x0d: /* JCN */ POP(a) POP8(b) if(b) JUMP(a) break;
		case 0x0e: /* JSR */ POP(a) s = (instr & 0x40) ? &u->wst : &u->rst; PUSH16(pc) JUMP(a) break;
		case 0x0f: /* STH */ POP(a) s = (instr & 0x40) ? &u->wst : &u->rst; PUSH(a) break;
		case 0x10: /* LDZ */ POP8(a) PEEK(b, a) PUSH(b) break;
		case 0x11: /* STZ */ POP8(a) POP(b) POKE(a, b) break;
		case 0x12: /* LDR */ POP8(a) PEEK(b, pc + (Sint8)a) PUSH(b) break;
		case 0x13: /* STR */ POP8(a) POP(b) POKE(pc + (Sint8)a, b) break;
		case 0x14: /* LDA */ POP16(a) PEEK(b, a) PUSH(b) break;
		case 0x15: /* STA */ POP16(a) POP(b) POKE(a, b) break;
		case 0x16: /* DEI */ POP8(a) DEVR(b, a) PUSH(b) break;
		case 0x17: /* DEO */ POP8(a) POP(b) DEVW(a, b) break;
		case 0x18: /* ADD */ POP(a) POP(b) PUSH(b + a) break;
		case 0x19: /* SUB */ POP(a) POP(b) PUSH(b - a) break;
		case 0x1a: /* MUL */ POP(a) POP(b) PUSH((Uint32)b * a) break;
		case 0x1b: /* DIV */ POP(a) POP(b) if(!a) HALT(3) PUSH(b / a) break;
		case 0x1c: /* AND */ POP(a) POP(b) PUSH(b & a) break;
		case 0x1d: /* ORA */ POP(a) POP(b) PUSH(b | a) break;
		case 0x1e: /* EOR */ POP(a) POP(b) PUSH(b ^ a) break;
		case 0x1f: /* SFT */ POP8(a) POP(b) PUSH(b >> (a & 0x0f) << ((a & 0xf0) >> 4)) break;
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
