#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Uefi.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <libc/main.h>

#include "./Color.h"

#include "SDL.h"

#define COLOR_PROGRESS_PER_SECOND 36.0

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720


#include <UEfiTimeSupport.h>

// C++ like functions

#ifdef __cplusplus
#error "Not supported, as this is a C application"
#endif

#include <errno.h>

#define NANOSECONDS(x) ((x) * 1000000000ULL)


static uint64_t std_chrono_steady_clock_now(void) {
    struct timespec ts;
    int res = clock_gettime(CLOCK_MONOTONIC, &ts);
    ASSERT(res == 0);

    uint64_t nanoseconds = (((uint64_t) ts.tv_sec) * NANOSECONDS(1)) + ts.tv_nsec;

    return nanoseconds;
}

static bool helper_sleep_nanoseconds(uint64_t nano_seconds) {
    int result = 0;
    struct timespec remaining = {};
    struct timespec current = (struct timespec){
        .tv_sec = nano_seconds / NANOSECONDS(1),
        .tv_nsec = nano_seconds % NANOSECONDS(1),
    };

    do { // NOLINT(cppcoreguidelines-avoid-do-while)
        result = nanosleep(&current, &remaining);

        if (result == 0) {
            return true;
        }

        if (errno != EINTR) {
            return false;
        }


        current = remaining;
    } while (true);
}

//NOTE: this is just the value on qemu, is it always correct, i don't think so, but until we have a method to get that, it's hard to say
#define UEFI_NANOSECONDS_WHICH_USE_TIMER 100000000ULL

//TODO: qith 60 fps this mostly stalls and therefore the cpu is ath 100%, are there alternatives in UEFI?
static bool uefi_nanosleep_wrapper(uint64_t nano_seconds) {

    const uint64_t start_counter = SDL_GetPerformanceCounter();

    const uint64_t desired_counter = start_counter + nano_seconds;

    uint64_t current_counter = start_counter;

    while (desired_counter > current_counter) {


        const uint64_t left = desired_counter - current_counter;

        uint64_t portion = left >= 10000ULL ? left / 2 : left;

        //as nanosleep on uefi uses stall, when the time is too low, it's better to use higher values, when possible, as then it uses the event timer, which doesn't stall
        if (left > (UEFI_NANOSECONDS_WHICH_USE_TIMER + 1)) {
            portion = UEFI_NANOSECONDS_WHICH_USE_TIMER + 1;
        }

        bool result = helper_sleep_nanoseconds(portion);

        if (!result) {
            return false;
        }

        current_counter = SDL_GetPerformanceCounter();
    }
    return true;
}


static uint64_t get_sleep_time_ns(uint64_t target_framerate) {
    if (target_framerate == 0) {
        return 0;
    }
    return NANOSECONDS(1) / target_framerate;
}

typedef struct {
    uint64_t framerate;
    uint64_t sleep_time_ns;
} FPSSetting;

FPSSetting init_fps_setting(uint64_t fps) {

    return (FPSSetting){
        .framerate = fps,
        .sleep_time_ns = get_sleep_time_ns(fps),
    };
}

static bool rand_bool(void) {
    return (rand() & 0x01) != 0;
}

#define COLOR_RED ((SDL_Color){ .r = 0xFF, .g = 0, .b = 0, .a = 0xFF })
#define COLOR_GREEN ((SDL_Color){ .r = 0, .g = 0xFF, .b = 0, .a = 0xFF })
#define COLOR_BLUE ((SDL_Color){ .r = 0, .g = 0, .b = 0xFF, .a = 0xFF })
#define COLOR_WHITE ((SDL_Color){ .r = 0, .g = 0, .b = 0, .a = 0xFF })
#define COLOR_BLACK ((SDL_Color){ .r = 0xFF, .g = 0xFF, .b = 0xFF, .a = 0xFF })

static void SDL_SetRenderDrawColorC(SDL_Renderer* renderer, SDL_Color color) {
    int result = SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    ASSERT(result == 0);
}


//using 8x8 bitmap font from
// https://github.com/dhepper/font8x8

#include "./font8x8/font8x8_basic.h"

void font_8x8_draw_char(SDL_Renderer* renderer, char c, int x, int y, int scale) {
    uint8_t* glyph = (uint8_t*) font8x8_basic[(uint8_t) c];

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {

            // Font uses the least-significant bit as the leftmost pixel.
            if (glyph[row] & (1 << col)) {

                SDL_Rect pixel = { x + col * scale, y + row * scale, scale, scale };

                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }
}

void font_8x8_draw_text(SDL_Renderer* renderer, const char* text, int x, int y, int scale, SDL_Color color) {
    SDL_SetRenderDrawColorC(renderer, color);
    for (int i = 0; text[i] != '\0'; i++) {
        font_8x8_draw_char(renderer, text[i], x + i * 8 * scale, y, scale);
    }
}

static double gMeasuredFps = 0;

[[maybe_unused]] void displayFPS(SDL_Renderer* renderer) {

#define FPS_BUFFER_SIZE 0xFF

    static char fps_buffer[FPS_BUFFER_SIZE];

    int res = SDL_snprintf(fps_buffer, FPS_BUFFER_SIZE, "FPS: %.2f", gMeasuredFps);
    ASSERT(res > 0 && res <= FPS_BUFFER_SIZE);

    int text_size = SDL_strlen(fps_buffer);

    const uint32_t character_scale = SDL_max(SCREEN_WIDTH / 50, SCREEN_HEIGHT / 20) / 8;

    SDL_Rect text_box = { 0, 0, (2 * character_scale) + (text_size * character_scale * 8),
                          (2 * character_scale) + (character_scale * 8) };

    SDL_SetRenderDrawColorC(renderer, COLOR_BLACK);

    SDL_RenderFillRect(renderer, &text_box);

    font_8x8_draw_text(
            renderer, fps_buffer, text_box.x + character_scale, text_box.y + character_scale, character_scale,
            COLOR_WHITE
    );
    //
}

typedef void* (*Sdl2RenderExampleModeInitData)();

typedef void (*Sdl2RenderExampleModeResetData)(void*);

typedef void (*Sdl2RenderExampleModeDestroyData)(void*);

typedef bool (*Sdl2RenderExampleModeProcessKeyData)(void*, SDL_Keysym keysm);

typedef bool (*Sdl2RenderExampleModeRender)(SDL_Renderer* renderer, double dt, void* data);

typedef struct {
    void* data;
    Sdl2RenderExampleModeInitData init_data;
    Sdl2RenderExampleModeDestroyData destroy_data;
    Sdl2RenderExampleModeRender render;
    Sdl2RenderExampleModeProcessKeyData process_key;
} Sdl2RenderExampleMode;

#define RECT_WIDTH_EXAMPLE1 200
#define RECT_HEIGHT_EXAMPLE1 100

#define MOVEMENT_PER_SECOND_DX_EXAMPLE1 400.0
#define MOVEMENT_PER_SECOND_DY_EXAMPLE1 400.0

typedef struct {
    SDL_Rect rect;
    double dx;
    double dy;
    Uint64 freq;
    Uint64 start_counter;
} Sdl2RenderExample1Data;

void Sdl2RenderExample1_reset_data(void* _data) {
    Sdl2RenderExample1Data* data = (Sdl2RenderExample1Data*) _data;


    data->rect = (SDL_Rect){ (SCREEN_WIDTH - RECT_HEIGHT_EXAMPLE1) / 2, (SCREEN_HEIGHT - RECT_HEIGHT_EXAMPLE1) / 2,
                             RECT_WIDTH_EXAMPLE1, RECT_HEIGHT_EXAMPLE1 };


    data->dx = rand_bool() ? -MOVEMENT_PER_SECOND_DX_EXAMPLE1 : MOVEMENT_PER_SECOND_DX_EXAMPLE1;
    data->dy = rand_bool() ? -MOVEMENT_PER_SECOND_DY_EXAMPLE1 : MOVEMENT_PER_SECOND_DY_EXAMPLE1;

    data->freq = SDL_GetPerformanceFrequency();
    data->start_counter = SDL_GetPerformanceCounter();
}


void* Sdl2RenderExample1_init_data(void) {

    Sdl2RenderExample1Data* data = SDL_malloc(sizeof(Sdl2RenderExample1Data));

    if (data == NULL) {
        return NULL;
    }

    Sdl2RenderExample1_reset_data(data);

    return data;
}

void Sdl2RenderExample1_destroy_data(void* _data) {
    Sdl2RenderExample1Data* data = (Sdl2RenderExample1Data*) _data;

    SDL_free(data);
}

static SDL_Color rgb_to_sdl_color(rgb color) {
    return (
            SDL_Color
    ){ .r = (Uint8) (color.r * 255.0), .g = (Uint8) (color.g * 255.0), .b = (Uint8) (color.b * 255.0), .a = 0xFF };
}

bool Sdl2RenderExample1_render(SDL_Renderer* renderer, double dt, void* _data) {

    Sdl2RenderExample1Data* data = (Sdl2RenderExample1Data*) _data;

    Uint64 raw_counter = SDL_GetPerformanceCounter();

    Uint64 counter = raw_counter - data->start_counter;

    const double h = fmod((((double) counter) / (double) data->freq) * COLOR_PROGRESS_PER_SECOND, 360.0);

    hsv orig_color = (hsv){
        .h = h,
        .s = 1.0,
        .v = 1.0,
    };
    rgb final_color = hsv2rgb(orig_color);

    SDL_SetRenderDrawColorC(renderer, rgb_to_sdl_color(final_color));
    SDL_RenderClear(renderer);

    data->rect.x += (int) (data->dx * dt);
    data->rect.y += (int) (data->dy * dt);

    // Bounce horizontally
    if (data->rect.x <= 0) {
        data->dx = MOVEMENT_PER_SECOND_DX_EXAMPLE1;
        int reversed = -(data->rect.x);
        data->rect.x = SDL_max(0, reversed);
    } else if (data->rect.x + data->rect.w >= SCREEN_WIDTH) {
        data->dx = -MOVEMENT_PER_SECOND_DX_EXAMPLE1;
        int reversed = SCREEN_WIDTH - ((data->rect.x + data->rect.w) - SCREEN_WIDTH) - data->rect.w;
        data->rect.x = SDL_min(SCREEN_WIDTH, reversed);
    }


    // Bounce vertically
    if (data->rect.y <= 0) {
        data->dy = MOVEMENT_PER_SECOND_DY_EXAMPLE1;
        int reversed = -(data->rect.y);
        data->rect.y = SDL_max(0, reversed);
    } else if (data->rect.y + data->rect.h >= SCREEN_HEIGHT) {
        data->dy = -MOVEMENT_PER_SECOND_DY_EXAMPLE1;
        int reversed = SCREEN_HEIGHT - ((data->rect.y + data->rect.h) - SCREEN_HEIGHT) - data->rect.h;
        data->rect.y = SDL_min(SCREEN_HEIGHT, reversed);
    }

    hsv rect_orig_color = (hsv){
        .h = fmod(h + 180.0, 360.0),
        .s = 1.0,
        .v = 1.0,
    };
    rgb final_rect_color = hsv2rgb(rect_orig_color);

    //SDL_Color ii = rgb_to_sdl_color(final_rect_color);
    //SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "final_rect_color: %u %u %u %u", ii.r, ii.g, ii.b, ii.a);


    SDL_SetRenderDrawColorC(renderer, rgb_to_sdl_color(final_rect_color));

    SDL_RenderFillRect(renderer, &data->rect);

    return false;
}


bool Sdl2RenderExample1_process_key(void* _data, SDL_Keysym keysym) {
    Sdl2RenderExample1Data* data = (Sdl2RenderExample1Data*) _data;

    if (keysym.sym == 'r') {
        Sdl2RenderExample1_reset_data(data);
        return true;
    }

    return false;
}


typedef enum {
    Sdl2RenderExample2ModeDefault = 0,
    Sdl2RenderExample2ModeOneColor,
} Sdl2RenderExample2Mode;

typedef struct {
    Sdl2RenderExample2Mode mode;
    SDL_Color color;
} Sdl2RenderExample2Data;


void Sdl2RenderExample2_reset_data(void* _data) {
    Sdl2RenderExample2Data* data = (Sdl2RenderExample2Data*) _data;

    data->mode = Sdl2RenderExample2ModeDefault;
    data->color = COLOR_WHITE;
}


void* Sdl2RenderExample2_init_data(void) {

    Sdl2RenderExample2Data* data = SDL_malloc(sizeof(Sdl2RenderExample2Data));

    if (data == NULL) {
        return NULL;
    }

    Sdl2RenderExample2_reset_data(data);

    return data;
}

void Sdl2RenderExample2_destroy_data(void* _data) {
    Sdl2RenderExample2Data* data = (Sdl2RenderExample2Data*) _data;

    SDL_free(data);
}


bool Sdl2RenderExample2_render(SDL_Renderer* renderer, double dt, void* _data) {

    Sdl2RenderExample2Data* data = (Sdl2RenderExample2Data*) _data;


    switch (data->mode) {
        case Sdl2RenderExample2ModeDefault: {


#define COLORS_SIZE 5
            static SDL_Color colors[] = {
                COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE, COLOR_BLACK,
            };

            SDL_COMPILE_TIME_ASSERT(colors, SDL_arraysize(colors) == COLORS_SIZE);

            const int bar_size = SCREEN_WIDTH / COLORS_SIZE;

            for (size_t i = 0; i < COLORS_SIZE; ++i) {
                SDL_Color color = colors[i];
                SDL_SetRenderDrawColorC(renderer, color);

                int start_x = i * bar_size;
                int end_x = (i + 1) * bar_size;

                if (i + 1 == COLORS_SIZE) {
                    end_x = SCREEN_WIDTH;
                }

                SDL_Rect bar_rect = { start_x, 0, end_x, SCREEN_HEIGHT };

                SDL_RenderFillRect(renderer, &bar_rect);
            }


            break;
        }
        case Sdl2RenderExample2ModeOneColor: {
            SDL_SetRenderDrawColorC(renderer, data->color);

            SDL_Rect fullscreen_rect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };

            SDL_RenderFillRect(renderer, &fullscreen_rect);
            break;
        }
        default: {
            ASSERT(data->mode && false);
        }
    }


    return false;
}

bool Sdl2RenderExample2_process_key(void* _data, SDL_Keysym keysym) {
    Sdl2RenderExample2Data* data = (Sdl2RenderExample2Data*) _data;

    if (keysym.sym == 'd') {
        data->mode = Sdl2RenderExample2ModeDefault;
        data->color = COLOR_WHITE;
        return true;
    } else if (keysym.sym == 'r') {
        data->mode = Sdl2RenderExample2ModeOneColor;
        data->color = COLOR_RED;
        return true;
    } else if (keysym.sym == 'g') {
        data->mode = Sdl2RenderExample2ModeOneColor;
        data->color = COLOR_GREEN;
        return true;
    } else if (keysym.sym == 'b') {
        data->mode = Sdl2RenderExample2ModeOneColor;
        data->color = COLOR_BLUE;
        return true;
    } else if (keysym.sym == 'w') {
        data->mode = Sdl2RenderExample2ModeOneColor;
        data->color = COLOR_WHITE;
        return true;
    } else if (keysym.sym == 's') {
        // b is already used, so use s (german "schwarz")
        data->mode = Sdl2RenderExample2ModeOneColor;
        data->color = COLOR_BLACK;
        return true;
    }

    return false;
}


#define MODES_SIZE 2
static Sdl2RenderExampleMode g_modes[] = {
    (Sdl2RenderExampleMode){
                            .data = NULL,
                            .init_data = Sdl2RenderExample1_init_data,
                            .destroy_data = Sdl2RenderExample1_destroy_data,
                            .render = Sdl2RenderExample1_render,
                            .process_key = Sdl2RenderExample1_process_key,
                            },
    (Sdl2RenderExampleMode){
                            .data = NULL,
                            .init_data = Sdl2RenderExample2_init_data,
                            .destroy_data = Sdl2RenderExample2_destroy_data,
                            .render = Sdl2RenderExample2_render,
                            .process_key = Sdl2RenderExample2_process_key,
                            }
};

SDL_COMPILE_TIME_ASSERT(g_modes, SDL_arraysize(g_modes) == MODES_SIZE);

static uint8_t g_current_mode_idx = 0;

Sdl2RenderExampleMode* mode_setup(uint8_t idx) {

    ASSERT(idx >= 0 && idx < MODES_SIZE);

    Sdl2RenderExampleMode* mode = &(g_modes[idx]);

    ASSERT(mode->data == NULL);

    ASSERT(mode->init_data != NULL);
    ASSERT(mode->render != NULL);
    ASSERT(mode->destroy_data != NULL);
    // process_key might be null

    mode->data = mode->init_data();

    ASSERT(mode->data != NULL);

    g_current_mode_idx = idx;

    return mode;
}

void mode_reset(Sdl2RenderExampleMode* mode) {

    ASSERT(mode->data != NULL);

    mode->destroy_data(mode->data);

    mode->data = NULL;
}
bool mode_render(Sdl2RenderExampleMode* mode, double dt, SDL_Renderer* renderer) {
    ASSERT(mode->data != NULL);

    return mode->render(renderer, dt, mode->data);
}

bool mode_process_key(Sdl2RenderExampleMode* mode, SDL_Keysym keysym) {
    if (mode->process_key == NULL) {
        return false;
    }

    return mode->process_key(mode->data, keysym);
}

#define FPS_STEP 5


int sdl2_main(void) {

    srand(time(NULL));

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    int result = SDL_Init(SDL_INIT_VIDEO);

    if (result != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 3;
    }

    SDL_Window* window = SDL_CreateWindow(
            "This title is never shown", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN
    );

    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        return 4;
    }


    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        return 5;
    }

    const Uint64 freq = SDL_GetPerformanceFrequency();

    uint64_t start_execution_time = std_chrono_steady_clock_now();
    double dt = 0.0;

#if !defined(NDEBUG)
    uint64_t start_time = SDL_GetPerformanceCounter();
    uint64_t frame_counter = 0;
    const uint64_t update_time = freq / 2; //0.5 s;
    const double count_per_s = (double) freq;

    bool shouldDisplayFps = true;

#endif

    Sdl2RenderExampleMode* current_mode = mode_setup(0);

    // start with 60 FPS
    FPSSetting fps_setting = init_fps_setting(60);

    SDL_Event event = {};

    bool quit = false;

    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {

            switch (event.type) {
                case SDL_QUIT:
                    SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "Quitting");
                    quit = true;
                    break;
                case SDL_MOUSEMOTION:
                    // SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "SDL_MOUSEMOTION event: %d", event.type);
                    break;
                case SDL_MOUSEWHEEL:
                    //SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "SDL_MOUSEWHEEL event: %d", event.type);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    // SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "SDL_MOUSEBUTTONDOWN event: %d", event.type);
                    break;
                case SDL_MOUSEBUTTONUP:
                    // SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "SDL_MOUSEBUTTONUP event: %d", event.type);
                    break;
                case SDL_KEYDOWN:
                    SDL_KeyboardEvent key_event = event.key;
                    /*   SDL_LogVerbose(
                            SDL_LOG_CATEGORY_APPLICATION, "SDL_KEYDOWN event: %d %c", key_event.keysym.sym,
                            key_event.keysym.sym
                    ); */
                    if (key_event.keysym.sym >= '1' && key_event.keysym.sym <= '9') {
                        // choose mode 0-MODES_SIZE with 1-9
                        uint8_t mode_idx = key_event.keysym.sym - '1';

                        if (mode_idx >= 0 && mode_idx < MODES_SIZE && g_current_mode_idx != mode_idx) {
                            mode_reset(current_mode);
                            current_mode = mode_setup(mode_idx);
                        }
                    } else if (key_event.keysym.sym == '0') {
                        // set unlimited fps
                        fps_setting = init_fps_setting(0);
                    } else if (key_event.keysym.sym == 'p') {
                        //using p instead of +, as + is not the same on eng and german keyboards
                        // increase the fps cap by FPS_STEP
                        fps_setting = init_fps_setting(fps_setting.framerate + FPS_STEP);
                    } else if (key_event.keysym.sym == 'm') {
                        //using m instead of -, as - is not the same on eng and german keyboards
                        // decrease the fps cap by FPS_STEP, minimum is 1
                        fps_setting = init_fps_setting(
                                fps_setting.framerate > FPS_STEP ? fps_setting.framerate - FPS_STEP : 1
                        );
                    } else if (key_event.keysym.sym == 'f') {
                        // show or hide the fps display
                        shouldDisplayFps = !shouldDisplayFps;
                    } else if (key_event.keysym.sym == 27) {
                        //ESC
                        SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "ESC: Quitting");
                        quit = true;
                    } else {
                        mode_process_key(current_mode, key_event.keysym);
                    }
                    break;
                case SDL_KEYUP:
                    //  SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "SDL_KEYUP event: %d", event.type);
                    break;
                case SDL_TEXTINPUT:
                    //  SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "SDL_TEXTINPUT event: %d", event.type);
                    break;
                default:
                    // SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "Unkown SDL event: %d", event.type);
                    break;
            }
        }


        bool render_quit = mode_render(current_mode, dt, renderer);
        if (render_quit) {
            quit = true;
        }


#if !defined(NDEBUG)
        frame_counter++;

        const Uint64 current_time = SDL_GetPerformanceCounter();

        if (current_time - start_time >= update_time) {
            const double elapsed = (double) (current_time - start_time) / count_per_s;

            gMeasuredFps = (double) (frame_counter) / elapsed;

            start_time = current_time;
            frame_counter = 0;
        }

        if (shouldDisplayFps) {
            displayFPS(renderer);
        }
#endif

        SDL_RenderPresent(renderer);


        const uint64_t now = std_chrono_steady_clock_now();
        const uint64_t runtime = now - start_execution_time;


        if (fps_setting.framerate != 0 && runtime < fps_setting.sleep_time_ns) {
            bool sleep = uefi_nanosleep_wrapper(fps_setting.sleep_time_ns - runtime);
            ASSERT(sleep);


            const uint64_t after_sleep_now = std_chrono_steady_clock_now();
            //TODO: uefi is pretty inaccurate, fix it here, by providing a smaller value for the  sleep function!
            /* SDL_LogVerbose(
                    SDL_LOG_CATEGORY_APPLICATION, "sleep time: %llu, actually slept: %llu  ", sleep_time - runtime,
                    after_sleep_now - now
            ); */

            dt = ((double) (after_sleep_now - start_execution_time)) / (double) (NANOSECONDS(1));

            start_execution_time = after_sleep_now;
        } else {
            start_execution_time = now;
            dt = ((double) runtime) / (double) (NANOSECONDS(1));
        }
    }

    mode_reset(current_mode);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}

const char* EFIAPI bool_string(bool value) {
    return value ? "true" : "false";
}

/***
  Demonstrates basic workings of the main() function by displaying a
  welcoming message.

  Note that the UEFI command line is composed of 16-bit UCS2 wide characters.
  The easiest way to access the command line parameters is to cast Argv as:
      wchar_t **wArgv = (wchar_t **)Argv;

  @param[in]  Argc    Number of argument tokens pointed to by Argv.
  @param[in]  Argv    Array of Argc pointers to command line tokens.

  @retval  0         The application exited normally.
  @retval  Other     An error occurred.
***/
int EDK2_LIBC_ENTRY_NAME(IN int Argc, IN char** Argv) {

    ASSERT(gST != NULL);
    ASSERT(gBS != NULL);
    ASSERT(gImageHandle != NULL);

    DEBUG((DEBUG_ERROR, "starting SDL2 example in <C>\r\n"));
    int result = sdl2_main();

    return result;
}
