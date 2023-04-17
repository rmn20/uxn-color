#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "uxn.h"

#pragma GCC diagnostic push
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#include <SDL.h>
#include "devices/system.h"
#include "devices/screen.h"
#include "devices/audio.h"
#include "devices/file.h"
#include "devices/controller.h"
#include "devices/mouse.h"
#include "devices/datetime.h"
#if defined(_WIN32) && defined(_WIN32_WINNT) && _WIN32_WINNT > 0x0602
#include <processthreadsapi.h>
#elif defined(_WIN32)
#include <windows.h>
#include <string.h>
#endif
#ifndef __plan9__
#define USED(x) (void)(x)
#endif
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

/*
Copyright (c) 2021-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define WIDTH 64 * 8
#define HEIGHT 40 * 8
#define PAD 4
#define TIMEOUT_MS 334
#define BENCH 0

static SDL_Window *gWindow;
static SDL_Texture *gTexture;
static SDL_Renderer *gRenderer;
static SDL_AudioDeviceID audio_id;
static SDL_Rect gRect;
static SDL_Thread *stdin_thread;

Uint16 deo_mask[] = {0xff08, 0x0300, 0xc028, 0x8000, 0x8000, 0x8000, 0x8000, 0x0000, 0x0000, 0x0000, 0xa260, 0xa260, 0x0000, 0x0000, 0x0000, 0x0000};
Uint16 dei_mask[] = {0x0000, 0x0000, 0x003c, 0x0014, 0x0014, 0x0014, 0x0014, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07ff, 0x0000, 0x0000, 0x0000};

/* devices */

static Uint8 zoom = 1;
static Uint32 stdin_event, audio0_event;
static Uint64 exec_deadline, deadline_interval, ms_interval;

char *rom_path;

static int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

static Uint8
audio_dei(int instance, Uint8 *d, Uint8 port)
{
	if(!audio_id) return d[port];
	switch(port) {
	case 0x4: return audio_get_vu(instance);
	case 0x2: POKE2(d + 0x2, audio_get_position(instance)); /* fall through */
	default: return d[port];
	}
}

static void
audio_deo(int instance, Uint8 *d, Uint8 port, Uxn *u)
{
	if(!audio_id) return;
	if(port == 0xf) {
		SDL_LockAudioDevice(audio_id);
		audio_start(instance, d, u);
		SDL_UnlockAudioDevice(audio_id);
		SDL_PauseAudioDevice(audio_id, 0);
	}
}

Uint8
uxn_dei(Uxn *u, Uint8 addr)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
	switch(d) {
	case 0x20: return screen_dei(u, addr);
	case 0x30: return audio_dei(0, &u->dev[d], p);
	case 0x40: return audio_dei(1, &u->dev[d], p);
	case 0x50: return audio_dei(2, &u->dev[d], p);
	case 0x60: return audio_dei(3, &u->dev[d], p);
	case 0xc0: return datetime_dei(u, addr);
	}
	return u->dev[addr];
}

void
uxn_deo(Uxn *u, Uint8 addr)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
	switch(d) {
	case 0x00:
		system_deo(u, &u->dev[d], p);
		if(p > 0x7 && p < 0xe)
			screen_palette(&uxn_screen, &u->dev[0x8]);
		break;
	case 0x10: console_deo(&u->dev[d], p); break;
	case 0x20: screen_deo(u->ram, &u->dev[d], p); break;
	case 0x30: audio_deo(0, &u->dev[d], p, u); break;
	case 0x40: audio_deo(1, &u->dev[d], p, u); break;
	case 0x50: audio_deo(2, &u->dev[d], p, u); break;
	case 0x60: audio_deo(3, &u->dev[d], p, u); break;
	case 0xa0: file_deo(0, u->ram, &u->dev[d], p); break;
	case 0xb0: file_deo(1, u->ram, &u->dev[d], p); break;
	}
}

#pragma mark - Generics

static void
audio_callback(void *u, Uint8 *stream, int len)
{
	int instance, running = 0;
	Sint16 *samples = (Sint16 *)stream;
	USED(u);
	SDL_memset(stream, 0, len);
	for(instance = 0; instance < POLYPHONY; instance++)
		running += audio_render(instance, samples, samples + len / 2);
	if(!running)
		SDL_PauseAudioDevice(audio_id, 1);
}

void
audio_finished_handler(int instance)
{
	SDL_Event event;
	event.type = audio0_event + instance;
	SDL_PushEvent(&event);
}

static int
stdin_handler(void *p)
{
	SDL_Event event;
	USED(p);
	event.type = stdin_event;
	while(read(0, &event.cbutton.button, 1) > 0 && SDL_PushEvent(&event) >= 0)
		;
	return 0;
}

static void
set_window_size(SDL_Window *window, int w, int h)
{
	SDL_Point win, win_old;
	SDL_GetWindowPosition(window, &win.x, &win.y);
	SDL_GetWindowSize(window, &win_old.x, &win_old.y);
	if(w == win_old.x && h == win_old.y) return;
	SDL_SetWindowPosition(window, (win.x + win_old.x / 2) - w / 2, (win.y + win_old.y / 2) - h / 2);
	SDL_SetWindowSize(window, w, h);
}

static int
set_size(void)
{
	gRect.x = PAD;
	gRect.y = PAD;
	gRect.w = uxn_screen.width;
	gRect.h = uxn_screen.height;
	if(gTexture != NULL) SDL_DestroyTexture(gTexture);
	SDL_RenderSetLogicalSize(gRenderer, uxn_screen.width + PAD * 2, uxn_screen.height + PAD * 2);
	gTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC, uxn_screen.width, uxn_screen.height);
	if(gTexture == NULL || SDL_SetTextureBlendMode(gTexture, SDL_BLENDMODE_NONE))
		return system_error("gTexture", SDL_GetError());
	if(SDL_UpdateTexture(gTexture, NULL, uxn_screen.pixels, sizeof(Uint32)) != 0)
		return system_error("SDL_UpdateTexture", SDL_GetError());
	set_window_size(gWindow, (uxn_screen.width + PAD * 2) * zoom, (uxn_screen.height + PAD * 2) * zoom);
	return 1;
}

static void
redraw(void)
{
	if(gRect.w != uxn_screen.width || gRect.h != uxn_screen.height) set_size();
	screen_redraw(&uxn_screen);
	if(SDL_UpdateTexture(gTexture, NULL, uxn_screen.pixels, uxn_screen.width * sizeof(Uint32)) != 0)
		system_error("SDL_UpdateTexture", SDL_GetError());
	SDL_RenderClear(gRenderer);
	SDL_RenderCopy(gRenderer, gTexture, NULL, &gRect);
	SDL_RenderPresent(gRenderer);
}

static int
init(void)
{
	SDL_AudioSpec as;
	SDL_zero(as);
	as.freq = SAMPLE_FREQUENCY;
	as.format = AUDIO_S16;
	as.channels = 2;
	as.callback = audio_callback;
	as.samples = 512;
	as.userdata = NULL;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
		return system_error("sdl", SDL_GetError());
	gWindow = SDL_CreateWindow("Uxn", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (WIDTH + PAD * 2) * zoom, (HEIGHT + PAD * 2) * zoom, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
	if(gWindow == NULL)
		return system_error("sdl_window", SDL_GetError());
	gRenderer = SDL_CreateRenderer(gWindow, -1, 0);
	if(gRenderer == NULL)
		return system_error("sdl_renderer", SDL_GetError());
	SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xff);
	audio_id = SDL_OpenAudioDevice(NULL, 0, &as, NULL, 0);
	if(!audio_id)
		system_error("sdl_audio", SDL_GetError());
	if(SDL_NumJoysticks() > 0 && SDL_JoystickOpen(0) == NULL)
		system_error("sdl_joystick", SDL_GetError());
	stdin_event = SDL_RegisterEvents(1);
	audio0_event = SDL_RegisterEvents(POLYPHONY);
	SDL_DetachThread(stdin_thread = SDL_CreateThread(stdin_handler, "stdin", NULL));
	SDL_StartTextInput();
	SDL_ShowCursor(SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	ms_interval = SDL_GetPerformanceFrequency() / 1000;
	deadline_interval = ms_interval * TIMEOUT_MS;
	return 1;
}

#pragma mark - Devices

/* Boot */

static int
start(Uxn *u, char *rom)
{
	free(u->ram);
	if(!uxn_boot(u, (Uint8 *)calloc(0x10000 * RAM_PAGES, sizeof(Uint8))))
		return system_error("Boot", "Failed to start uxn.");
	if(!system_load(u, rom))
		return system_error("Boot", "Failed to load rom.");
	exec_deadline = SDL_GetPerformanceCounter() + deadline_interval;
	if(!uxn_eval(u, PAGE_PROGRAM))
		return system_error("Boot", "Failed to eval rom.");
	SDL_SetWindowTitle(gWindow, rom);
	return 1;
}

static void
set_zoom(Uint8 z)
{
	zoom = z;
	set_window_size(gWindow, (uxn_screen.width + PAD * 2) * zoom, (uxn_screen.height + PAD * 2) * zoom);
}

static void
capture_screen(void)
{
	const Uint32 format = SDL_PIXELFORMAT_RGB24;
	time_t t = time(NULL);
	char fname[64];
	int w, h;
	SDL_Surface *surface;
	SDL_GetRendererOutputSize(gRenderer, &w, &h);
	surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, format);
	SDL_RenderReadPixels(gRenderer, NULL, format, surface->pixels, surface->pitch);
	strftime(fname, sizeof(fname), "screenshot-%Y%m%d-%H%M%S.bmp", localtime(&t));
	SDL_SaveBMP(surface, fname);
	SDL_FreeSurface(surface);
	fprintf(stderr, "Saved %s\n", fname);
	fflush(stderr);
}

static void
restart(Uxn *u)
{
	screen_resize(&uxn_screen, WIDTH, HEIGHT);
	if(!start(u, "launcher.rom"))
		start(u, rom_path);
}

static Uint8
get_button(SDL_Event *event)
{
	switch(event->key.keysym.sym) {
	case SDLK_LCTRL: return 0x01;
	case SDLK_LALT: return 0x02;
	case SDLK_LSHIFT: return 0x04;
	case SDLK_HOME: return 0x08;
	case SDLK_UP: return 0x10;
	case SDLK_DOWN: return 0x20;
	case SDLK_LEFT: return 0x40;
	case SDLK_RIGHT: return 0x80;
	}
	return 0x00;
}

static Uint8
get_button_joystick(SDL_Event *event)
{
	return 0x01 << (event->jbutton.button & 0x3);
}

static Uint8
get_vector_joystick(SDL_Event *event)
{
	if(event->jaxis.value < -3200)
		return 1;
	if(event->jaxis.value > 3200)
		return 2;
	return 0;
}

static Uint8
get_key(SDL_Event *event)
{
	int sym = event->key.keysym.sym;
	SDL_Keymod mods = SDL_GetModState();
	if(sym < 0x20 || sym == SDLK_DELETE)
		return sym;
	if(mods & KMOD_CTRL) {
		if(sym < SDLK_a)
			return sym;
		else if(sym <= SDLK_z)
			return sym - (mods & KMOD_SHIFT) * 0x20;
	}
	return 0x00;
}

static void
do_shortcut(Uxn *u, SDL_Event *event)
{
	if(event->key.keysym.sym == SDLK_F1)
		set_zoom(zoom == 3 ? 1 : zoom + 1);
	else if(event->key.keysym.sym == SDLK_F2)
		system_inspect(u);
	else if(event->key.keysym.sym == SDLK_F3)
		capture_screen();
	else if(event->key.keysym.sym == SDLK_F4)
		restart(u);
}

static int
handle_events(Uxn *u)
{
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		/* Window */
		if(event.type == SDL_QUIT)
			return system_error("Run", "Quit.");
		else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED)
			redraw();
		else if(event.type == SDL_DROPFILE) {
			screen_resize(&uxn_screen, WIDTH, HEIGHT);
			start(u, event.drop.file);
			SDL_free(event.drop.file);
		}
		/* Audio */
		else if(event.type >= audio0_event && event.type < audio0_event + POLYPHONY)
			uxn_eval(u, PEEK2(&u->dev[0x30 + 0x10 * (event.type - audio0_event)]));
		/* Mouse */
		else if(event.type == SDL_MOUSEMOTION)
			mouse_pos(u, &u->dev[0x90], clamp(event.motion.x - PAD, 0, uxn_screen.width - 1), clamp(event.motion.y - PAD, 0, uxn_screen.height - 1));
		else if(event.type == SDL_MOUSEBUTTONUP)
			mouse_up(u, &u->dev[0x90], SDL_BUTTON(event.button.button));
		else if(event.type == SDL_MOUSEBUTTONDOWN)
			mouse_down(u, &u->dev[0x90], SDL_BUTTON(event.button.button));
		else if(event.type == SDL_MOUSEWHEEL)
			mouse_scroll(u, &u->dev[0x90], event.wheel.x, event.wheel.y);
		/* Controller */
		else if(event.type == SDL_TEXTINPUT)
			controller_key(u, &u->dev[0x80], event.text.text[0]);
		else if(event.type == SDL_KEYDOWN) {
			int ksym;
			if(get_key(&event))
				controller_key(u, &u->dev[0x80], get_key(&event));
			else if(get_button(&event))
				controller_down(u, &u->dev[0x80], get_button(&event));
			else
				do_shortcut(u, &event);
			ksym = event.key.keysym.sym;
			if(SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_KEYUP, SDL_KEYUP) == 1 && ksym == event.key.keysym.sym) {
				return 1;
			}
		} else if(event.type == SDL_KEYUP)
			controller_up(u, &u->dev[0x80], get_button(&event));
		else if(event.type == SDL_JOYAXISMOTION) {
			Uint8 vec = get_vector_joystick(&event);
			if(!vec)
				controller_up(u, &u->dev[0x80], (3 << (!event.jaxis.axis * 2)) << 4);
			else
				controller_down(u, &u->dev[0x80], (1 << ((vec + !event.jaxis.axis * 2) - 1)) << 4);
		} else if(event.type == SDL_JOYBUTTONDOWN)
			controller_down(u, &u->dev[0x80], get_button_joystick(&event));
		else if(event.type == SDL_JOYBUTTONUP)
			controller_up(u, &u->dev[0x80], get_button_joystick(&event));
		else if(event.type == SDL_JOYHATMOTION) {
			/* NOTE: Assuming there is only one joyhat in the controller */
			switch(event.jhat.value) {
			case SDL_HAT_UP:
				controller_down(u, &u->dev[0x80], 0x10);
				break;
			case SDL_HAT_DOWN:
				controller_down(u, &u->dev[0x80], 0x20);
				break;
			case SDL_HAT_LEFT:
				controller_down(u, &u->dev[0x80], 0x40);
				break;
			case SDL_HAT_RIGHT:
				controller_down(u, &u->dev[0x80], 0x80);
				break;
			case SDL_HAT_LEFTDOWN:
				controller_down(u, &u->dev[0x80], 0x40 | 0x20);
				break;
			case SDL_HAT_LEFTUP:
				controller_down(u, &u->dev[0x80], 0x40 | 0x10);
				break;
			case SDL_HAT_RIGHTDOWN:
				controller_down(u, &u->dev[0x80], 0x80 | 0x20);
				break;
			case SDL_HAT_RIGHTUP:
				controller_down(u, &u->dev[0x80], 0x80 | 0x10);
				break;
			case SDL_HAT_CENTERED:
				/* Set all directions to down */
				controller_up(u, &u->dev[0x80], 0x10 | 0x20 | 0x40 | 0x80);
				break;
			default:
				/* Ignore */
				break;
			}
		}
		/* Console */
		else if(event.type == stdin_event)
			console_input(u, event.cbutton.button, CONSOLE_STD);
	}
	return 1;
}

static int
run(Uxn *u)
{
	Uint64 now = SDL_GetPerformanceCounter(), frame_end, frame_interval = SDL_GetPerformanceFrequency() / 60;
	for(;;) {
		Uint16 screen_vector;
		/* .System/halt */
		if(u->dev[0x0f])
			return system_error("Run", "Ended.");
		frame_end = now + frame_interval;
		exec_deadline = now + deadline_interval;
		if(!handle_events(u))
			return 0;
		screen_vector = PEEK2(&u->dev[0x20]);
		uxn_eval(u, screen_vector);
		if(uxn_screen.fg.changed || uxn_screen.bg.changed)
			redraw();
		now = SDL_GetPerformanceCounter();
		if(screen_vector) {
			if(!BENCH && ((Sint64)(frame_end - now)) > 0) {
				SDL_Delay((frame_end - now) / ms_interval);
				now = frame_end;
			}
		} else
			SDL_WaitEvent(NULL);
	}
}

int
main(int argc, char **argv)
{
	SDL_DisplayMode DM;
	Uxn u = {0};
	int i = 1;
	if(!init())
		return system_error("Init", "Failed to initialize emulator.");
	/* default resolution */
	screen_resize(&uxn_screen, WIDTH, HEIGHT);
	/* default zoom */
	if(argc > 1 && (strcmp(argv[i], "-1x") == 0 || strcmp(argv[i], "-2x") == 0 || strcmp(argv[i], "-3x") == 0))
		set_zoom(argv[i++][1] - '0');
	else if(SDL_GetCurrentDisplayMode(0, &DM) == 0)
		set_zoom(DM.w / 1280);
	/* load rom */
	if(i == argc)
		return system_error("usage", "uxnemu [-2x][-3x] file.rom");
	u.dev[0x17] = i == argc - 1;
	if(!start(&u, argv[i]))
		return system_error("Start", "Failed");
	rom_path = argv[i++];
	/* read arguments */
	for(; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(&u, *p++, CONSOLE_ARG);
		console_input(&u, '\n', i == argc - 1 ? CONSOLE_END : CONSOLE_EOA);
	}
	/* start rom */
	run(&u);
	/* finished */
#ifdef _WIN32
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
	TerminateThread((HANDLE)SDL_GetThreadID(stdin_thread), 0);
#elif !defined(__APPLE__)
	close(0); /* make stdin thread exit */
#endif
	SDL_Quit();
	return 0;
}
