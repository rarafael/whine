#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <SDL.h>

#define FREQ 48000
#define SAMPLE_DT (1.0f / FREQ)

// TODO: Better color scheme for the elements

typedef struct {
    unsigned int seedp;
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
    // TODOO: Mix in more randomness into the generated white noise (subfrequency)
    // TODOOOO: the first "period" of the generator is always 0
    float gen_step = (1.0f / (gen->step_time * SAMPLE_DT));

    for (size_t i = 0; i < stream_len; ++i) {
        gen->a += gen_step * SAMPLE_DT;
        // TODO: smoother interpolation
        stream[i] = lerpf(gen->next, gen->current, gen->a) * gen->volume;

        if (gen->a >= 1.0f) {
            // TODOOO: the periods have weird jumps
            Sint16 value = rand_r(&gen->seedp) % (1 << 10);
            Sint16 sign = (rand_r(&gen->seedp) % 2) * 2 - 1;
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

int active_id = -1;

#define WAVE_PREVIEW_WIDTH 1000.0f
#define WAVE_PREVIEW_HEIGHT 300.0f
#define WAVE_PREVIEW_SAMPLE_COLOR 0xFFFF00FF

void wave_preview(SDL_Renderer *renderer,
                  float pos_x, float pos_y,
                  Sint16 *stream, size_t stream_len)
{
    // TODO: when sample_width < 0 nothing is drawn
    float sample_width = WAVE_PREVIEW_WIDTH / (float) stream_len;
    float y_axis = pos_y + WAVE_PREVIEW_HEIGHT * 0.5f;

    for (size_t i = 0; i < stream_len; ++i) {
        float sample_x = pos_x + (float) i * sample_width;
        float sample_y = lerpf(pos_y,
                               pos_y + WAVE_PREVIEW_HEIGHT,
                               ilerpf(-((1 << 10) - 1), ((1 << 10) - 1), stream[i]));

        SDL_SetRenderDrawColor(renderer, HEXCOLOR(WAVE_PREVIEW_SAMPLE_COLOR));
        SDL_Rect rect = {
            .x = sample_x,
            .y = y_axis,
            .w = sample_width,
            .h = sample_y - y_axis,
        };
        SDL_RenderFillRect(renderer, &rect);
    }

}

#define SLIDER_THICCNESS 5.0f
#define SLIDER_COLOR 0x00FF00FF
#define SLIDER_GRIP_SIZE 30.0f
#define SLIDER_GRIP_COLOR 0xFF0000FF

bool slider(SDL_Renderer *renderer, int id,
            float pos_x, float pos_y, float len,
            float *value, float min, float max)
{
    bool modified = false;

    // TODO: display the current value of the slider

    // Slider Body
    {
        SDL_SetRenderDrawColor(renderer, HEXCOLOR(SLIDER_COLOR));
        SDL_Rect rect = {
            .x = pos_x,
            .y = pos_y - SLIDER_THICCNESS * 0.5f,
            .w = len,
            .h = SLIDER_THICCNESS,
        };
        SDL_RenderFillRect(renderer, &rect);
    }

    // Grip
    {
        assert(min <= max);
        float grip_value = ilerpf(min, max, *value) * len;

        // TODO: the grip should go outside of the slider body

        SDL_SetRenderDrawColor(renderer, HEXCOLOR(SLIDER_GRIP_COLOR));
        SDL_Rect rect = {
            .x = pos_x - SLIDER_GRIP_SIZE + grip_value,
            .y = pos_y - SLIDER_GRIP_SIZE,
            .w = SLIDER_GRIP_SIZE * 2.0f,
            .h = SLIDER_GRIP_SIZE * 2.0f,
        };
        SDL_RenderFillRect(renderer, &rect);

        int x, y;
        Uint32 buttons = SDL_GetMouseState(&x, &y);

        // TODO: the grip should maintain the initial offset between its position and mouse_x

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
                    float grip_min = pos_x - SLIDER_GRIP_SIZE;
                    float grip_max = grip_min + len;
                    float xf = clampf(x - SLIDER_GRIP_SIZE, grip_min, grip_max);
                    xf = ilerpf(grip_min, grip_max, xf);
                    xf = lerpf(min, max, xf);
                    *value = xf;
                    modified = true;
                }
            }
        }
    }

    return modified;
}

typedef enum {
    // TODO: add a slider for the amount of preview samples
    SLIDER_FREQ = 0,
    SLIDER_VOLUME,
} Slider;

#define PREVIEW_STREAM_CAP 100
Sint16 preview_stream[PREVIEW_STREAM_CAP];

void regen_preview_stream(const Gen *gen)
{
    Gen preview_gen = {
        .step_time = gen->step_time,
        .volume = gen->volume,
    };
    white_noise(&preview_gen, preview_stream, PREVIEW_STREAM_CAP);
}

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
        .step_time = 10.0f,
        .volume = 0.5f,
    };

    regen_preview_stream(&gen);

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

        // TODO: automatic layouting of the widgets based on the window size

        if (slider(renderer, SLIDER_FREQ, 100.0f, 100.0f, 500.0f, &gen.step_time, 1.0f, 50.0f)) {
            regen_preview_stream(&gen);
        }

        if (slider(renderer, SLIDER_VOLUME, 100.0f, 200.0f, 500.0f, &gen.volume, 0.0f, 1.0f)) {
            regen_preview_stream(&gen);
        }

        wave_preview(renderer, 100.0f, 300.0f, preview_stream, PREVIEW_STREAM_CAP);

        SDL_RenderPresent(renderer);
    }

    SDL_Quit();

    return 0;
}
