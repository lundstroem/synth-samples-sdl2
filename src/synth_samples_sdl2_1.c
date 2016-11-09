/*
 
 This is free and unencumbered software released into the public domain.
 
 Anyone is free to copy, modify, publish, use, compile, sell, or
 distribute this software, either in source code form or as a compiled
 binary, for any purpose, commercial or non-commercial, and by any
 means.
 
 In jurisdictions that recognize copyright laws, the author or authors
 of this software dedicate any and all copyright interest in the
 software to the public domain. We make this dedication for the benefit
 of the public at large and to the detriment of our heirs and
 successors. We intend this dedication to be an overt act of
 relinquishment in perpetuity of all present and future rights to this
 software under copyright law.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 
 For more information, please refer to <http://unlicense.org>
 
 ----------------------------------------------------------------
 
 Synth Samples SDL2 C - 1 produce audio.
 
 1. The focus of this tutorial is to present the basics of
    a realtime audio application in SDL2.
 
    We tell SDL which function we want as audio callback:
    want.callback = audio_callback;
    Everytime a buffer needs to be filled, the audio_callback function which we have declared will be called.
    The buffer will then be filled with random samples to produce noise.
 
 
 dialect: C99
 dependencies: SDL2
 created by Harry Lundstrom on 14/09/15.
 these samples will be updated continually, check the repo for latest versions.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <SDL2/SDL.h>

// general
static bool quit = false;
static bool debuglog = false;
static int alloc_count = 0;

// SDL
static Uint16 buffer_size = 4096; // must be a power of two, decrease to allow for a lower latency, increase to reduce risk of underrun.
static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;
static SDL_Event event;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_GLContext context;
static int sample_rate = 44100;

// functions
static void run(void);
static void check_sdl_events(SDL_Event event);
static void main_loop(void);
static void *alloc_memory(size_t size, char *name); // malloc wrapper
static void *free_memory(void *ptr); // malloc wrapper
static void audio_callback(void *unused, Uint8 *byte_stream, int byte_stream_length);
static void write_samples(int16_t *s_byteStream, long begin, long end, long length);
static void setup_sdl(void);
static int setup_sdl_audio(void);
static void destroy_sdl(void);
static void t_log(char *message);


int main(int argc, char* argv[]) {
    
    run();
    return 0;
}

static void run(void) {
    
    setup_sdl();
    t_log("setup SDL successful.");
    
    setup_sdl_audio();
    t_log("setup SDL audio successful.");
    
    while (!quit) {
        main_loop();
    }
    
    destroy_sdl();
    t_log("SDL cleanup successful.");
    
    SDL_CloseAudioDevice(audio_device);
    t_log("SDL audio cleanup successful.");
    
    SDL_Quit();
    t_log("SDL quit successful.");
}

static void check_sdl_events(SDL_Event event) {
    
    while (SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_QUIT:
                quit = true;
                break;
        }
    }
}

static void main_loop(void) {
    
    // check for keyboard events etc.
    check_sdl_events(event);
    
    // update screen.
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
}

static void *alloc_memory(size_t size, char *name) {
    
    void *ptr = NULL;
    ptr = malloc(size);
    if(ptr == NULL) {
        if(debuglog) {
            printf("alloc_memory error: malloc with size %zu returned NULL.\n name:%s", size, name);
        }
    } else {
        alloc_count++;
    }
    return ptr;
}

static void *free_memory(void *ptr) {
    
    if(ptr != NULL) {
        free(ptr);
        alloc_count--;
    }
    return NULL;
}

static void audio_callback(void *unused, Uint8 *byte_stream, int byte_stream_length) {
    
    /*
     This function is called whenever the audio buffer needs to be filled to allow
     for a continuous stream of audio.
     Write samples to byteStream according to byteStreamLength.
     The audio buffer is interleaved, meaning that both left and right channels exist in the same
     buffer.
     */
    
    // zero the buffer
    memset(byte_stream, 0, byte_stream_length);
    
    if(quit) {
        return;
    }
    
    // cast buffer as 16bit signed int.
    Sint16 *s_byte_stream = (Sint16*)byte_stream;
    
    // buffer is interleaved, so get the length of 1 channel.
    int remain = byte_stream_length / 2;
    
    // write random samples to buffer to generate noise.
    for (int i = 0; i < remain; i += 2) {
        Sint16 sample = (Sint16)(rand()%(INT16_MAX*2))-INT16_MAX;
        sample *= 0.3; // scale volume.
        s_byte_stream[i] = sample; // left channel
        s_byte_stream[i+1] = sample; // right channel
    }
}

static void setup_sdl(void) {
    
    SDL_Init(SDL_INIT_VIDEO);
    
    // Get current display mode of all displays.
    SDL_DisplayMode current;
    for(int i = 0; i < SDL_GetNumVideoDisplays(); ++i) {
        int should_be_zero = SDL_GetCurrentDisplayMode(i, &current);
        if(should_be_zero != 0) {
            // In case of error...
            if(debuglog) { SDL_Log("Could not get display mode for video display #%d: %s", i, SDL_GetError()); }
        } else {
            // On success, print the current display mode.
            if(debuglog) { SDL_Log("Display #%d: current display mode is %dx%dpx @ %dhz. \n", i, current.w, current.h, current.refresh_rate); }
        }
    }
    
    window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_OPENGL);
    
    if(window != NULL) {
        context = SDL_GL_CreateContext(window);
        if(context == NULL) {
            printf("\nFailed to create context: %s\n", SDL_GetError());
        }
        
        renderer = SDL_CreateRenderer(window, -1, 0);
        if (renderer != NULL) {
            SDL_GL_SetSwapInterval(1);
            SDL_SetWindowTitle(window, "SDL2 synth sample 1");
        } else {
            if(debuglog) {
                printf("Failed to create renderer: %s", SDL_GetError());
            }
        }
    } else {
        if(debuglog) {
            printf("Failed to create window:%s", SDL_GetError());
        }
    }
}

static int setup_sdl_audio(void) {
    
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
    SDL_AudioSpec want;
    SDL_zero(want);
    SDL_zero(audio_spec);
    
    want.freq = sample_rate;
    // request 16bit signed little-endian sample format.
    want.format = AUDIO_S16LSB;
    // request 2 channels (stereo)
    want.channels = 2;
    want.samples = buffer_size;
    
    /*
     Tell SDL to call this function (audio_callback) that we have defined whenever there is an audiobuffer ready to be filled.
     */
    want.callback = audio_callback;
    
    if(debuglog) {
        printf("\naudioSpec want\n");
        printf("----------------\n");
        printf("sample rate:%d\n", want.freq);
        printf("channels:%d\n", want.channels);
        printf("samples:%d\n", want.samples);
        printf("----------------\n\n");
    }
    
    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &audio_spec, 0);
    
    if(debuglog) {
        printf("\naudioSpec get\n");
        printf("----------------\n");
        printf("sample rate:%d\n", audio_spec.freq);
        printf("channels:%d\n", audio_spec.channels);
        printf("samples:%d\n", audio_spec.samples);
        printf("size:%d\n", audio_spec.size);
        printf("----------------\n");
    }
    
    if (audio_device == 0) {
        if(debuglog) {
            printf("\nFailed to open audio: %s\n", SDL_GetError());
        }
        return 1;
    }
    
    if (audio_spec.format != want.format) {
        if(debuglog) {
            printf("\nCouldn't get requested audio format.\n");
        }
        return 2;
    }
    
    buffer_size = audio_spec.samples;
    SDL_PauseAudioDevice(audio_device, 0);// unpause audio.
    return 0;
}

static void destroy_sdl(void) {
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

static void t_log(char *message) {
    
    if(debuglog) {
        printf("log: %s \n", message);
    }
}

