#include <arm_neon.h>
#include "ppu.h"

void
ppu_redraw(Ppu *p, Uint32 *screen)
{
	/* FIXME(sigrid): do this better */
	Uint32 *rgba = __builtin_assume_aligned(screen, 16);
	Uint8 *fg = __builtin_assume_aligned(p->fg.pixels, 16);
	Uint8 *bg = __builtin_assume_aligned(p->bg.pixels, 16);
	Uint8 *palette = __builtin_assume_aligned((Uint8*)p->palette, 16);
	uint8x16x4_t pal = vld4q_u8(palette); enum { R, G, B, A };
	int i;

	for(i = 0; i < p->width * p->height; i += 16, fg += 16, bg += 16, rgba += 16) {
		uint8x16_t fg8 = vld1q_u8(fg);
		uint8x16_t bg8 = vld1q_u8(bg);
		uint8x16_t bgmask = vceqzq_u8(fg8);
		uint8x16_t px8 = vorrq_u8(vandq_u8(bg8, bgmask), vandq_u8(fg8, vceqzq_u8(bgmask)));
		uint8x16x4_t px = {
			vqtbl1q_u8(pal.val[R], px8),
			vqtbl1q_u8(pal.val[G], px8),
			vqtbl1q_u8(pal.val[B], px8),
			vqtbl1q_u8(pal.val[A], px8),
		};
		vst4q_u8((uint8_t*)rgba, px);
	}
}
