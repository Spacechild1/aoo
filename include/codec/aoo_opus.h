/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief Opus codec settings
 */

#pragma once

#include "aoo_config.h"
#include "aoo_defines.h"
#include "aoo_types.h"

#include "opus_multistream.h"

#include <string.h>

struct AooSource;

/** \cond DO_NOT_DOCUMENT */
AOO_API AooError AOO_CALL AooSource_codecControl(
        AooSource *source, const AooChar *codec,
        AooCtl ctl, AooIntPtr index,
        void *data, AooSize size);
/** \endcond */

AOO_PACK_BEGIN

/*---------------------------------------------------*/

/** \brief name of Opus codec */
#define kAooCodecOpus "opus"

/** \brief Opus codec format */
typedef struct AooFormatOpus
{
    AooFormat header;
    /** Opus application type.
     * Possible values:
     * `OPUS_APPLICATION_VOIP`,
     * `OPUS_APPLICATION_AUDIO` or
     * `OPUS_APPLICATION_RESTRICTED_LOWDELAY`
     */
    opus_int32 applicationType;
} AooFormatOpus;

/*--------------------------------------------------*/

/** \brief initialize AooFormatOpus struct */
AOO_INLINE void AooFormatOpus_init(
        AooFormatOpus *fmt,
        AooInt32 numChannels, AooInt32 sampleRate,
        AooInt32 blockSize, opus_int32 applicationType)
{
    AOO_STRUCT_INIT(&fmt->header, AooFormatOpus, applicationType);
    fmt->header.numChannels = numChannels;
    fmt->header.sampleRate = sampleRate;
    fmt->header.blockSize = blockSize;
    fmt->applicationType = applicationType;
    strcpy(fmt->header.codecName, kAooCodecOpus);
}

/*----------- helper functions for common controls ------------------*/

/** \brief set bitrate
 *
 * \param src the AOO source
 * \param sink the AOO sink (`NULL` for all sinks)
 * \param bitrate bits/s, `OPUS_BITRATE_MAX` or `OPUS_AUTO`
 */
AOO_INLINE AooError AooSource_setOpusBitrate(
        AooSource *src, const AooEndpoint *sink, opus_int32 bitrate) {
    return AooSource_codecControl(src, kAooCodecOpus,
                OPUS_SET_BITRATE_REQUEST, (AooIntPtr)sink,
                &bitrate, sizeof(bitrate));
}

/** \brief get bitrate */
AOO_INLINE AooError AooSource_getOpusBitrate(
        AooSource *src, const AooEndpoint *sink, opus_int32 *bitrate) {
    return AooSource_codecControl(src, kAooCodecOpus,
                OPUS_GET_BITRATE_REQUEST, (AooIntPtr)sink,
                bitrate, sizeof(*bitrate));
}

/** \brief set complexity
 *
 * \param src the AOO source
 * \param sink the AOO sink (`NULL` for all sinks)
 * \param complexity the complexity (0-10 or `OPUS_AUTO`)
 */
AOO_INLINE AooError AooSource_setOpusComplexity(
        AooSource *src, const AooEndpoint *sink, opus_int32 complexity) {
    return AooSource_codecControl(src, kAooCodecOpus,
                OPUS_SET_COMPLEXITY_REQUEST, (AooIntPtr)sink,
                &complexity, sizeof(complexity));
}

/** \brief get complexity */
AOO_INLINE AooError AooSource_getOpusComplexity(
        AooSource *src, const AooEndpoint *sink, opus_int32 *complexity) {
    return AooSource_codecControl(src, kAooCodecOpus,
                OPUS_GET_COMPLEXITY_REQUEST, (AooIntPtr)sink,
                complexity, sizeof(*complexity));
}

/** \brief set signal type
 *
 * \param src the AOO source
 * \param sink the AOO sink (`NULL` for all sinks)
 * \param signalType the signal type
 * (`OPUS_SIGNAL_VOICE`, `OPUS_SIGNAL_MUSIC` or `OPUS_AUTO`)
 */
AOO_INLINE AooError AooSource_setOpusSignalType(
        AooSource *src, const AooEndpoint *sink, opus_int32 signalType) {
    return AooSource_codecControl(src, kAooCodecOpus,
                OPUS_SET_SIGNAL_REQUEST, (AooIntPtr)sink,
                &signalType, sizeof(signalType));
}

/** \brief get signal type */
AOO_INLINE AooError AooSource_getOpusSignalType(
        AooSource *src, const AooEndpoint *sink, opus_int32 *signalType) {
    return AooSource_codecControl(src, kAooCodecOpus,
                OPUS_GET_SIGNAL_REQUEST, (AooIntPtr)sink,
                signalType, sizeof(*signalType));
}

/*--------------------------------------------------------------------*/

AOO_PACK_END
