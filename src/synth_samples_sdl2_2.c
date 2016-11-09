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
 
 Synth Samples SDL2 C - 2 produce notes.
 
 The focus of this tutorial is to present the basics of
 a realtime audio application in SDL2 and to produce notes.
 
 1. We tell SDL which function we want as audio callback.
    want.callback = audio_callback;
    Everytime a buffer needs to be filled, the audio_callback function which we have declared will be called.
 
 2. To produce the notes we create a sinewave table, so the synthesis method here will be wavetable synthesis.
    chromatic_ratio is used to get the correct frequency for the notes and is then modified in relation to
    sample rate and table size to get correct phase for the oscillator.
    Use the keyboard to play different notes, plus and minus to change octave
 
 dialect: C99
 dependencies: SDL2
 created by Harry Lundstrom on 5/10/15.
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
static int table_length = 1024;

// voice
static const double pi = 3.14159265358979323846;
static const double chromatic_ratio = 1.059463094359295264562;
static double phase_double = 0;
static int phase_int = 0;
static int16_t *sine_wave_table;
static int note = 30; // integer representing halfnotes.
static int octave = 2;
static int max_note = 131;
static int min_note = 12;

// functions
static void run(void);
static void *alloc_memory(size_t size, char *name); // malloc wrapper
static void *free_memory(void *ptr); // malloc wrapper
static void build_sine_table(int16_t *data, int wave_length);
static double get_pitch(double note);
static void write_samples(int16_t *s_byteStream, long begin, long end, long length);
static void cleanup_data(void);
static void setup_sdl(void);
static int setup_sdl_audio(void);
static void check_sdl_events(SDL_Event event);
static void destroy_sdl(void);
static void init_data(void);
static void t_log(char *message);
static void handle_key_up(SDL_Keysym* keysym);
static void handle_key_down(SDL_Keysym* keysym);
static int get_deltatime(void);
static void main_loop(void);
static void handle_note_keys(SDL_Keysym* keysym);
static void print_note(int note);

int main(int argc, char* argv[]) {
    
    run();
    return 0;
}

static void run(void) {
    
    init_data();
    t_log("init data successful.");
    
    setup_sdl();
    t_log("setup SDL successful.");
    
    setup_sdl_audio();
    t_log("setup SDL audio successful.");
    
    while (!quit) {
        main_loop();
    }
    
    cleanup_data();
    t_log("cleanup data successful.");
    
    destroy_sdl();
    t_log("SDL cleanup successful.");
    
    SDL_CloseAudioDevice(audio_device);
    t_log("SDL audio cleanup successful.");
    
    SDL_Quit();
    t_log("SDL quit successful.");
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

static void build_sine_table(int16_t *data, int wave_length) {
    
    /* 
        Build sine table to use as oscillator:
        Generate a 16bit signed integer sinewave table with 1024 samples.
        This table will be used to produce the notes.
        Different notes will be created by stepping through
        the table at different intervals (phase).
    */
    
    double phase_increment = (2.0f * pi) / (double)wave_length;
    double current_phase = 0;
    for(int i = 0; i < wave_length; i++) {
        int sample = (int)(sin(current_phase) * INT16_MAX);
        data[i] = (int16_t)sample;
        current_phase += phase_increment;
    }
}

static double get_pitch(double note) {
    
    /*
        Calculate pitch from note value.
        offset note by 57 halfnotes to get correct pitch from the range we have chosen for the notes.
    */
    double p = pow(chromatic_ratio, note - 57);
    p *= 440;
    return p;
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
    
    // split the rendering up in chunks to make it buffersize agnostic.
    long chunk_size = 64;
    int iterations = remain/chunk_size;
    for(long i = 0; i < iterations; i++) {
        long begin = i*chunk_size;
        long end = (i*chunk_size) + chunk_size;
        write_samples(s_byte_stream, begin, end, chunk_size);
    }
}

static void write_samples(int16_t *s_byteStream, long begin, long end, long length) {
    
    if(note > 0) {
        double d_sample_rate = sample_rate;
        double d_table_length = table_length;
        double d_note = note;
        
        /*
            get correct phase increment for note depending on sample rate and table length.
        */
        double phase_increment = (get_pitch(d_note) / d_sample_rate) * d_table_length;
        
        /*
            loop through the buffer and write samples.
        */
        for (int i = 0; i < length; i+=2) {
            phase_double += phase_increment;
            phase_int = (int)phase_double;
            if(phase_double >= table_length) {
                double diff = phase_double - table_length;
                phase_double = diff;
                phase_int = (int)diff;
            }
            
            if(phase_int < table_length && phase_int > -1) {
                if(s_byteStream != NULL) {
                    int16_t sample = sine_wave_table[phase_int];
                    sample *= 0.3; // scale volume.
                    s_byteStream[i+begin] = sample; // left channel
                    s_byteStream[i+begin+1] = sample; // right channel
                }
            }
        }
    }
}

static void cleanup_data(void) {

    free_memory(sine_wave_table);
    printf("alloc count:%d\n", alloc_count);
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
            SDL_SetWindowTitle(window, "SDL2 synth sample 2");
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
        if(debuglog) { printf("\nFailed to open audio: %s\n", SDL_GetError()); }
        return 1;
    }
    
    if (audio_spec.format != want.format) {
        if(debuglog) { printf("\nCouldn't get requested audio format.\n"); }
        return 2;
    }
    
    buffer_size = audio_spec.samples;
    SDL_PauseAudioDevice(audio_device, 0);// unpause audio.
    return 0;
}

static void check_sdl_events(SDL_Event event) {
    
    while (SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                handle_key_down(&event.key.keysym);
                break;
            case SDL_KEYUP:
                handle_key_up(&event.key.keysym);
                break;
        }
    }
}

static void destroy_sdl(void) {

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

static void init_data(void) {
    
    // allocate memory for sine table and build it.
    sine_wave_table = alloc_memory(sizeof(int16_t)*table_length, "PCM table");
    build_sine_table(sine_wave_table, table_length);
}

static void t_log(char *message) {
    
    if(debuglog) {
        printf("log: %s \n", message);
    }
}

static void handle_key_up(SDL_Keysym* keysym) {
    
}

static void handle_key_down(SDL_Keysym* keysym) {
    
    handle_note_keys(keysym);
}

static void main_loop(void) {
    
    // check for keyboard events etc.
    check_sdl_events(event);
    
    // update screen.
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
}

static void handle_note_keys(SDL_Keysym* keysym) {
    
    // change note or octave depending on which key is pressed.
    
    int new_note = note;
    switch(keysym->sym) {
        case SDLK_PLUS:
            octave++;
            if(octave > 6) {
                octave = 6;
            } else {
                note += 12;
                if(note > max_note) {
                    note = max_note;
                }
            }
            printf("increased octave to:%d\n", octave);
            return;
            break;
        case SDLK_MINUS:
            octave--;
            if(octave < 0) {
                octave = 0;
            } else {
                note -= 12;
                if(note < min_note) {
                    note = min_note;
                }
            }
            printf("decreased octave to:%d\n", octave);
            return;
            break;
        case SDLK_z:
            new_note = 12;
            break;
        case SDLK_s:
            new_note = 13;
            break;
        case SDLK_x:
            new_note = 14;
            break;
        case SDLK_d:
            new_note = 15;
            break;
        case SDLK_c:
            new_note = 16;
            break;
        case SDLK_v:
            new_note = 17;
            break;
        case SDLK_g:
            new_note = 18;
            break;
        case SDLK_b:
            new_note = 19;
            break;
        case SDLK_h:
            new_note = 20;
            break;
        case SDLK_n:
            new_note = 21;
            break;
        case SDLK_j:
            new_note = 22;
            break;
        case SDLK_m:
            new_note = 23;
            break;
        case SDLK_COMMA:
            new_note = 24;
            break;
        case SDLK_l:
            new_note = 25;
            break;
        case SDLK_PERIOD:
            new_note = 26;
            break;
            
            //upper keyboard
        case SDLK_q:
            new_note = 24;
            break;
        case SDLK_2:
            new_note = 25;
            break;
        case SDLK_w:
            new_note = 26;
            break;
        case SDLK_3:
            new_note = 27;
            break;
        case SDLK_e:
            new_note = 28;
            break;
        case SDLK_r:
            new_note = 29;
            break;
        case SDLK_5:
            new_note = 30;
            break;
        case SDLK_t:
            new_note = 31;
            break;
        case SDLK_6:
            new_note = 32;
            break;
        case SDLK_y:
            new_note = 33;
            break;
        case SDLK_7:
            new_note = 34;
            break;
        case SDLK_u:
            new_note = 35;
            break;
        case SDLK_i:
            new_note = 36;
            break;
        case SDLK_9:
            new_note = 37;
            break;
        case SDLK_o:
            new_note = 38;
            break;
        case SDLK_0:
            new_note = 39;
            break;
        case SDLK_p:
            new_note = 40;
            break;
        default:
            return;
            break;
    }
    
    if(new_note > -1) {
        note = new_note;
        note += (octave * 12);
        print_note(note);
    }
}

static void print_note(int note) {
    
    int note_without_octave = note%12;
    int note_octave = (note/12)-1;
    char *note_chars = NULL;
    switch (note_without_octave) {
        case 0:
            note_chars = "C-";
            break;
        case 1:
            note_chars = "C#";
            break;
        case 2:
            note_chars = "D-";
            break;
        case 3:
            note_chars = "D#";
            break;
        case 4:
            note_chars = "E-";
            break;
        case 5:
            note_chars = "F-";
            break;
        case 6:
            note_chars = "F#";
            break;
        case 7:
            note_chars = "G-";
            break;
        case 8:
            note_chars = "G#";
            break;
        case 9:
            note_chars = "A-";
            break;
        case 10:
            note_chars = "A#";
            break;
        case 11:
            note_chars = "B-";
            break;
    }
    printf("note: %s%d pitch: %fHz\n", note_chars, note_octave, get_pitch(note));
}
