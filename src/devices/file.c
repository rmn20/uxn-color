#include "../uxn.h"
#include "file.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE *f;
static DIR *d;
static int dir_fd;
static char *current_filename = "";
static enum { IDLE,
	FILE_READ,
	FILE_WRITE,
	DIR_READ } state;
static struct dirent *de;

static char hex[] = "0123456789abcdef";

static void
reset(void)
{
	if(f != NULL) {
		fclose(f);
		f = NULL;
	}
	if(d != NULL) {
		closedir(d);
		d = NULL;
	}
	de = NULL;
	state = IDLE;
}

void
file_prepare(void *filename)
{
	reset();
	current_filename = (char *)filename;
}

static Uint16
write_entry(char *p, Uint16 len, const char *filename, struct stat *st)
{
	if(len < strlen(filename) + 7) return 0;
	memcpy(p, "???? ", 5);
	strcpy(p + 5, filename);
	strcat(p, "\n");
	if(S_ISDIR(st->st_mode)) {
		memcpy(p, "---- ", 5);
	} else if(st->st_size < 0x10000) {
		p[0] = hex[(st->st_size >> 12) & 0xf];
		p[1] = hex[(st->st_size >> 8) & 0xf];
		p[2] = hex[(st->st_size >> 4) & 0xf];
		p[3] = hex[(st->st_size >> 0) & 0xf];
	}
	return strlen(p);
}

Uint16
file_read(void *dest, Uint16 len)
{
	if(state != FILE_READ && state != DIR_READ) {
		reset();
		if((d = opendir(current_filename)) != NULL) {
			state = DIR_READ;
			dir_fd = dirfd(d);
		} else if((f = fopen(current_filename, "rb")) != NULL)
			state = FILE_READ;
	}
	if(state == FILE_READ)
		return fread(dest, 1, len, f);
	if(state == DIR_READ) {
		char *p = dest;
		if(de == NULL) de = readdir(d);
		for(; de != NULL; de = readdir(d)) {
			struct stat st;
			Uint16 n;
			if(de->d_name[0] == '.' && de->d_name[1] == '\0')
				continue;
			if(fstatat(dir_fd, de->d_name, &st, 0))
				continue;
			n = write_entry(p, len, de->d_name, &st);
			if(!n) break;
			p += n;
			len -= n;
		}
		return p - (char *)dest;
	}
	return 0;
}

Uint16
file_write(void *src, Uint16 len, Uint8 flags)
{
	if(state != FILE_WRITE) {
		reset();
		if((f = fopen(current_filename, (flags & 0x01) ? "ab" : "wb")) != NULL)
			state = FILE_WRITE;
	}
	if(state == FILE_WRITE)
		return fwrite(src, 1, len, f);
	return 0;
}

Uint16
file_stat(void *dest, Uint16 len)
{
	struct stat st;
	if(stat(current_filename, &st))
		return 0;
	return write_entry((char *)dest, len, current_filename, &st);
}

Uint16
file_delete(void)
{
	return unlink(current_filename);
}
