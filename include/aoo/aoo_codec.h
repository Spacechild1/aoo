/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief AOO codec API
 *
 * This header contains the API for adding new audio codecs to the AOO library.
 */

#pragma once

#include "aoo_config.h"
#include "aoo_defines.h"
#include "aoo_types.h"

AOO_PACK_BEGIN

/*-----------------------------------------------------*/

/** \brief base class for all codec classes */
typedef struct AooCodec
{
    struct AooCodecInterface *cls;
} AooCodec;

/** \brief codec constructor */
typedef AooCodec * (AOO_CALL *AooCodecNewFunc)(void);

/** \brief codec destructor */
typedef void (AOO_CALL *AooCodecFreeFunc)(AooCodec *codec);

/** \brief codec setup function */
typedef AooError (AOO_CALL *AooCodecSetupFunc)(
        /** the encoder/decoder instance */
        AooCodec *codec,
        /** the desired format; validated and updated on success */
        AooFormat *format
);

/** \brief encode audio samples to bytes */
typedef AooError (AOO_CALL *AooCodecEncodeFunc)(
        /** the encoder instance */
        AooCodec *encoder,
        /** [in] input samples (interleaved) */
        const AooSample *inSamples,
        /** [in] number of samples */
        AooInt32 numSamples,
        /** [out] output buffer */
        AooByte *outData,
        /** [in,out] max. buffer size in bytes
         * (updated to actual size) */
        AooInt32 *outSize
);

/** \brief decode bytes to samples */
typedef AooError (AOO_CALL *AooCodecDecodeFunc)(
        /** the decoder instance */
        AooCodec *decoder,
        /** [in] input data */
        const AooByte *inData,
        /** [in] input data size in bytes */
        AooInt32 numBytes,
        /** [out] output samples (interleaved) */
        AooSample *outSamples,
        /** [in,out] max. number of samples
         * (updated to actual number) */
        AooInt32 *numSamples
);

/** \brief AOO codec controls
 *
 * Negative values are reserved for generic controls;
 * codec specific controls must be positiv.
 */
AOO_ENUM(AooCodecCtl)
{
    /** reset the codec state (`NULL`) */
    kAooCodecCtlReset = -1000,
    /** get encoding/decoding latency in samples (AooInt32) */
    kAooCodecCtlGetLatency
};

/** \brief codec control function */
typedef AooError (AOO_CALL *AooCodecControlFunc)(
        /** the encoder/decoder instance */
        AooCodec *codec,
        /** the control constant */
        AooCodecCtl ctl,
        /** pointer to value */
        void *data,
        /** the value size */
        AooSize size
);

/** \brief serialize format extension
 *
 * (= everything after the AooFormat header).
 * On success, write the format extension to the given buffer.
 */
typedef AooError (AOO_CALL *AooCodecSerializeFunc)(
        /** [in] the source format */
        const AooFormat *format,
        /** [out] extension buffer; `NULL` returns the required buffer size. */
        AooByte *buffer,
        /** [in,out] max. buffer size; updated to actual resp. required size */
        AooInt32 *bufsize
);

/** \brief deserialize format extension
 *
 * (= everything after the AooFormat header).
 * On success, write the format extension to the given format structure.
 *
 * \note This function should *not* update the `structSize` member of the `format` argument.
 */
typedef AooError (AOO_CALL *AooCodecDeserializeFunc)(
        /** [in] the extension buffer */
        const AooByte *buffer,
        /** [in] the extension buffer size */
        AooInt32 bufsize,
        /** [out] destination format structure; `NULL` returns the required format size */
        AooFormat *format,
        /** max. format size; updated to actual resp. required size */
        AooInt32 *fmtsize
);

/** \brief interface to be implemented by AOO codec classes */
typedef struct AooCodecInterface
{
    AooSize structSize;
    const AooChar * name;
    /* encoder methods */
    AooCodecNewFunc encoderNew;
    AooCodecFreeFunc encoderFree;
    AooCodecSetupFunc encoderSetup;
    AooCodecControlFunc encoderControl;
    AooCodecEncodeFunc encoderEncode;
    /* decoder methods */
    AooCodecNewFunc decoderNew;
    AooCodecFreeFunc decoderFree;
    AooCodecSetupFunc decoderSetup;
    AooCodecControlFunc decoderControl;
    AooCodecDecodeFunc decoderDecode;
    /* free functions */
    AooCodecSerializeFunc serialize;
    AooCodecDeserializeFunc deserialize;
} AooCodecInterface;

#define AOO_CODEC_INTERFACE_SIZE \
    AOO_STRUCT_SIZE(AooCodecInterface, deserialize)

/*----------------- helper functions ----------------------*/

/** \brief setup encoder
 *  \see AooCodecSetupFunc */
AOO_INLINE AooError AooEncoder_setup(
    AooCodec *enc, AooFormat *format)
{
    return enc->cls->encoderSetup(enc, format);
}

/** \brief encode audio samples to bytes
 *  \see AooCodecEncodeFunc */
AOO_INLINE AooError AooEncoder_encode(
        AooCodec *enc, const AooSample *input, AooInt32 numSamples,
        AooByte *output, AooInt32 *numBytes)
{
    return enc->cls->encoderEncode(enc, input, numSamples, output, numBytes);
}

/** \brief control encoder instance
 *  \see AooCodecControlFunc */
AOO_INLINE AooError AooEncoder_control(
        AooCodec *enc, AooCodecCtl ctl, void *data, AooSize size)
{
    return enc->cls->encoderControl(enc, ctl, data, size);
}

/** \brief reset encoder state */
AOO_INLINE AooError AooEncoder_reset(AooCodec *enc)
{
    return enc->cls->encoderControl(enc, kAooCodecCtlReset, NULL, 0);
}

/** \brief setup decoder
 *  \see AooCodecSetupFunc */
AOO_INLINE AooError AooDecoder_setup(
    AooCodec *dec, AooFormat *format)
{
    return dec->cls->decoderSetup(dec, format);
}

/** \brief decode bytes to audio samples
 *  \see AooCodecDecodeFunc */
AOO_INLINE AooError AooDecoder_decode(
        AooCodec *dec, const AooByte *input, AooInt32 numBytes,
        AooSample *output, AooInt32 *numSamples)
{
    return dec->cls->decoderDecode(dec, input, numBytes, output, numSamples);
}

/** \brief control decoder instance
 *  \see AooCodecControlFunc */
AOO_INLINE AooError AooDecoder_control(
        AooCodec *dec, AooCodecCtl ctl, void *data, AooSize size)
{
    return dec->cls->decoderControl(dec, ctl, data, size);
}

/** \brief reset decoder state */
AOO_INLINE AooError AooDecoder_reset(AooCodec *dec)
{
    return dec->cls->decoderControl(dec, kAooCodecCtlReset, NULL, 0);
}

/*----------------- register codecs -----------------------*/

/** \brief function for registering codecs */
typedef AooError (AOO_CALL *AooCodecRegisterFunc)(
        const AooCodecInterface *cls
);

/** \brief host interface passed to codec plugins */
typedef struct AooCodecHostInterface
{
    AooSize structSize;
    AooCodecRegisterFunc registerCodec;
    AooAllocFunc alloc;
    AooLogFunc log;
} AooCodecHostInterface;

/** \brief global host interface instance */
AOO_API const AooCodecHostInterface * aoo_getCodecHostInterface(void);

/** \brief register an external codec plugin */
AOO_API AooError AOO_CALL aoo_registerCodec(const AooCodecInterface *codec);

/** \brief type of entry function for codec plugin module
 *
 * \note AOO doesn't support dynamic plugin loading out of the box,
 * but it is quite easy to implement on your own.
 * You just have to put one or more codecs in a shared library and export
 * a single function of type AooCodecLoadFunc with the name `aoo_load`:
 *
 *     void aoo_load(const AooCodecHostInterface *interface);
 *
 * In your host application, you would then scan directories for shared libraries,
 * check if they export a function named `aoo_load`, and if yes, call it with a
 * pointer to the host interface table.
 */
typedef AooError (AOO_CALL *AooCodecLoadFunc)
        (const AooCodecHostInterface *);

/** \brief type of exit function for codec plugin module
 *
 * Your codec plugin can optionally export a function `aoo_unload` which should be
 * called before program exit to properly release shared resources.
 */
typedef AooError (AOO_CALL *AooCodecUnloadFunc)(void);


/*-------------------------------------------------------------------------------------*/

AOO_PACK_END
