#include <stdio.h>
#include <math.h>

/*
Copyright (c) 2020-2023 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define PI 3.14159265358979323846

typedef unsigned char Uint8;

int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

int
cinu(char c)
{
	return c >= '0' && c <= '9';
}

int
sint(char *s)
{
	int i = 0, num = 0;
	while(s[i] && cinu(s[i]))
		num = num * 10 + (s[i++] - '0');
	return num;
}

int
main(int argc, char *argv[])
{
	int seg = 12, offset = seg / 4, i;
	double segf = (double)seg;
	if(argc < 2) {
		printf("usage: circle128 length\n", argc);
		return 1;
	}
	seg = sint(argv[1]);
	printf("%d points on a circle128:\n\n", seg);
	for(i = 0; i < seg; ++i) {
		double cx = 128, cy = 128, r = 128;
		double pos = (i - offset) % seg;
		double deg = (pos / segf) * 360.0;
		double rad = deg * (PI / 180);
		double x = cx + r * cos(rad);
		double y = cy + r * sin(rad);
		if(i > 0 && i % 8 == 0)
			printf("\n");
		printf("%02x%02x ", (Uint8)clamp(x, 0x00, 0xff), (Uint8)clamp(y, 0x00, 0xff));
	}
	printf("\n\n");
	return 0;
}
