#include "../uxn.h"
#include "audio.h"
#include <stdbool.h>
#include <string.h>

/*
Copyright (c) 2021-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define SOUND_TIMER (AUDIO_BUFSIZE / SAMPLE_FREQUENCY * 1000.0f)
#define N_CHANNELS 4
#define XFADE_SAMPLES 100
#define INTERPOL_METHOD 1

typedef enum EnvStage {
    ENV_ATTACK  = (1 << 0),
    ENV_DECAY   = (1 << 1),
    ENV_SUSTAIN = (1 << 2),
    ENV_RELEASE = (1 << 3),
} EnvStage;

typedef struct Envelope {
    float a;
    float d;
    float s;
    float r;
    float vol;
    EnvStage stage;
} Envelope;

typedef struct Sample {
    Uint8 *data;
    float len;
    float pos;
    float inc;
    float loop;
    Uint8 pitch;
    Envelope env;
} Sample;

typedef struct AudioChannel {
    Sample sample;
    Sample next_sample;
    bool xfade;
    float duration;
    float vol_l;
    float vol_r;
} AudioChannel;

AudioChannel channel[N_CHANNELS];

const float tuning[109] = {
         0.09921257f,  0.10511205f,  0.11136234f,  0.11798429f,
         0.12500000f,  0.13243289f,  0.14030776f,  0.14865089f,
         0.15749013f,  0.16685498f,  0.17677670f,  0.18728838f,
         0.19842513f,  0.21022410f,  0.22272468f,  0.23596858f,
         0.25000000f,  0.26486577f,  0.28061551f,  0.29730178f,
         0.31498026f,  0.33370996f,  0.35355339f,  0.37457677f,
         0.39685026f,  0.42044821f,  0.44544936f,  0.47193716f,
         0.50000000f,  0.52973155f,  0.56123102f,  0.59460356f,
         0.62996052f,  0.66741993f,  0.70710678f,  0.74915354f,
         0.79370053f,  0.84089642f,  0.89089872f,  0.94387431f,
         1.00000000f,  1.05946309f,  1.12246205f,  1.18920712f,
         1.25992105f,  1.33483985f,  1.41421356f,  1.49830708f,
         1.58740105f,  1.68179283f,  1.78179744f,  1.88774863f,
         2.00000000f,  2.11892619f,  2.24492410f,  2.37841423f,
         2.51984210f,  2.66967971f,  2.82842712f,  2.99661415f,
         3.17480210f,  3.36358566f,  3.56359487f,  3.77549725f,
         4.00000000f,  4.23785238f,  4.48984819f,  4.75682846f,
         5.03968420f,  5.33935942f,  5.65685425f,  5.99322831f,
         6.34960421f,  6.72717132f,  7.12718975f,  7.55099450f,
         8.00000000f,  8.47570475f,  8.97969639f,  9.51365692f,
        10.07936840f, 10.67871883f, 11.31370850f, 11.98645662f,
        12.69920842f, 13.45434264f, 14.25437949f, 15.10198900f,
        16.00000000f, 16.95140951f, 17.95939277f, 19.02731384f,
        20.15873680f, 21.35743767f, 22.62741700f, 23.97291323f,
        25.39841683f, 26.90868529f, 28.50875898f, 30.20397801f,
        32.00000000f, 33.90281902f, 35.91878555f, 38.05462768f,
        40.31747360f, 42.71487533f, 45.25483400f, 47.94582646f,
        50.79683366f,
};

void
env_on(Envelope *env) {
    env->stage = ENV_ATTACK;
    env->vol = 0.0f;
    if (env->a > 0) {
        env->a = (SOUND_TIMER / AUDIO_BUFSIZE) / env->a;
    } else if (env->stage == ENV_ATTACK) {
        env->stage = ENV_DECAY;
        env->vol = 1.0f;
    }
    if (env->d > 0) {
        env->d = (SOUND_TIMER / AUDIO_BUFSIZE) / env->d;
    } else if (env->stage == ENV_DECAY) {
        env->stage = ENV_SUSTAIN;
        env->vol = env->s;
    }
    if (env->r > 0) {
        env->r = (SOUND_TIMER / AUDIO_BUFSIZE) / env->r;
    }
}

void
env_off(Envelope *env) {
    env->stage = ENV_RELEASE;
}

void
note_on(AudioChannel *channel, Uint16 duration, Uint8 *data, Uint16 len, Uint8 vol,
        Uint8 attack, Uint8 decay, Uint8 sustain, Uint8 release, Uint8 pitch, bool loop) {
    channel->duration = duration;
    channel->vol_l = (vol >> 4) / 16.0f * 0.9;
    channel->vol_r = (vol & 0xf) / 16.0f * 0.9;

    Sample sample = {0};
    sample.data = data;
    sample.len = len;
    sample.pos = 0;
    sample.env.a = attack * 62.0f;
    sample.env.d = decay * 62.0f;
    sample.env.s = sustain / 16.0f;
    sample.env.r = release * 62.0f;
    if (loop) {
        sample.loop = len;
    } else {
        sample.loop = 0;
    }
    env_on(&sample.env);
    const float *inc = &tuning[(pitch + 1) - 21];
    sample.inc = *(inc);

    channel->next_sample = sample;
    channel->xfade = true;
}

void
note_off(AudioChannel *channel, Uint16 duration) {
    channel->duration = duration;
    env_off(&channel->sample.env);
}

void
env_advance(Envelope *env) {
    switch (env->stage) {
        case ENV_ATTACK: {
            env->vol += env->a;
            if (env->vol >= 1.0f) {
                env->stage = ENV_DECAY;
                env->vol = 1.0f;
            }
        } break;
        case ENV_DECAY: {
            env->vol -= env->d;
            if (env->vol <= env->s || env->d <= 0) {
                env->stage = ENV_SUSTAIN;
                env->vol = env->s;
            }
        } break;
        case ENV_SUSTAIN: {
            env->vol = env->s;
        } break;
        case ENV_RELEASE: {
            if (env->vol <= 0 || env->r <= 0) {
                env->vol = 0;
            } else {
                env->vol -= env->r;
            }
        } break;
    }
}

float
interpolate_sample(Uint8 *data, Uint16 len, float pos) {
#if INTERPOL_METHOD == 0
    return data[(int)pos];

#elif INTERPOL_METHOD == 1
    float x = pos;
    int x0 = (int)x;
    int x1 = (x0 + 1);
    float y0 = data[x0];
    float y1 = data[x1 % len];
    x = x - x0;
    float y = y0 + x * (y1 - y0);
    return y;

#elif INTERPOL_METHOD == 2
    float x = pos;
    int x0 = x - 1;
    int x1 = x;
    int x2 = x + 1;
    int x3 = x + 2;
    float y0 = data[x0 % len];
    float y1 = data[x1];
    float y2 = data[x2 % len];
    float y3 = data[x3 % len];
    x = x - x1;
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
    float c3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);
    return ((c3 * x + c2) * x + c1) * x + c0;
#endif
}

Sint16
next_sample(Sample *sample) {
    if (sample->pos >= sample->len) {
        if (sample->loop == 0) {
            sample->data = 0;
            return 0;
        }
        while (sample->pos >= sample->len) {
            sample->pos -= sample->loop;
        }
    }

    float val = interpolate_sample(sample->data, sample->len, sample->pos);
    val *= sample->env.vol;
    Sint8 next = (Sint8)0x80 ^ (Uint8)val;

    sample->pos += sample->inc;
    env_advance(&sample->env);
    return next;
}

void
audio_handler(void *ctx, Uint8 *out_stream, int len) {
    Sint16 *stream = (Sint16 *)out_stream;
    memset(stream, 0x00, len);

    int n;
    for (n = 0; n < N_CHANNELS; n++) {
        Uint8 device = (3 + n) << 4;
        Uxn *u = (Uxn *)ctx;
        Uint8 *addr = &u->dev[device];
        if (channel[n].duration <= 0 && PEEK2(addr)) {
			uxn_eval(u, PEEK2(addr));
        }
        channel[n].duration -= SOUND_TIMER;

        int x = 0;
        if (channel[n].xfade) {
            float delta = 1.0f / (XFADE_SAMPLES * 2);
            while (x < XFADE_SAMPLES * 2) {
                float alpha = x * delta;
                float beta = 1.0f - alpha;
                Sint16 next_a = next_sample(&channel[n].next_sample);
                Sint16 next_b = 0;
                if (channel[n].sample.data != 0) {
                    next_b = next_sample(&channel[n].sample);
                }
                Sint16 next = alpha * next_a + beta * next_b;
                stream[x++] += next * channel[n].vol_l;
                stream[x++] += next * channel[n].vol_r;
            }
            channel[n].sample = channel[n].next_sample;
            channel[n].xfade = false;
        }
        Sample *sample = &channel[n].sample;
        while (x < len / 2) {
            if (sample->data == 0) {
                break;
            }
            Sint16 next = next_sample(sample);
            stream[x++] += next * channel[n].vol_l;
            stream[x++] += next * channel[n].vol_r;
        }
    }
    int i;
    for (i = 0; i < len; i++) {
        stream[i] <<= 4;
    }
}

void
audio_start(int idx, Uint8 *d, Uxn *u)
{
    Uint16 duration = PEEK2(d + 0x5);
    Uint8 off = d[0xf] == 0xff;

    if (!off) {
        Uint16 addr = PEEK2(d + 0xc);
        Uint8 *data = &u->ram[addr];
        Uint16 len = PEEK2(d + 0xa);
        Uint8 volume = d[0xe];
        bool loop = !!(d[0xf] & 0x80);
        Uint8 pitch = d[0xf] & 0x7f;
        Uint16 adsr = PEEK2(d + 0x8);
        Uint8 attack = (adsr >> 12) & 0xF;
        Uint8 decay = (adsr >> 8) & 0xF;
        Uint8 sustain = (adsr >> 4) & 0xF;
        Uint8 release = (adsr >> 0) & 0xF;
        note_on(&channel[idx], duration, data, len, volume, attack, decay, sustain, release, pitch, loop);
    } else {
        note_off(&channel[idx], duration);
    }
}
