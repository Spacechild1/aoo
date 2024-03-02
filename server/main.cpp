#include "aoo/aoo.h"
#include "aoo/aoo_server.hpp"

#include "common/net_utils.hpp"
#include "common/sync.hpp"

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <thread>

#ifdef _WIN32
# include <windows.h>
#else
# include <signal.h>
# include <stdio.h>
#endif

#ifndef AOO_DEFAULT_SERVER_PORT
# define AOO_DEFAULT_SERVER_PORT 7078
#endif

AooLogLevel g_loglevel = kAooLogLevelWarning;

void log_function(AooLogLevel level, const AooChar *msg) {
    if (level <= g_loglevel) {
        switch (level) {
        case kAooLogLevelDebug:
            std::cout << "[debug] ";
            break;
        case kAooLogLevelVerbose:
            std::cout << "[verbose] ";
            break;
        case kAooLogLevelWarning:
            std::cout << "[warning] ";
            break;
        case kAooLogLevelError:
            std::cout << "[error] ";
            break;
        default:
            break;
        }
        std::cout << msg << std::endl;
    }
}

void AOO_CALL handle_event(void *, const AooEvent *event, AooThreadLevel level) {
    switch (event->type) {
    case kAooEventClientLogin:
    {
        auto e = event->clientLogin;
        if (e.error == kAooOk) {
            std::cout << "New client with ID " << e.id << std::endl;
        } else {
            std::cout << "Client " << e.id << " failed to log in" << std::endl;
        }
        break;
    }
    case kAooEventClientLogout:
    {
        auto e = event->clientLogout;
        if (e.errorCode == kAooOk) {
            std::cout << "Client " << e.id << " logged out" << std::endl;
        } else {
            std::cout << "Client " << e.id << " logged out after error: "
                      << e.errorMessage << std::endl;
        }
        break;
    }
    case kAooEventGroupAdd:
    {
        std::cout << "Add new group '" << event->groupAdd.name << "'" << std::endl;
        break;
    }
    case kAooEventGroupRemove:
    {
        std::cout << "Remove group '" << event->groupRemove.name << "'" << std::endl;
        break;
    }
    default:
        break;
    }
}

AooServer::Ptr g_server;

AooError g_error_code = 0;
char g_error_message[256] = {};
aoo::sync::semaphore g_semaphore;

void stop_server(int error, const char *msg = nullptr) {
    if (msg) {
        snprintf(g_error_message, sizeof(g_error_message), "%s", msg);
    }
    g_error_code = error;
    g_semaphore.post();
}

#ifdef _WIN32
BOOL WINAPI console_handler(DWORD signal) {
    switch (signal) {
    case CTRL_C_EVENT:
        stop_server(0);
        return TRUE;
    case CTRL_CLOSE_EVENT:
        return TRUE;
    // Pass other signals to the next handler.
    default:
        return FALSE;
    }
}
#else
bool set_signal_handler(int sig, sig_t handler) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(sig, &sa, nullptr) == 0) {
        return true;
    } else {
        perror("sigaction");
        return false;
    }
}

bool set_signal_handlers() {
    // NB: stop_server() is async-signal-safe!
    auto handler = [](int) { stop_server(0); };
    return set_signal_handler(SIGINT, handler)
           && set_signal_handler(SIGTERM, handler);
}
#endif

void print_usage() {
    std::cout
        << "Usage: aooserver [OPTIONS]...\n"
        << "Run an AOO server instance\n"
        << "Options:\n"
        << "  -h, --help             display help and exit\n"
        << "  -v, --version          print version and exit\n"
        << "  -p, --port             port number (default = " << AOO_DEFAULT_SERVER_PORT << ")\n"
        << "  -r, --relay            enable server relay\n"
        << "  -l, --log-level=LEVEL  set log level\n"
        << std::endl;
}

bool check_arguments(const char **argv, int argc, int numargs) {
    if (argc > numargs) {
        return true;
    } else {
        std::cout << "Missing argument(s) for option '" << argv[0] << "'";
        return false;
    }
}

bool match_option(const char *str, const char *short_option, const char *long_option) {
    return (short_option && !strcmp(str, short_option))
           || (long_option && !strcmp(str, long_option));
}

int main(int argc, const char **argv) {
    // set control handler
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
        std::cout << "Could not set console handler" << std::endl;
        return EXIT_FAILURE;
    }
#else
    if (!set_signal_handlers()) {
        return EXIT_FAILURE;
    }
#endif

    // parse command line options
    int port = AOO_DEFAULT_SERVER_PORT;
    bool relay = false;

    argc--; argv++;

    try {
        while ((argc > 0) && (argv[0][0] == '-')) {
            if (match_option(argv[0], "-h", "--help")) {
                print_usage();
                return EXIT_SUCCESS;
            } else if (match_option(argv[0], "-v", "--version")) {
                std::cout << "aooserver " << aoo_getVersionString() << std::endl;
                return EXIT_SUCCESS;
            } else if (match_option(argv[0], "-p", "--port")) {
                if (!check_arguments(argv, argc, 1)) {
                    return EXIT_FAILURE;
                }
                port = std::stoi(argv[1]);
                if (port <= 0 || port > 65535) {
                    std::cout << "Port number " << port << " out of range" << std::endl;
                    return EXIT_FAILURE;
                }
                argc--; argv++;
            } else if (match_option(argv[0], "-r", "--relay")) {
                relay = true;
            } else if (match_option(argv[0], "-l", "--log-level")) {
                if (!check_arguments(argv, argc, 1)) {
                    return EXIT_FAILURE;
                }
                g_loglevel = std::stoi(argv[1]);
                argc--; argv++;
            } else {
                std::cout << "Unknown command line option '" << argv[0] << "'" << std::endl;
                print_usage();
                return EXIT_FAILURE;
            }
            argc--; argv++;
        }
        if (argc > 0) {
            std::cout << "Ignoring excess arguments: ";
            for (int i = 0; i < argc; ++i) {
                std::cout << argv[i] << " ";
            }
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Bad argument for option '" << argv[0] << "'" << std::endl;
        return EXIT_FAILURE;
    }

    AooSettings settings;
    AooSettings_init(&settings);
    settings.logFunc = log_function;
    if (auto err = aoo_initialize(&settings); err != kAooOk) {
        std::cout << "Could not initialize AOO library: "
                  << aoo_strerror(err) << std::endl;
        return EXIT_FAILURE;
    }

    AooError err;
    g_server = AooServer::create(&err);
    if (!g_server) {
        std::cout << "Could not create AooServer: "
                  << aoo_strerror(err) << std::endl;
        return EXIT_FAILURE;
    }

    // we only need the event handler for logging
    if (g_loglevel >= kAooLogLevelVerbose) {
        g_server->setEventHandler(handle_event, nullptr, kAooEventModeCallback);
    }

    AooServerSettings server_settings;
    AooServerSettings_init(&server_settings);
    server_settings.portNumber = port;
    err = g_server->setup(server_settings);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket_strerror(aoo::socket_errno());
        } else {
            msg = aoo_strerror(err);
        }
        std::cout << "Could not setup AooServer: " << msg << std::endl;
        return EXIT_FAILURE;
    }

    g_server->setUseInternalRelay(relay);

    if (g_loglevel >= kAooLogLevelVerbose) {
        std::cout << "Listening on port " << port << std::endl;
    }

    // run server threads
    // NB: we *could* just block on the run() method, but then there
    // would be no "safe" way to break from a signal/control handler.
    auto thread = std::thread([]() {
        auto err = g_server->run(kAooFalse);
        if (err != kAooOk) {
            std::string msg;
            if (err == kAooErrorSocket) {
                msg = aoo::socket_strerror(aoo::socket_errno());
            } else {
                msg = aoo_strerror(err);
            }
            // break from the main thread
            stop_server(err, msg.c_str());
        }
    });

    auto udp_thread = std::thread([]() {
        auto err = g_server->receiveUDP(kAooFalse);
        if (err != kAooOk) {
            std::string msg;
            if (err == kAooErrorSocket) {
                msg = aoo::socket_strerror(aoo::socket_errno());
            } else {
                msg = aoo_strerror(err);
            }
            // break from the main thread
            stop_server(err, msg.c_str());
        }
    });

    // wait for stop signal
    g_semaphore.wait();

    if (g_error_code == 0) {
        std::cout << "Program stopped by the user" << std::endl;
    } else {
        std::cout << "Program stopped because of an error: "
                  << g_error_message << std::endl;
    }

    // stop server and join threads
    g_server->quit();
    if (thread.joinable()) {
        thread.join();
    }
    if (udp_thread.joinable()) {
        udp_thread.join();
    }

    aoo_terminate();

    return g_error_code == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
