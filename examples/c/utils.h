#pragma once

#include "aoo.h"

#include "portaudio.h"

#include <stdbool.h>

#define SLEEP_GRAIN 100

void setup(void);

// wait for Ctrl+C
void wait_for_quit(void);

// return random float in the range [0.0, 1.0]
double randf(void);

// Print all available input devices. You can then pass the number
// of the desired device to the -d resp. --device flag.
void print_input_devices(void);

// Print all available output devices. You can then pass the number
// of the desired device to the -d resp. --device flag.
void print_output_devices(void);

typedef struct audio_device_info {
    int channels;
    double sample_rate;
    size_t index;
    char display_name[128];
} audio_device_info;

// get audio device by number
void get_audio_input_device(int devno, audio_device_info* info);

void get_audio_output_device(int devno, audio_device_info* info);

// helper functions to match command line options
bool match_option(const char *argv, const char *short_option,
                 const char *long_option);

bool parse_int(const char *s, int *i);

bool get_endpoint_argument(const char **argv, int argc, const char **host,
                          int *port, AooId *id);
