/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief C interface for AOO source
 */

#pragma once

#include "aoo_config.h"
#include "aoo_controls.h"
#include "aoo_defines.h"
#include "aoo_events.h"
#include "aoo_types.h"

/** \cond DO_NOT_DOCUMENT */
typedef struct AooSource AooSource;
/** \endcond */

/** \brief create a new AOO source instance
 *
 * \param id the ID
 * \return new AooSource instance on success; `NULL` on failure
 */
AOO_API AooSource * AOO_CALL AooSource_new(AooId id);

/** \brief destroy the AOO source instance */
AOO_API void AOO_CALL AooSource_free(AooSource *source);

/** \copydoc AooSource::setup() */
AOO_API AooError AOO_CALL AooSource_setup(
        AooSource *source, AooInt32 numChannels, AooSampleRate sampleRate,
        AooInt32 maxBlockSize, AooSetupFlags flags);

/** \copydoc AooSource::handleMessage() */
AOO_API AooError AOO_CALL AooSource_handleMessage(
        AooSource *source, const AooByte *data, AooInt32 size,
        const void *address, AooAddrSize addrlen);

/** \copydoc AooSource::send() */
AOO_API AooError AOO_CALL AooSource_send(
        AooSource *source, AooSendFunc fn, void *user);

/** \copydoc AooSource::addStreamMessage() */
AOO_API AooError AOO_CALL AooSource_addStreamMessage(
        AooSource *source, const AooStreamMessage *message);

/** \copydoc AooSource::process() */
AOO_API AooError AOO_CALL AooSource_process(
        AooSource *source, AooSample **data, AooInt32 numSamples, AooNtpTime t);

/** \copydoc AooSource::setEventHandler() */
AOO_API AooError AOO_CALL AooSource_setEventHandler(
        AooSource *source, AooEventHandler fn, void *user, AooEventMode mode);

/** \copydoc AooSource::eventsAvailable() */
AOO_API AooBool AOO_CALL AooSource_eventsAvailable(AooSource *source);

/** \copydoc AooSource::pollEvents() */
AOO_API AooError AOO_CALL AooSource_pollEvents(AooSource *source);

/** \copydoc AooSource::startStream() */
AOO_API AooError AOO_CALL AooSource_startStream(
        AooSource *source, AooInt32 sampleOffset, const AooData *metadata);

/** \copydoc AooSource::stopStream() */
AOO_API AooError AOO_CALL AooSource_stopStream(
        AooSource *source, AooInt32 sampleOffset);

/** \copydoc AooSource::addSink() */
AOO_API AooError AOO_CALL AooSource_addSink(
        AooSource *source, const AooEndpoint *sink, AooBool active);

/** \copydoc AooSource::removeSink() */
AOO_API AooError AOO_CALL AooSource_removeSink(
        AooSource *source, const AooEndpoint *sink);

/** \copydoc AooSource::removeAll() */
AOO_API AooError AOO_CALL AooSource_removeAll(AooSource *source);

/** \copydoc AooSource::handleInvite() */
AOO_API AooError AOO_CALL AooSource_handleInvite(
        AooSource *source, const AooEndpoint *sink, AooId token, AooBool accept);

/** \copydoc AooSource::handleUninvite() */
AOO_API AooError AOO_CALL AooSource_handleUninvite(
        AooSource *source, const AooEndpoint *sink, AooId token, AooBool accept);

/** \copydoc AooSource::control() */
AOO_API AooError AOO_CALL AooSource_control(
        AooSource *source, AooCtl ctl, AooIntPtr index, void *data, AooSize size);

/** \copydoc AooSource::codecControl() */
AOO_API AooError AOO_CALL AooSource_codecControl(
        AooSource *source, const AooChar *codec, AooCtl ctl, AooIntPtr index,
        void *data, AooSize size);

/*--------------------------------------------*/
/*         type-safe control functions        */
/*--------------------------------------------*/

/** \copydoc AooSource::activate() */
AOO_INLINE AooError AooSource_activate(
        AooSource *source, const AooEndpoint *sink, AooBool active)
{
    return AooSource_control(source, kAooCtlActivate, (AooIntPtr)sink, AOO_ARG(active));
}

/** \copydoc AooSource::isActive() */
AOO_INLINE AooError AooSource_isActive(
        AooSource *source, const AooEndpoint *sink, AooBool *active)
{
    return AooSource_control(source, kAooCtlIsActive, (AooIntPtr)sink, AOO_ARG(*active));
}

/** \copydoc AooSource::reset() */
AOO_INLINE AooError AooSource_reset(AooSource *source)
{
    return AooSource_control(source, kAooCtlReset, 0, NULL, 0);
}

/** \copydoc AooSource::setFormat() */
AOO_INLINE AooError AooSource_setFormat(AooSource *source, AooFormat *format)
{
    return AooSource_control(source, kAooCtlSetFormat, 0, AOO_ARG(*format));
}

/** \copydoc AooSource::getFormat() */
AOO_INLINE AooError AooSource_getFormat(AooSource *source, AooFormatStorage *format)
{
    return AooSource_control(source, kAooCtlGetFormat, 0, AOO_ARG(*format));
}

/** \copydoc AooSource::setId() */
AOO_INLINE AooError AooSource_setId(AooSource *source, AooId id)
{
    return AooSource_control(source, kAooCtlSetId, 0, AOO_ARG(id));
}

/** \copydoc AooSource::getId() */
AOO_INLINE AooError AooSource_getId(AooSource *source, AooId *id)
{
    return AooSource_control(source, kAooCtlGetId, 0, AOO_ARG(*id));
}

/** \copydoc AooSource::setBufferSize() */
AOO_INLINE AooError AooSource_setBufferSize(AooSource *source, AooSeconds s)
{
    return AooSource_control(source, kAooCtlSetBufferSize, 0, AOO_ARG(s));
}

/** \copydoc AooSource::getBufferSize() */
AOO_INLINE AooError AooSource_getBufferSize(AooSource *source, AooSeconds *s)
{
    return AooSource_control(source, kAooCtlGetBufferSize, 0, AOO_ARG(*s));
}

/** \copydoc AooSource::reportXRun() */
AOO_INLINE AooError AooSource_reportXRun(AooSource *source, AooInt32 numSamples)
{
    return AooSource_control(source, kAooCtlReportXRun, 0, AOO_ARG(numSamples));
}

/** \copydoc AooSource::setResampleMethod() */
AOO_INLINE AooError AooSource_setResampleMethod(AooSource *source, AooResampleMethod mode)
{
    return AooSource_control(source, kAooCtlSetResampleMethod, 0, AOO_ARG(mode));
}

/** \copydoc AooSource::getResampleMethod() */
AOO_INLINE AooError AooSource_getResampleMethod(AooSource *source, AooResampleMethod *mode)
{
    return AooSource_control(source, kAooCtlGetResampleMethod, 0, AOO_ARG(*mode));
}

/** \copydoc AooSource::setDynamicResampling() */
AOO_INLINE AooError AooSource_setDynamicResampling(AooSource *source, AooBool b)
{
    return AooSource_control(source, kAooCtlSetDynamicResampling, 0, AOO_ARG(b));
}

/** \copydoc AooSource::getDynamicResampling() */
AOO_INLINE AooError AooSource_getDynamicResampling(AooSource *source, AooBool *b)
{
    return AooSource_control(source, kAooCtlGetDynamicResampling, 0, AOO_ARG(*b));
}

/** \copydoc AooSource::getRealSampleRate() */
AOO_INLINE AooError AooSource_getRealSampleRate(AooSource *source, AooSample *sr)
{
    return AooSource_control(source, kAooCtlGetRealSampleRate, 0, AOO_ARG(*sr));
}

/** \copydoc AooSource::setDllBandwidth() */
AOO_INLINE AooError AooSource_setDllBandwidth(AooSource *source, double q)
{
    return AooSource_control(source, kAooCtlSetDllBandwidth, 0, AOO_ARG(q));
}

/** \copydoc AooSource::getDllBandwidth() */
AOO_INLINE AooError AooSource_getDllBandwidth(AooSource *source, double *q)
{
    return AooSource_control(source, kAooCtlGetDllBandwidth, 0, AOO_ARG(*q));
}

/** \copydoc AooSource::resetDll() */
AOO_INLINE AooError AooSource_resetDll(AooSource *source)
{
    return AooSource_control(source, kAooCtlResetDll, 0, NULL, 0);
}

/** \copydoc AooSource::setPacketSize() */
AOO_INLINE AooError AooSource_setPacketSize(AooSource *source, AooInt32 n)
{
    return AooSource_control(source, kAooCtlSetPacketSize, 0, AOO_ARG(n));
}

/** \copydoc AooSource::getPacketSize() */
AOO_INLINE AooError AooSource_getPacketSize(AooSource *source, AooInt32 *n)
{
    return AooSource_control(source, kAooCtlGetPacketSize, 0, AOO_ARG(*n));
}

/** \copydoc AooSource::setPingInterval() */
AOO_INLINE AooError AooSource_setPingInterval(AooSource *source, AooSeconds s)
{
    return AooSource_control(source, kAooCtlSetPingInterval, 0, AOO_ARG(s));
}

/** \copydoc AooSource::getPingInterval() */
AOO_INLINE AooError AooSource_getPingInterval(AooSource *source, AooSeconds *s)
{
    return AooSource_control(source, kAooCtlGetPingInterval, 0, AOO_ARG(*s));
}

/** \copydoc AooSource::setResendBufferSize() */
AOO_INLINE AooError AooSource_setResendBufferSize(AooSource *source, AooSeconds s)
{
    return AooSource_control(source, kAooCtlSetResendBufferSize, 0, AOO_ARG(s));
}

/** \copydoc AooSource::getResendBufferSize() */
AOO_INLINE AooError AooSource_getResendBufferSize(AooSource *source, AooSeconds *s)
{
    return AooSource_control(source, kAooCtlGetResendBufferSize, 0, AOO_ARG(*s));
}

/** \copydoc AooSource::setRedundancy() */
AOO_INLINE AooError AooSource_setRedundancy(AooSource *source, AooInt32 n)
{
    return AooSource_control(source, kAooCtlSetRedundancy, 0, AOO_ARG(n));
}

/** \copydoc AooSource::getRedundancy() */
AOO_INLINE AooError AooSource_getRedundancy(AooSource *source, AooInt32 *n)
{
    return AooSource_control(source, kAooCtlGetRedundancy, 0, AOO_ARG(*n));
}

/** \copydoc AooSource::setBinaryFormat() */
AOO_INLINE AooError AooSource_setBinaryFormat(AooSource *source, AooBool b)
{
    return AooSource_control(source, kAooCtlSetBinaryFormat, 0, AOO_ARG(b));
}

/** \copydoc AooSource::getBinaryFormat() */
AOO_INLINE AooError AooSource_getBinaryFormat(AooSource *source, AooBool *b)
{
    return AooSource_control(source, kAooCtlGetBinaryFormat, 0, AOO_ARG(*b));
}

/** \copydoc AooSource::setStreamTimeSendInterval() */
AOO_INLINE AooError AooSource_setStreamTimeSendInterval(AooSource *source, AooSeconds s)
{
    return AooSource_control(source, kAooCtlSetStreamTimeSendInterval, 0, AOO_ARG(s));
}

/** \copydoc AooSource::getStreamTimeSendInterval() */
AOO_INLINE AooError AooSource_getStreamTimeSendInterval(AooSource *source, AooSeconds *s)
{
    return AooSource_control(source, kAooCtlGetStreamTimeSendInterval, 0, AOO_ARG(*s));
}

/** \copydoc AooSource::setSinkChannelOffset() */
AOO_INLINE AooError AooSource_setSinkChannelOffset(
        AooSource *source, const AooEndpoint *sink, AooInt32 onset)
{
    return AooSource_control(source, kAooCtlSetSinkChannelOffset, (AooIntPtr)sink, AOO_ARG(onset));
}

/** \copydoc AooSource::getSinkChannelOffset() */
AOO_INLINE AooError AooSource_getSinkChannelOffset(
        AooSource *source, const AooEndpoint *sink, AooInt32 *onset)
{
    return AooSource_control(source, kAooCtlGetSinkChannelOffset, (AooIntPtr)sink, AOO_ARG(*onset));
}
