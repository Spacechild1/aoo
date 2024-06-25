#pragma once

#include "aoo.h"

#include "portaudio.h"

#include <iostream>
#include <string>
#include <string_view>

constexpr int SLEEP_GRAIN = 100;

void setup();

// wait for Ctrl+C
void wait_for_quit();

// return random float in the range [0.0, 1.0]
double randf();

// helper function to print AooEndpoint
std::ostream& operator<<(std::ostream& os, const AooEndpoint& ep);

// Print all available input devices. You can then pass the number
// of the desired device to the -d resp. --device flag.
void print_input_devices();

// Print all available output devices. You can then pass the number
// of the desired device to the -d resp. --device flag.
void print_output_devices();

struct audio_device_info {
    int channels;
    double sample_rate;
    size_t index;
    std::string display_name;
};

// get audio device by number
audio_device_info get_audio_input_device(int devno = -1);

audio_device_info get_audio_output_device(int devno = -1);

// helper functions to match command line options
bool match_option(const char **argv, std::string_view short_option,
                  std::string_view long_option = "");

bool match_option(const char **argv, int argc, std::string_view short_option,
                  std::string_view long_option, int& value);

void get_endpoint_argument(const char **argv, int argc, std::string& host,
                           int& port, AooId& id);
