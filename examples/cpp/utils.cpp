#include "utils.hpp"

#include <array>
#include <sstream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#else
# include <signal.h>
# include <stdio.h>
#endif

volatile bool quit = false;

std::default_random_engine randgen;

// console/signal handler to quit the program with Ctrl+C.

#ifdef _WIN32

BOOL WINAPI console_handler(DWORD signal) {
    switch (signal) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        quit = true;
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

void set_signal_handler(void) {
    struct sigaction sa;
    sa.sa_handler = [](int) {
        quit = true;
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
}

#endif

void setup() {
    set_signal_handler();

    // seed random number generator, see randf()
    std::random_device r;
    randgen.seed(r());
}

void wait_for_quit() {
    // For simplicity, we just wait in loop and wake up every few
    // milliseconds to check if the user has asked us to quit.
    // In real code you should use a proper synchronization primitive!
    while (!quit) {
        Pa_Sleep(SLEEP_GRAIN);
    }
}

double randf() {
    // not thread-safe, but we for our examples we don't care.
    std::uniform_real_distribution d;
    return d(randgen);
}

// helper function to print AooEndpoint
std::ostream& operator<<(std::ostream& os, const AooEndpoint& ep) {
    std::array<AooChar, 64> ip;
    AooSize size = ip.size();
    AooUInt16 port = 0;

    aoo_sockAddrToIpEndpoint(ep.address, ep.addrlen, ip.data(), &size, &port, 0);

    os << "[" << ip.data() << "]:" << port << "|" << ep.id;

    return os;
}

// Print all available input devices. You can then pass the number of the
// desired device to the -d resp. --device flag.
void print_devices(bool output) {
    auto defapi = Pa_GetHostApiInfo(Pa_GetDefaultHostApi());
    int defdev = output ? defapi->defaultOutputDevice : defapi->defaultInputDevice;
    int ndevs = Pa_GetDeviceCount();
    for (int i = 0, j = 0; i < ndevs; i++) {
        auto info = Pa_GetDeviceInfo(i);
        auto channels = output ? info->maxOutputChannels
                               : info->maxInputChannels;
        if (channels > 0) {
            auto api = Pa_GetHostApiInfo(info->hostApi);
            std::cout << "[" << j << "] " << api->name << " : " << info->name;
            if (i == defdev) {
                std::cout << " (DEFAULT)";
            }
            std::cout << "\n";
            j++;
        }
    }
}

void print_input_devices() {
    print_devices(false);
}

void print_output_devices() {
    print_devices(true);
}

audio_device_info get_audio_device(int devno, bool output) {
    const PaHostApiInfo *api = nullptr;
    const PaDeviceInfo *device = nullptr;
    size_t index = 0;

    // If the user provided a device number, get the
    // corresponding portaudio device index and info.
    if (devno >= 0) {
        int ndevs = Pa_GetDeviceCount();
        for (int i = 0, j = 0; i < ndevs; i++) {
            auto info = Pa_GetDeviceInfo(i);
            auto channels = output ? info->maxOutputChannels
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

    std::stringstream name;
    name << api->name << " : " << device->name;

    auto channels = output ? device->maxOutputChannels
                           : device->maxInputChannels;

    return { channels, device->defaultSampleRate, index, name.str() };
}

audio_device_info get_audio_input_device(int devno) {
    return get_audio_device(devno, false);
}

audio_device_info get_audio_output_device(int devno) {
    return get_audio_device(devno, true);
}

// helper functions to match command line options
bool match_option(const char **argv, std::string_view short_option,
                  std::string_view long_option) {
    std::string_view s(argv[0]);
    return s == short_option || s == long_option;
}

bool match_option(const char **argv, int argc, std::string_view short_option,
                  std::string_view long_option, int& value) {
    if (match_option(argv, short_option, long_option)) {
        if (argc < 2) {
            throw std::runtime_error("missing argument");
        }
        value = std::stoi(argv[1]);
        return true;
    } else {
        return false;
    }
}

void get_endpoint_argument(const char **argv, int argc, std::string& host,
                           int& port, AooId& id) {
    if (argc > 0) {
        if (argc >= 2) {
            host = argv[0];
            port = std::stoi(argv[1]);
        } else {
            throw std::runtime_error("missing argument");
        }

        if (argc >= 3) {
            id = std::stoi(argv[2]);
        }
    }
}
