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

The `aoo` library is written in C++17 and provides a pure C API as well as a C++ API. The public API headers are contained in the [`include`](include) directory.

For build instructions please see [INSTALL.md](INSTALL.md).

The C API may be used for creating bindings to other languages, such as Rust, Python, Java or C#.

**NOTE**:
In general, C++ does not have a standardized ABI. However, by following certain COM idioms, we provide portable C++ interfaces. This means you can use a pre-build version of the `aoo` shared library, even though it may have been built with a different compiler (version).

The library features four object classes:

- `AooSource` - AOO source object, see [`aoo_source.hpp`](include/aoo_source.hpp) resp. [`aoo_source.h`](include/aoo_source.h).

- `AooSink` - AOO sink object, see [`aoo_sink.hpp`](include/aoo_sink.hpp) resp. [`aoo_sink.h`](include/aoo_sink.h).

- `AooClient` - AOO client object, see [`aoo_client.hpp`](include/aoo_client.hpp) resp. [`aoo_client.h`](include/aoo_client.h).

- `AooServer` - AOO UDP hole punching server, see [`aoo_server.hpp`](include/aoo_server.hpp) resp. [`aoo_server.h`](include/aoo_server.h).


To generate the API documentation you need to have `doxygen` installed. Just run the `doxygen`
command in the toplevel folder and it will store the generated files in the `doxygen` folder.
You can view the documentation in a standard web browser by opening `doxygen/html/index.html`.

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
