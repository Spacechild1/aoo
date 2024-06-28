AOO (audio over OSC) v2.0-test3
===============================

AOO ("audio over OSC") is a message based audio system, using Open Sound Control [^OSC] as the underlying transport protocol.
It is fundamentally connectionless and allows to send audio in real time and on demand between arbitrary network endpoints.

**WARNING**: AOO is still alpha software, there are breaking changes between pre-releases!

---

### Features

* peer-to-peer audio networks (IPv4 and IPv6) of any topology with arbitrary ad-hoc connections.

* each endpoint can have multiple sources/sinks (each with their own ID).

* AOO sources can send audio to any sink at any time.

* AOO sinks can listen to several sources at the same time, summing the signals.

* AOO is connectionless: streams can start/stop at any time, enabling a "message-based audio" approach.

* AOO sinks can "invite" and "uninvite" sources, i.e. ask them to send resp. stop sending audio.
  The source may accept the (un)invitation or decline it.

* streams and invitations can contain arbitrary metadata.

* AOO sources can send arbitrary messages together with the audio data with sample accuracy.

* AOO sinks and sources can operate at different blocksizes and samplerates.

* clock differences between machines can be adjusted automatically (= dynamic resampling).

* the stream format can be set independently for each source.

* support for different audio codecs. Currently, only PCM (uncompressed) and Opus (compressed) are implemented,
  but additional codecs can be added with the codec plugin API.

* the sink jitter buffer helps to deal with network jitter, packet reordering and packet loss
  at the cost of latency. The size can be adjusted dynamically.

* sinks can ask the source(s) to resend dropped packets.

* settable UDP packet size (= MTU) to optimize for local networks or the internet.

* pings are exchanged between sources and sinks at a configurable rate.
  They carry NTP timestamps, so the user can calculate the network latency and perform latency compensation.
  The source also receives the current average packet loss percentage.

* several diagnostic events about packet loss, resent packets, etc.

* the AOO server is a connection server [^Udp] that facilitates peer-to-peer communication in a local network or over the public internet.

* the AOO client manages multiple AOO sources/sinks (on the same port) and may connect to an AOO server.

* AOO clients can send each other timestamped messages with optional reliable transmission.

* AOO is realtime-safe, i.e. it will never block the audio thread. (Network I/O is handled on dedicated threads.)

* the C/C++ library can be easily embedded in host applications or plugins.

---

### History

The vision of AOO was first proposed in 2009 by Winfried Ritsch together with an initial draft of realisation for embedded devices.
In 2010 it has been implemented by Wolfgang Jäger as a library (v1.0-b2) with externals for Pure Data (Pd) [^Pd],
but major issues with the required networking objects made this version unpracticable and so it was not used extensively.
More on this version of AOO as "message based audio system" was published at LAC 2014 [^LAC14]

A new version (2.0, not backwards compatible) has been written from scratch by Christof Ressi, with a first pre-release in April 2020.
It has been initially developed in February 2020 for a network streaming project at Kunsthaus Graz with Bill Fontana, using an independent
wireless network infrastructure FunkFeuer Graz [^0xFF]. It was subsequently used for the Virtual Rehearsal Room project [^VRR]
and also helped reviving the Virtual IEM Computer Music Ensemble [^VICE] within a seminar at the IEM [^IEM] in Graz.

---

### Content

`aoo`      - AOO C/C++ library source code

`cmake`    - CMake helper files

`common`   - shared code

`deps`     - dependencies

`doc`      - documentation

`doxygen`  - doxygen documentation

`examples` - examples

`include`  - public headers

`pd`       - Pd external

`server`   - the `aooserver` command line program

`tests`    - test suite

---

### C/C++ library

The public API headers are contained in the `aoo/include` directory.

To generate the API documentation you need to have `doxygen` installed. Just run the `doxygen`
command in the toplevel folder and it will store the generated files in the `doxygen` folder.
You can view the documentation in a standard web browser by opening `doxygen/html/index.html`.

The library features four object classes:

`AooSource` - AOO source object, see `aoo_source.h` resp. `aoo_source.hpp` in `include/aoo`.

`AooSink` - AOO sink object, see `aoo_sink.h` resp. `aoo_sink.hpp` in `include/aoo`.

`AooClient` - AOO client object, see `aoo_client.h` resp. `aoo_client.hpp` in `include/aoo`.

`AooServer` - AOO UDP hole punching server, see `aoo_server.h` resp. `aoo_server.hpp` in `include/aoo`.

**NOTE**:
By following COM conventions (no virtual destructors, no method overloading, only simple
function parameters and return types), we achieve portable C++ interfaces on Windows and all other
platforms with a stable vtable layout (generally true for Linux and macOS in my experience).
This means you can take a pre-build version of the AOO shared library, which might have been built
with another compiler (version), and directly use it in your C++ project.

The C interface is meant to be used in C projects and for creating bindings to other languages.

---

### Pd external

Objects:

`[aoo_send~]` - send an AOO stream (with integrated threaded network IO)

`[aoo_receive~]` - receive one or more AOO streams (with integrated threaded network IO)

`[aoo_client]` - connect to AOO peers over the public internet or in a local network

`[aoo_server]` - AOO connection server

For documentation see the corresponding help patches.

The Pd external is available on Deken (in Pd -> Help -> "Find externals" search for "aoo".)

---

### The aooserver command line program

If you want to host your own (private or public) AOO server, you only have to run `aooserver`
on the command line or as a service and make sure that clients can connect to your machine.

Run `aooserver -h` to see all available options.

---

### Build instructions

#### Prerequisites

1. Install CMake [^CMake]

2. Install Git [^Git]

3. Get the AOO source code: http://git.iem.at/cm/aoo/
   HTTPS: `git clone https://git.iem.at/cm/aoo.git`
   SSH: `git clone git@git.iem.at:cm/aoo.git`

4. Fetch submodules:
   `git submodule update --init`


#### Pure Data

1. Install Pure Data.
   Windows/macOS: http://msp.ucsd.edu/software.html
   Linux: `sudo apt-get install pure-data-dev`

2. The `AOO_BUILD_PD_EXTERNAL` CMake variable must be `ON`

3. Make sure that `PD_INCLUDE_DIR` points to the Pd `src` or `include` directory.

4. Windows: make sure that `PD_BIN_DIR` points to the Pd `bin` directory.

5. Set `PD_INSTALL_DIR` to the desired installation path (if you're not happy with the default).


#### Opus

Opus[^Opus] is a high quality low latency audio codec.
If you want to build AOO with integrated Opus support there are two options:

a) Link with the system wide `opus` library

1. Install Opus:
  * macOS -> homebrew: `brew install opus`
  * Windows -> Msys2: `pacman -S mingw32/mingw-w64-i686-opus` (32 bit) resp.
                      `pacman -S mingw64/mingw-w64-x86_64-opus` (64 bit)
  * Linux -> apt: `sudo apt-get install libopus-dev`

2. Set the `AOO_LOCAL_OPUS` CMake variable to `OFF` (see below)

b) Use local Opus library (default)

1. Make sure that `AOO_LOCAL_OPUS` CMake variable is `ON` (default).

2. Now Opus will be included in the project and you can configure it as needed.

   **NOTE**: by default Opus will be built as a static library.
   If `BUILD_SHARED_LIBS` or `AOO_BUILD_SHARED_LIBRARY` is `ON`,
   both AOO and Opus will be built as shared libraries.


#### PortAudio

PortAudio is only required for the examples (`AOO_BUILD_EXAMPLES`).


#### macOS

By default, the minimum macOS deployment target is OSX 10.13. You may choose a *higher* version by setting the `CMAKE_OSX_DEPLOYMENT_TARGET` CMake variable.


#### Build

1. In the "aoo" folder create a subfolder named "build".
2. Navigate to `build` and execute `cmake .. <options>`. Available options are listed below.
3. Build the project with `cmake --build .`
4. Install the project with `cmake --build . --target install/strip`


#### CMake options

CMake options are set with the following syntax:
`cmake .. -D<name1>=<value1> -D<name2>=<value2>` etc.

**HINT**: setting options is much more convenient in a graphical interface like `cmake-gui`.
(You might need to install it seperately.)

These are the most important project options:

* `CMAKE_BUILD_TYPE` (STRING) - Choose one of the following build types:
   "Release", "RelMinSize", "RelWithDebInfo", "Debug". Default: "Release".

* `CMAKE_INSTALL_PREFIX` (PATH) - Where to install the AOO C/C++ library.

* `AOO_BUILD_STATIC_LIBRARY` (BOOL) - Build static AOO library. Default: `ON`.

* `AOO_BUILD_SHARED_LIBRARY` (BOOL) - Build shared AOO library. Default: `ON`.

* `AOO_BUILD_PD_EXTERNAL` (BOOL) - Build the Pd external. Default: `ON`.

* `AOO_BUILD_SERVER` (BOOL) - Build the `aooserver` command line program. Default: `ON`

* `AOO_USE_OPUS` (BOOL) - Enable/disable built-in Opus support

* `AOO_LOG_LEVEL` (STRING) - Choose one of the following log levels:
   "None", "Error", "Warning", "Verbose", "Debug". Default: "Warning".

* `AOO_STATIC_LIBS` (BOOL) - Linux and MinGW only:
   Link statically with `libgcc`, `libstdc++` and `libpthread`. Makes sure that the
   resulting binaries don't depend on specific system library versions. Default: `ON`.

* `AOO_NATIVE` (BOOL) - optimize for this particular machine.
  NB: the resulting binaries are not portable and might not run on other machines!

* `AOO_NET` (BOOL) - Build with integrated networking support (`AooClient` and `AooServer`).
  Disable it if you don't need it and want to reduce code size.
  **NOTE**: It is required for the Pd external. Default: `ON`.


`cmake-gui` will show all available options. Alternatively, run `cmake . -LH` from the `build` folder.

---

### Footnotes

[^CMake]: https://cmake.org/

[^Git]: https://git-scm.com/

[^IEM]: http://iem.at/

[^Jaeger]: https://phaidra.kug.ac.at/view/o:11413

[^LAC14]: see docu/lac2014_aoo.pdf

[^Opus]: https://opus-codec.org/

[^OSC]: http://opensoundcontrol.org/

[^Pd]: http://puredata.info/

[^Udp]: https://en.wikipedia.org/wiki/
UDP_hole_punching

[^VICE]: https://iaem.at/projekte/ice/overview

[^VRR]: https://vrr.iem.at/

[^0xFF]: http://graz.funkfeuer.at/
