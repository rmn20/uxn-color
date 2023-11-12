#include <varvara.h>

uint8_t cursor[8] = {
	0b11000000,
	0b11100000,
	0b11110000,
	0b11111000,
	0b11111100,
	0b11111100,
	0b11110000,
	0b11000000
};

void on_screen() {
	set_screen_xy(0, 128);
	set_work_palette(0);
	draw_pixel(0b11100000);
	
	for(uint8_t col = 0; col < 32; col++) {
		//background layer
		set_screen_xy(col * 8, 128);
		set_work_palette(col >> 2);
		draw_pixel(0b10100000 | (col & 3));
		
		//foreground layer
		set_screen_xy(col * 8, 128);
		set_work_palette(0b10000000 | (col >> 2));
		draw_pixel(0b11000000 | (col & 3));
	}
	
	set_work_palette(0);
	set_screen_xy(mouse_x() - 8, mouse_y());
	set_screen_addr(cursor);
	draw_sprite(0b01001111);
}

void initExtPalettes(uint8_t layer) {
	
	uint16_t dark = layer ? 0x0135 : 0x0246;
	uint16_t bright = layer ? 0x09BD : 0x8ADF;
	
	uint8_t layerShifted = layer << 7;
	
	//each bit in pal equals to rgb
	for(uint8_t pal = 0; pal < 8; pal++) {
		set_work_palette(layerShifted | pal);
		set_palette_ext((pal & 1) ? bright : dark, (pal & 2) ? bright : dark, (pal & 4) ? bright : dark);
	}
}

void main() {
	set_palette(0x27CF, 0x27CF, 0x27CF);
	
	initExtPalettes(0);
	initExtPalettes(1);
	
	set_screen_size(256, 256);
}