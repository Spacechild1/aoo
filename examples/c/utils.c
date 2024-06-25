#include "utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
# include <signal.h>
# include <stdio.h>
#endif

volatile bool quit = false;

// console/signal handler to quit the program with Ctrl+C.

#ifdef _WIN32

BOOL WINAPI console_handler(DWORD signal) {
    switch (signal) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        quit = 1;
        return TRUE;
    // Pass other signals to the next handler.
    default:
        return FALSE;
    }
}

void set_signal_handler(void) {
    SetConsoleCtrlHandler(console_handler, TRUE);
}

#else

void signal_handler(int sig) {
    quit = 1;
}

void set_signal_handler(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
}

#endif

void setup(void) {
    set_signal_handler();

    // Seed PRNG with system time, see randf().
    srand(time(NULL));
    // Call rand() a few times so the numbers are not too similar
    // when two or more processes are started in close succession.
    // Yes, this is terrible, but it's C after all :)
    // See cpp/utils.cpp for a decent PRNG in C++.
    for (int i = 0; i < 100; i++)
        rand();
}

void wait_for_quit(void) {
    // For simplicity, we just wait in loop and wake up every few
    // milliseconds to check if the user has asked us to quit.
    // In real code you should use a proper synchronization primitive!
    while (!quit) {
        Pa_Sleep(SLEEP_GRAIN);
    }
}

double randf(void) {
    // Yes, rand() is bad, but it's good enough for our purposes.
    return (double)rand() / (double)RAND_MAX;
}


// Print all available input devices. You can then pass the number of the
// desired device to the -d resp. --device flag.
void print_devices(int output) {
    const PaHostApiInfo *defapi = Pa_GetHostApiInfo(Pa_GetDefaultHostApi());
    int defdev = output ? defapi->defaultOutputDevice : defapi->defaultInputDevice;
    int ndevs = Pa_GetDeviceCount();
    for (int i = 0, j = 0; i < ndevs; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        int channels = output ? info->maxOutputChannels
                              : info->maxInputChannels;
        if (channels > 0) {
            const PaHostApiInfo *api = Pa_GetHostApiInfo(info->hostApi);
            fprintf(stdout, "[%d] %s : %s} %s\n", j, api->name, info->name,
                    i == defdev ? "(DEFAULT)" : "");
            j++;
        }
    }
}

void print_input_devices(void) {
    print_devices(0);
}

void print_output_devices(void) {
    print_devices(1);
}

void get_audio_device(int devno, audio_device_info *result, bool output) {
    const PaHostApiInfo *api = 0;
    const PaDeviceInfo *device = 0;
    size_t index = 0;

    // If the user provided a device number, get the
    // corresponding portaudio device index and info.
    if (devno >= 0) {
        int ndevs = Pa_GetDeviceCount();
        for (int i = 0, j = 0; i < ndevs; i++) {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            int channels = output ? info->maxOutputChannels
                                  : info->maxInputChannels;
            if (channels > 0) {
                if (j == devno) {
                    device = info;
                    api = Pa_GetHostApiInfo(info->hostApi);
                    index = i;
                    break;
                }
                j++;
            }
        }
    }

    if (!device) {
        // use default device
        api = Pa_GetHostApiInfo(Pa_GetDefaultHostApi());
        index = output ? api->defaultOutputDevice : api->defaultInputDevice;
        device = Pa_GetDeviceInfo(index);
    }

    result->channels = output ? device->maxOutputChannels
                              : device->maxInputChannels;
    result->sample_rate = device->defaultSampleRate;
    result->index = index;
    snprintf(result->display_name, sizeof(result->display_name),
             "%s : %s", api->name, device->name);
}

void get_audio_input_device(int devno, audio_device_info *info) {
    get_audio_device(devno, info, false);
}

void get_audio_output_device(int devno, audio_device_info *info) {
    get_audio_device(devno, info, true);
}

// helper functions to match command line options
bool match_option(const char *argv, const char *short_option,
                 const char *long_option) {
    return (short_option && !strcmp(argv, short_option)) ||
           (long_option && !strcmp(argv, long_option));
}

bool parse_int(const char *s, int *i) {
    return sscanf(s, "%d", i) == 1;
}

bool get_endpoint_argument(const char **argv, int argc, const char **host,
                           int *port, AooId *id) {
    if (argc > 0) {
        if (argc >= 2) {
            *host = argv[0];
            if (sscanf(argv[1], "%d", port) != 1) {
                fprintf(stdout, "bad PORT argument\n");
                return false;
            }
        } else {
            fprintf(stdout, "missing argument\n");
            return false;
        }

        if (argc >= 3) {
            if (sscanf(argv[1], "%d", id) != 1) {
                fprintf(stdout, "bad ID argument\n");
                return false;
            }
        }
    }

    return true;
}
