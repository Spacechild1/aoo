/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief AOO event types
 */

#pragma once

#include "aoo_config.h"
#include "aoo_defines.h"
#include "aoo_types.h"

AOO_PACK_BEGIN

/*--------------------------------------------*/

/** \brief AOO source/sink event types */
AOO_ENUM(AooEventType)
{
    /** generic error event */
    kAooEventError = 0,
    /*----------------------------------*/
    /*     AooSource/AooSink events     */
    /*----------------------------------*/
    /** AooSource: received ping from sink */
    kAooEventSinkPing,
    /** AooSink: received ping from source */
    kAooEventSourcePing,
    /** AooSource: invited by sink */
    kAooEventInvite,
    /** AooSource: uninvited by sink */
    kAooEventUninvite,
    /** AooSource: sink added */
    kAooEventSinkAdd,
    /** AooSource: sink removed */
    kAooEventSinkRemove,
    /** AooSink: source added */
    kAooEventSourceAdd,
    /** AooSink: source removed */
    kAooEventSourceRemove,
    /** AooSink: stream started */
    kAooEventStreamStart,
    /** AooSink: stream stopped */
    kAooEventStreamStop,
    /** AooSink: stream changed state */
    kAooEventStreamState,
    /** AooSink: stream time stamp */
    kAooEventStreamTime,
    /** AooSink: source format changed */
    kAooEventFormatChange,
    /** AooSink: invitation has been declined */
    kAooEventInviteDecline,
    /** AooSink: invitation timed out */
    kAooEventInviteTimeout,
    /** AooSink: uninvitation timed out */
    kAooEventUninviteTimeout,
    /** AooSink: buffer overrun */
    kAooEventBufferOverrun,
    /** AooSink: buffer underrun */
    kAooEventBufferUnderrun,
    /** AooSink: blocks had to be skipped/dropped */
    kAooEventBlockDrop,
    /** AooSink: blocks have been resent */
    kAooEventBlockResend,
    /** AooSink: empty blocks caused by source xrun */
    kAooEventBlockXRun,
    /** AooSource: frames have been resent */
    kAooEventFrameResend,
    /*--------------------------------------------*/
    /*         AooClient/AooServer events         */
    /*--------------------------------------------*/
    /** AooClient: disconnected from server */
    kAooEventDisconnect = 1000,
    /** AooClient: received a server notification */
    kAooEventNotification,
    /** AooClient: ejected from a group */
    kAooEventGroupEject,
    /** AooClient: received ping (reply) from peer */
    kAooEventPeerPing,
    /** AooClient: peer state change */
    kAooEventPeerState,
    /** AooClient: peer handshake has started */
    kAooEventPeerHandshake,
    /** AooClient: peer handshake has timed out */
    kAooEventPeerTimeout,
    /** AooClient: peer has joined the group */
    kAooEventPeerJoin,
    /** AooClient: peer has left the group */
    kAooEventPeerLeave,
    /** AooClient: received message from peer */
    kAooEventPeerMessage,
    /** AooClient: peer has been updated */
    kAooEventPeerUpdate,
    /** AooClient: a group has been updated by a peer or by the server
     *  AooServer: a group has been updated by a user */
    kAooEventGroupUpdate,
    /** AooClient: our user has been updated by the server
     *  AooServer: a user has updated itself */
    kAooEventUserUpdate,
    /** AooServer: client logged in successfully or failed to do so */
    kAooEventClientLogin,
    /** AooServer: client logged out (normally or because of an error) */
    kAooEventClientLogout,
    /** AooServer: client error */
    kAooEventClientError,
    /** AooServer: a new group has been added (automatically) */
    kAooEventGroupAdd,
    /** AooServer: a group has been removed (automatically) */
    kAooEventGroupRemove,
    /** AooServer: a user has joined a group */
    kAooEventGroupJoin,
    /** AooServer: a user has left a group */
    kAooEventGroupLeave,
    /** start of user defined events (for custom AOO versions) */
    kAooEventCustom = 10000
};

/*--------------------------------------------*/

/** \brief common header of all event structs */
#define AOO_EVENT_HEADER            \
    /** \cond DO_NOT_DOCUMENT */    \
    AooEventType type;              \
    AooUInt32 structSize;           \
    /** \endcond */

/** \brief base event */
typedef struct AooEventBase
{
    AOO_EVENT_HEADER
} AooEventBase;

/** \brief error event */
typedef struct AooEventError
{
    AOO_EVENT_HEADER
    /** platform- or application-specific error code */
    AooInt32 errorCode;
    /** error message */
    const AooChar *errorMessage;
} AooEventError;

/*-------------------------------------------------*/
/*              AOO source/sink events             */
/*-------------------------------------------------*/

/** \brief generic source/sink event */
typedef struct AooEventEndpoint
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< source/sink endpoint */
} AooEventEndpoint;

/** \brief received ping (reply) from source
 *
 * \details The effective network round trip time (RTT) can be
 * calculated with the formula `(t4 - t1) - (t3 - t2)`.
 *
 * \note The clocks on both machines can diverge significantly,
 * so the end-to-end delay can *not* be calculated with `(t2 - t1)`
 * resp. `(t4 - t3)`! It can only be estimated, e.g. with `RTT * 0.5`.
 *
 * The time offset between source and sink can be estimated with
 * `((t2 - t1) + (t3 - t4)) * 0.5`. Together with AooEventStreamTime
 * this can  be used to synchronize multiple streams from different
 * machines.
 *
 * Ideally, you would filter all these values to account for network jitter.
 * See also https://en.wikipedia.org/wiki/Network_Time_Protocol#Clock_synchronization_algorithm
 */
typedef struct AooEventSourcePing
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< source endpoint */
    AooNtpTime t1; /**< sink send time */
    AooNtpTime t2; /**< source receive time */
    AooNtpTime t3; /**< source send time */
    AooNtpTime t4; /**< sink receive time */
} AooEventSourcePing;

/** \brief received ping (reply) from sink
 *
 *  \see AooEventSourcePing
 */
typedef struct AooEventSinkPing
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< sink endpoint */
    AooNtpTime t1; /**< source send time */
    AooNtpTime t2; /**< sink receive time */
    AooNtpTime t3; /**< sink send time */
    AooNtpTime t4; /**< source receive time */
    float packetLoss; /**< packet loss percentage (0.0 - 1.0) */
} AooEventSinkPing;

/** \brief (AooSink) a new source has been added */
typedef AooEventEndpoint AooEventSourceAdd;

/** \brief (AooSink) a source has been removed */
typedef AooEventEndpoint AooEventSourceRemove;

/** \brief (AooSource) a sink has been added */
typedef AooEventEndpoint AooEventSinkAdd;

/** \brief (AooSource) a sink has been removed */
typedef AooEventEndpoint AooEventSinkRemove;

/** \brief (AooSink) buffer overrun occurred */
typedef AooEventEndpoint AooEventBufferOverrun;

/** \brief (AooSink) buffer underrun occurred */
typedef AooEventEndpoint AooEventBufferUnderrun;

/** \brief (AooSink) a new stream has started */
typedef struct AooEventStreamStart
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< source endpoint */
    AooNtpTime tt; /**< stream start time (source-side) */
    const AooData *metadata; /**< optional stream metadata */
} AooEventStreamStart;

/** \brief (AooSink) a stream has stopped */
typedef AooEventEndpoint AooEventStreamStop;

/** \brief stream states */
AOO_ENUM(AooStreamState)
{
    /** stream is (temporarily) inactive */
    kAooStreamStateInactive = 0,
    /** stream is active */
    kAooStreamStateActive = 1,
    /** stream is buffering */
    kAooStreamStateBuffering = 2
};

/** \brief (AooSink) the stream state has changed */
typedef struct AooEventStreamState
{
    AOO_EVENT_HEADER
        AooEndpoint endpoint; /**< source endpoint */
    AooStreamState state; /**< new stream state */
    AooInt32 sampleOffset; /**< corresponding sample offset */
} AooEventStreamState;

/** \brief (AooSink) the source stream format has changed */
typedef struct AooEventFormatChange
{
    AOO_EVENT_HEADER
        AooEndpoint endpoint; /**< source endpoint */
    const AooFormat *format; /**< new stream format */
} AooEventFormatChange;

/** \brief (AooSource) received invitation by sink */
typedef struct AooEventInvite
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< sink endpoint */
    AooId token; /**< stream token (for AooSource::handleInvite) */
    const AooData *metadata; /**< optional metadata */
} AooEventInvite;

/** \brief (AooSource) received uninvitation by sink */
typedef struct AooEventUninvite
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< sink endpoint */
    AooId token; /**< stream token (for AooSource::handleUninvite) */
} AooEventUninvite;

/** \brief (AooSink) invitation has been declined */
typedef AooEventEndpoint AooEventInviteDecline;

/** \brief (AooSink) invitation has timed out */
typedef AooEventEndpoint AooEventInviteTimeout;

/** \brief (AooSink) uninvitation has timed out */
typedef AooEventEndpoint AooEventUninviteTimeout;

/** \brief (AooSink) stream time event
 *
 * \details Both the source and local time are measured
 * in sample time, using the respective stream start time
 * as the reference point.
 *
 * The stream time can be used to synchronize multiple
 * streams from the same machine. If you know the actual
 * clock time offset, you can even synchronize streams
 * from different machines.
 * \see AooEventSourcePing and AooEventSinkPing.
 */
typedef struct AooEventStreamTime
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< source endpoint */
    AooNtpTime sourceTime; /**< source time stamp (remote) */
    AooNtpTime sinkTime; /**< sink time stamp (local) */
    AooInt32 sampleOffset; /**< corresponding sample offset */
} AooEventStreamTime;

/** \brief (AooSink) generic stream diagnostic event */
typedef struct AooEventBlock
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< source endpoint */
    AooInt32 count; /**< event count */
} AooEventBlock;

/** \brief (AooSink) blocks had to be skipped/dropped */
typedef AooEventBlock AooEventBlockDrop;

/** \brief (AooSink) blocks have been resent */
typedef AooEventBlock AooEventBlockResend;

/** \brief (AooSink) empty blocks caused by source xrun */
typedef AooEventBlock AooEventBlockXRun;

/** \brief (AooSource) frames have been resent */
typedef struct AooEventFrameResend
{
    AOO_EVENT_HEADER
    AooEndpoint endpoint; /**< sink end point */
    AooInt32 count;  /**< number of resent frames */
} AooEventFrameResend;

/*-------------------------------------------------*/
/*            AOO server/client events             */
/*-------------------------------------------------*/

/* client events */

/** \brief client has been disconnected from server */
typedef AooEventError AooEventDisconnect;

/** \brief client received server notification */
typedef struct AooEventNotification
{
    AOO_EVENT_HEADER
    AooData message; /**< the message */
} AooEventNotification;

/** \brief we have been ejected from a group */
typedef struct AooEventGroupEject
{
    AOO_EVENT_HEADER
    AooId groupId; /**< the group ID */
} AooEventGroupEject;

/** \brief group metadata has been updated,
  * either by another user or on the server. */
typedef struct AooEventGroupUpdate
{
    AOO_EVENT_HEADER
    AooId groupId; /**< the group ID */
    AooId userId; /**< the user who updated the group;
                  #kAooIdNone if updated on the server */
    AooData groupMetadata; /**< the new group metadata */
} AooEventGroupUpdate;

/** \brief user metadata has been updated by the server */
typedef struct AooEventUserUpdate
{
    AOO_EVENT_HEADER
    AooId groupId; /**< the group ID */
    AooId userId; /**< the user ID */
    AooData userMetadata; /**< the new metadata */
} AooEventUserUpdate;

/* peer events */

/** \brief generic peer event */
typedef struct AooEventPeer
{
    AOO_EVENT_HEADER
    AooId groupId; /**< the group ID */
    AooId userId; /**< the user ID */
    const AooChar *groupName; /**< the group name */
    const AooChar *userName; /**< the user name */
    AooSockAddr address; /**< the socket address used for
                         peer-to-peer communication */
    AooPeerFlags flags; /**< flags */
    const AooChar *version; /**< the peer's AOO version string */
    const AooData *metadata; /**< (optional) peer metadata,
                             see AooResponseGroupJoin::userMetadata */
#if 0
    /** relay address providedby this peer, see AooClient::joinGroup() */
    const AooIpEndpoint *relayAddress;
#endif
} AooEventPeer;

/** \brief peer handshake has started */
typedef AooEventPeer AooEventPeerHandshake;

/** \brief peer handshake has timed out */
typedef AooEventPeer AooEventPeerTimeout;

/** \brief peer has joined a group */
typedef AooEventPeer AooEventPeerJoin;

/** \brief peer has left a group */
#if 1
typedef AooEventPeer AooEventPeerLeave;
#else
typedef struct AooEventPeerLeave
{
    AOO_EVENT_HEADER
    AooId group;
    AooId user;
} AooEventPeerLeave;
#endif

/** \brief received ping (reply) from peer */
typedef struct AooEventPeerPing
{
    AOO_EVENT_HEADER
    AooId group; /**< group ID */
    AooId user; /**< user ID */
    AooNtpTime t1; /**< local send time */
    AooNtpTime t2; /**< remote receive time */
    AooNtpTime t3; /**< remote send time */
    AooNtpTime t4; /**< local receive time */
} AooEventPeerPing;

/** \brief peer state */
typedef struct AooEventPeerState
{
    AOO_EVENT_HEADER
    AooId group; /**< group ID */
    AooId user; /**< user ID */
    AooBool active; /**< peer is (in)active */
} AooEventPeerState;

/** \brief received peer message */
typedef struct AooEventPeerMessage
{
    AOO_EVENT_HEADER
    AooId groupId; /**< group ID */
    AooId userId; /**< user ID */
    AooNtpTime timeStamp; /**< send time */
    AooData data; /**< the message data */
} AooEventPeerMessage;

/** \brief peer metadata has been updated,
 * either by another user or on the server. */
typedef struct AooEventPeerUpdate
{
    AOO_EVENT_HEADER
    AooId groupId; /**< group ID */
    AooId userId; /**< user ID */
    AooData userMetadata; /**< the new user metadata */
} AooEventPeerUpdate;

/* server events */

/** \brief client error */
typedef struct AooEventClientError
{
    AOO_EVENT_HEADER
    AooId id; /**< client Id */
    AooError error; /**< error code */
    const AooChar *message; /**< error message */
} AooEventClientError;

/** \brief client tried to log in */
typedef struct AooEventClientLogin
{
    AOO_EVENT_HEADER
    AooId id; /**< client ID */
    AooError error; /**< error code;
                    #kAooOk if logged in successfully */
    /* TODO: error message? */
    /* TODO: socket address */
    const AooChar *version; /**< the client's AOO version */
    const AooData *metadata; /**< optional metadata */
} AooEventClientLogin;

/** \brief client logged out */
typedef struct AooEventClientLogout
{
    AOO_EVENT_HEADER
    AooId id; /**< client ID */
    /* TODO: socket address? */
    AooError errorCode; /**< error code;
                        #kAooErrorNone if logged out normally */
    const AooChar *errorMessage; /**< error message */
} AooEventClientLogout;

/** \brief group added */
typedef struct AooEventGroupAdd
{
    AOO_EVENT_HEADER
    AooId id; /**< group ID */
    AooGroupFlags flags; /**< flags */
    const AooChar *name; /**< group name */
    const AooData *metadata; /**< optional group metadata */
#if 0
    const AooIpEndpoint *relayAddress; /**< optional relay address */
#endif
} AooEventGroupAdd;

/** \brief group removed */
typedef struct AooEventGroupRemove
{
    AOO_EVENT_HEADER
    AooId id; /**< group ID */
#if 1
    const AooChar *name; /**< group name */
#endif
} AooEventGroupRemove;

/** \brief user joined group */
typedef struct AooEventGroupJoin
{
    AOO_EVENT_HEADER
    AooId groupId; /**< group ID */
    AooId userId; /**< user ID */
#if 1
    const AooChar *groupName; /**< group name */
#endif
    const AooChar *userName; /**< user name */
    AooId clientId; /**< client ID */
    AooUserFlags userFlags; /**< user flags */
    const AooData *userMetadata; /**< optional user metadata */
#if 0
    const AooIpEndpoint *relayAddress; /**< optional relay address */
#endif
} AooEventGroupJoin;

/** \brief user left group */
typedef struct AooEventGroupLeave
{
    AOO_EVENT_HEADER
    AooId groupId; /**< group ID */
    AooId userId; /**< user ID */
#if 1
    const AooChar *groupName; /**< group name */
    const AooChar *userName; /**< user name */
#endif
} AooEventGroupLeave;

/*----------------------------------------------------*/

/** \brief union holding all possible events */
union AooEvent
{
    AooEventType type; /**< \brief the event type */
    AooEventBase base; /**< \brief base */
    AooEventError error; /**< \brief error */
    /* AOO source/sink events */
    AooEventEndpoint endpoint; /**< \brief endpoint */
    AooEventSourcePing sourcePing; /**< \brief source ping */
    AooEventSinkPing sinkPing; /**< \brief sink ping */
    AooEventInvite invite; /**< \brief invite */
    AooEventUninvite uninvite; /**< \brief uninvite */
    AooEventSinkAdd sinkAdd; /**< \brief sink added */
    AooEventSinkRemove sinkRemove; /**< \brief sink removed */
    AooEventSourceAdd sourceAdd; /**< \brief source added */
    AooEventSourceRemove sourceRemove;/**< \brief source removed */
    AooEventStreamStart streamStart; /**< \brief stream started */
    AooEventStreamStop streamStop; /**< \brief stream stopped */
    AooEventStreamState streamState; /**< \brief stream state changed */
    AooEventStreamTime streamTime; /**< \brief stream time */
    AooEventFormatChange formatChange; /**< \brief format changed */
    AooEventInviteDecline inviteDecline; /**< \brief invitation declined */
    AooEventInviteTimeout inviteTimeout; /**< \brief invitation timed out */
    AooEventUninviteTimeout uninviteTimeout; /**< \brief uninvitation timed out */
    AooEventBufferOverrun bufferOverrrun; /**< \brief jitter buffer overrun */
    AooEventBufferUnderrun bufferUnderrun; /**< \brief jitter buffer underrun */
    AooEventBlockDrop blockDrop; /**< \brief block dropped */
    AooEventBlockResend blockResend; /**< \brief bock resent */
    AooEventBlockXRun blockXRun; /**< \brief empty block for source xrun */
    AooEventFrameResend frameResend; /**< \brief frames resent */
    /* AooClient/AooServer events */
    AooEventDisconnect disconnect; /**< \brief disconnected from server */
    AooEventNotification notification; /**< \brief server notification */
    AooEventGroupEject groupEject; /**< \brief ejected from group */
    AooEventPeer peer; /**< \brief peer event */
    AooEventPeerPing peerPing; /**< \brief peer ping */
    AooEventPeerState peerState; /**< \brief peer state changed */
    AooEventPeerHandshake peerHandshake; /**< \brief peer handshake started */
    AooEventPeerTimeout peerTimeout; /**< \brief peer handshake timed out */
    AooEventPeerJoin peerJoin; /**< \brief peer joined group */
    AooEventPeerLeave peerLeave; /**< \brief peer left group */
    AooEventPeerMessage peerMessage; /**< \brief peer message */
    AooEventPeerUpdate peerUpdate; /**< \brief peer metadata changed */
    AooEventGroupUpdate groupUpdate; /**< \brief group metadata changed */
    AooEventUserUpdate userUpdate; /**< \brief own user metadata changed */
    AooEventClientError clientError; /**< \brief generic client error */
    AooEventClientLogin clientLogin; /**< \brief client tried to log in */
    AooEventClientLogout clientLogout; /**< \brief client logged out */
    AooEventGroupAdd groupAdd; /**< \brief group added (automatically) */
    AooEventGroupRemove groupRemove; /**< \brief group removed (automatically) */
    AooEventGroupJoin groupJoin; /**< \brief user joined group */
    AooEventGroupLeave groupLeave;/**< \brief user left group */
};

/*-------------------------------------------------*/

AOO_PACK_END
