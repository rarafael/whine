#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <SDL.h>

#define FREQ 48000
#define SAMPLE_DT (1.0f / FREQ)

// TODOO: Visualize a smol section of the white noise sound wave
// TODO: Mix in more randomness into the generated white noise

typedef struct {
    Sint16 current;
    Sint16 next;
    float step_time;
    float volume;
    float a;
} Gen;

float clampf(float v, float lo, float hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

float lerpf(float a, float b, float t)
{
    return (b - a) * t + a;
}

float ilerpf(float a, float b, float v)
{
    return (v - a) / (b - a);
}

void white_noise(Gen *gen, Sint16 *stream, size_t stream_len)
{
    float gen_step = (1.0f / (gen->step_time * SAMPLE_DT));

    for (size_t i = 0; i < stream_len; ++i) {
        gen->a += gen_step * SAMPLE_DT;
        stream[i] = lerpf(gen->next, gen->current, cosf(gen->a)) * gen->volume;

        if (gen->a >= 1.0f) {
            Sint16 value = rand() % (1 << 10);
            Sint16 sign = (rand() % 2) * 2 - 1;
            gen->current = gen->next;
            gen->next = value * sign;
            gen->a = 0.0f;
        }
    }
}

void white_noise_callback(void *userdata, Uint8 *stream, int len)
{
    assert(len % 2 == 0);
    white_noise(userdata, (Sint16*) stream, len / 2);
}

#define HEXCOLOR(code) \
  ((code) >> (3 * 8)) & 0xFF, \
  ((code) >> (2 * 8)) & 0xFF, \
  ((code) >> (1 * 8)) & 0xFF, \
  ((code) >> (0 * 8)) & 0xFF

#define BACKGROUND_COLOR 0x181818FF

#define STEP_TIME_SLIDER_THICC 5.0f
#define STEP_TIME_SLIDER_COLOR 0x00FF00FF
#define STEP_TIME_GRIP_SIZE 10.0f
#define STEP_TIME_GRIP_COLOR 0xFF0000FF

int hot_id = -1;
int active_id = -1;

void slider(SDL_Renderer *renderer, int id,
            float pos_x, float pos_y, float len,
            float *value, float min, float max)
{
    // Slider Body
    {
        SDL_SetRenderDrawColor(renderer, HEXCOLOR(STEP_TIME_SLIDER_COLOR));
        SDL_Rect rect = {
            .x = pos_x,
            .y = pos_y - STEP_TIME_SLIDER_THICC * 0.5f,
            .w = len,
            .h = STEP_TIME_SLIDER_THICC,
        };
        SDL_RenderFillRect(renderer, &rect);
    }

    // Grip
    {
        assert(min <= max);
        float grip_value = ilerpf(min, max, *value) * len;

        SDL_SetRenderDrawColor(renderer, HEXCOLOR(STEP_TIME_GRIP_COLOR));
        SDL_Rect rect = {
            .x = pos_x - STEP_TIME_GRIP_SIZE + grip_value,
            .y = pos_y - STEP_TIME_GRIP_SIZE,
            .w = STEP_TIME_GRIP_SIZE * 2.0f,
            .h = STEP_TIME_GRIP_SIZE * 2.0f,
        };
        SDL_RenderFillRect(renderer, &rect);

        int x, y;
        Uint32 buttons = SDL_GetMouseState(&x, &y);

        if (active_id < 0) {
            SDL_Point cursor = {x, y};
            if (SDL_PointInRect(&cursor, &rect) && (buttons & SDL_BUTTON_LMASK) != 0) {
                active_id = id;
            }
        } else {
            if (active_id == id) {
                if ((buttons & SDL_BUTTON_LMASK) == 0) {
                    active_id = -1;
                } else {
                    float grip_min = pos_x - STEP_TIME_GRIP_SIZE;
                    float grip_max = grip_min + len;
                    float xf = clampf(x - STEP_TIME_GRIP_SIZE, grip_min, grip_max);
                    xf = ilerpf(grip_min, grip_max, xf);
                    xf = lerpf(min, max, xf);
                    *value = xf;
                }
            }
        }
    }
}

typedef enum {
    SLIDER_FREQ = 0,
    SLIDER_VOLUME,
} Slider;

int main(void)
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "ERROR: could not initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_Window *window = SDL_CreateWindow("Whine", 0, 0, 800, 600, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        fprintf(stderr, "ERROR: could not create a window: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "ERROR: could not create a renderer: %s\n", SDL_GetError());
        exit(1);
    }

    Gen gen = {
        .step_time = 50.0f,
        .volume = 1.0f,
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

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch(event.type) {
            case SDL_QUIT: {
                quit = true;
            }
            break;
            }
        }

        SDL_SetRenderDrawColor(renderer, HEXCOLOR(BACKGROUND_COLOR));
        SDL_RenderClear(renderer);

        slider(renderer, SLIDER_FREQ,
               100.0f, 100.0f, 500.0f,
               &gen.step_time, 1.0f, 200.0f);
        slider(renderer, SLIDER_VOLUME,
               100.0f, 200.0f, 500.0f,
               &gen.volume, 0.0f, 1.0f);

        SDL_RenderPresent(renderer);
    }

    SDL_Quit();

    return 0;
}
