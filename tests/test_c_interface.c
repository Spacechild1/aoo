#include "aoo/aoo.h"
#include "aoo/aoo_source.h"
#include "aoo/aoo_sink.h"
#include "aoo/codec/aoo_pcm.h"
#if AOO_NET
# include "aoo/aoo_server.h"
# include "aoo/aoo_client.h"
#endif

#define BUFFERSIZE 64
#define SAMPLERATE 48000

AooSample input[2][BUFFERSIZE];
AooSample output[2][BUFFERSIZE];

int main(int argc, const char * arg[]) {
    AooSample *inChannels[2] = { input[0], input[1] };
    AooSample *outChannels[2] = { output[0], output[1] };
    AooNtpTime time;
    AooSource *source;
    AooSink *sink;
#if AOO_NET
    AooClient *client;
    AooServer *server;
#endif
    AooFormatPcm format;

    AooSettings settings = AOO_SETTINGS_INIT();
    aoo_initialize(&settings);

    source = AooSource_new(0);
    sink = AooSink_new(0);
#if AOO_NET
    client = AooClient_new();
    server = AooServer_new();
#endif

    AooSource_setup(source, 2, SAMPLERATE, BUFFERSIZE, 0);
    AooSink_setup(sink, 2, SAMPLERATE, BUFFERSIZE, 0);

    AooFormatPcm_init(&format, 2, SAMPLERATE, BUFFERSIZE, kAooPcmInt24);

    time = aoo_getCurrentNtpTime();

    AooSource_process(source, inChannels, BUFFERSIZE, time);
    AooSink_process(sink, outChannels, BUFFERSIZE, time, NULL, NULL);

    AooSource_free(source);
    AooSink_free(sink);
#if AOO_NET
    AooClient_free(client);
    AooServer_free(server);
#endif

    aoo_terminate();

    return 0;
}
