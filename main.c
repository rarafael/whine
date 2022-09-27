#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <signal.h>

#include <SDL.h>

#define FREQ 43200
#define SAMPLE_DT (1.0f / (float) FREQ)

// TODO: Better color scheme for the elements

// The state of the white noise generator
typedef struct {
    // These are "public" user customizable fields.
    // TODOOO: period should be probably in hertz
    float period;           // the duration of the period (in samples)
    float volume;           // the volume of the samples (between 0.0 and 1.0)

    // These fields are "private" and should be simply zero initialized and left alone
    bool initialized;       // indicates whether the generator was already initialized
    unsigned int seedp;     // the current state of rand_r
    Sint16 current;         // the current value of the generate sample (between SDL_MIN_SINT16 and SDL_MAX_SINT16)
    Sint16 target;          // the target value of the generate sample (between SDL_MIN_SINT16 and SDL_MAX_SINT16)
    float a;                // interpolator beween `current` and `target`
} Gen;

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

static inline float lerpf(float a, float b, float t)
{
    return (b - a) * t + a;
}

static inline float ilerpf(float a, float b, float v)
{
    return (v - a) / (b - a);
}

static void next_period(Gen *gen)
{
    Sint16 value = rand_r(&gen->seedp) % (1 << 10);
    Sint16 sign = (rand_r(&gen->seedp) % 2) * 2 - 1;
    gen->current = gen->target;
    gen->target = value * sign;
    gen->a = 0.0f;
}

static Sint16 next_sample(Gen *gen)
{
    if (!gen->initialized || gen->a >= 1.0) {
        next_period(gen);
        gen->initialized = true;
    }

    // TODOO: Mix in more randomness into the generated white noise (subfrequency)
    float step = 1.0f / (float) gen->period;

    gen->a += step;
    // TODOO: smoother interpolation
    return floorf(lerpf(gen->current, gen->target, gen->a) * gen->volume);
}

static void white_noise(Gen *gen, Sint16 *stream, size_t stream_len)
{
    for (size_t i = 0; i < stream_len; ++i) {
        stream[i] = next_sample(gen);
    }
}

static void white_noise_callback(void *userdata, Uint8 *stream, int len)
{
    assert(len % 2 == 0);
    white_noise(userdata, (Sint16*) stream, len / 2);
}

static void interrupt(int signal)
{
    SDL_Quit();
    exit(EXIT_SUCCESS);
}

int main(void)
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "ERROR: could not initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    Gen gen = {
        .period = 10.0f,
        .volume = 0.5f,
    };

    SDL_AudioSpec desired = {
        .freq = FREQ,
        .format = AUDIO_S16LSB,
        .channels = 1,
        .callback = white_noise_callback,
        .userdata = &gen,
    };

    if (SDL_OpenAudio(&desired, NULL) < 0) {
        fprintf(stderr, "ERROR: could not open audio device: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_PauseAudio(0);

    signal(SIGINT, interrupt);
    while (1);

    SDL_Quit();

    return EXIT_SUCCESS;
}
