AOO (audio over OSC)
=====================

# Introduction

AOO is a lightweight and flexible peer-to-peer audio streaming and messaging solution, using Open Sound Control [^OSC] as the underlying transport protocol.

It is fundamentally connectionless and allows to send audio in real time and on demand between arbitrary network endpoints.

The C/C++ library can be easily embedded in host applications or plugins. It even runs on embedded devices, such as the ESP32. In addition, the project contains a Pure Data external, and soon also a SuperCollider extension.

The following article provides a high-level overview: https://www.soundingfuture.com/en/article/aoo-low-latency-peer-peer-audio-streaming-and-messaging

**WARNING**: AOO is still alpha software, there are breaking changes between pre-releases!

---

# Selected features

- Peer-to-peer audio networks (IPv4 and IPv6) of any topology with arbitrary ad-hoc connections.

- Each IP endpoint can have multiple so-called 'sources' (= senders) and 'sinks' (= receivers), each with their own ID.

- Sources can send audio to several sinks.
  Conversely, sinks can listen to several sources, summing the signals at the output.

- AOO sinks can listen to several sources at the same time, summing the signals.

- AOO is connectionless: streams can start/stop at any time, enabling a "message-based audio" approach.

- AOO sinks can "invite" and "uninvite" sources, i.e. ask them to send resp. stop sending audio.
  The source may accept the (un)invitation or decline it.

- Sources can send arbitrary messages with sample-accurate timestamps together with the audio data. For example, this may be used to embed control data, timing information or MIDI messages.

- Sources and sinks can operate at different blocksizes and samplerates. Streams are resampled and reblocked automatically.

- Clock differences between machines can be adjusted automatically with dynamic resampling.

- Support for different audio codecs. Currently, only PCM (uncompressed) and Opus (compressed) are implemented,
  but additional codecs can be added with the codec plugin API.

- Network jitter, packet reordering and packet loss are handled by the sink jitter buffer deals. The latency can be adjusted dynamically.

- Sinks can ask sources to resend dropped packets.

- Several diagnostic events about packet loss, resent packets, etc.

- A connection server (`aooserver`) facilitates peer-to-peer communication in local networks or over the public internet.

- AOO clients can send each other timestamped messages with optional reliable transmission.

---

# History

The vision of AOO was first presented in 2009 by Winfried Ritsch together with a proof-of-concept for embedded devices.
In 2010 it has been implemented by Wolfgang Jäger as a library (v1.0-b2) with externals for Pure Data (Pd) [^Jaeger], but its practical use remained limited.
AOO 1.0 is the topic of Winfried Ritsch's paper "towards message based audio systems" which he presented at LAC 2014 [^LAC14].

In 2020 AOO has been reimplemented from scratch by Christof Ressi.
The first draft version has been developed in February 2020 for a network streaming project at Kunsthaus Graz with Bill Fontana, using an independent wireless network infrastructure FunkFeuer Graz [^0xFF].
It was subsequently used for the Virtual Rehearsal Room project [^VRR] which allowed musicians at the University of Arts Graz to rehearse and perform remotely during the early Covid pandemic.
The first public pre-release of AOO 2.0 has been published in April 2020.
Since then it has been used in several art projects and software applications.

---

# Content

`aoo`      - AOO C/C++ library source code

`cmake`    - CMake helper files

`common`   - shared code

`deps`     - dependencies

`doc`      - documentation

`doxygen`  - doxygen documentation

`examples` - examples

`include`  - public headers

`pd`       - Pd external

`sc`       - SC extension

`server`   - the `aooserver` command line program

`tests`    - test suite

---

# C/C++ library

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

---

# Pd external

Objects:

`[aoo_send~]` - send an AOO stream (with integrated threaded network IO)

`[aoo_receive~]` - receive one or more AOO streams (with integrated threaded network IO)

`[aoo_client]` - connect to AOO peers over the public internet or in a local network

`[aoo_server]` - AOO connection server

For documentation see the corresponding help patches.

The Pd external is available on Deken (in Pd -> Help -> "Find externals" search for "aoo".)

---

# SC extension

TODO

---

# The aooserver command line program

If you want to host your own (private or public) AOO server, you only have to run `aooserver`
on the command line or as a service and make sure that clients can connect to your machine.

Run `aooserver -h` to see all available options.

---

# Footnotes

[^Jaeger]: https://phaidra.kug.ac.at/view/o:11413

[^LAC14]: http://lac.linuxaudio.org/2014/papers/36.pdf

[^Opus]: https://opus-codec.org/

[^OSC]: http://opensoundcontrol.org/

[^VRR]: https://vrr.iem.at/

[^0xFF]: http://graz.funkfeuer.at/
