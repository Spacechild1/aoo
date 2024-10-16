/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief macros and enumerations
 */

#pragma once

#include "aoo_config.h"

/** \cond DO_NOT_DOCUMENT */

/* check for C++11
 * NB: MSVC does not correctly set __cplusplus by default! */
#if defined(__cplusplus) && (__cplusplus >= 201103L || ((defined(_MSC_VER) && _MSC_VER >= 1900)))
    #define AOO_HAVE_CXX11 1
#else
    #define AOO_HAVE_CXX11 0
#endif

#if defined(__cplusplus)
# define AOO_INLINE inline
#else
# if (__STDC_VERSION__ >= 199901L)
#  define AOO_INLINE static inline
# else
#  define AOO_INLINE static
# endif
#endif

#ifndef AOO_CALL
# ifdef _WIN32
#  define AOO_CALL __cdecl
# else
#  define AOO_CALL
# endif
#endif

#ifndef AOO_EXPORT
# ifdef AOO_SHARED /* shared library */
#  if defined(_WIN32) /* Windows */
#   if defined(AOO_BUILD)
#      define AOO_EXPORT __declspec(dllexport)
#   else
#      define AOO_EXPORT __declspec(dllimport)
#   endif
#  elif defined(__GNUC__) && defined(AOO_BUILD) /* GNU C */
#   define AOO_EXPORT __attribute__ ((visibility ("default")))
#  else /* Other */
#   define AOO_EXPORT
#  endif
# else /* static library */
#  define AOO_EXPORT
# endif
#endif

#ifdef __cplusplus
# define AOO_API extern "C" AOO_EXPORT
#else
# define AOO_API AOO_EXPORT
#endif

/** \endcond */

/*-------------------- struct packing ----------------------*/

/** \cond DO_NOT_DOCUMENT */

#if defined(__GNUC__)
# define AOO_PACK_BEGIN _Pragma("pack(push,8)")
# define AOO_PACK_END _Pragma("pack(pop)")
# elif defined(_MSC_VER)
# define AOO_PACK_BEGIN __pragma(pack(push,8))
# define AOO_PACK_END __pragma(pack(pop))
#else
# define AOO_PACK_BEGIN
# define AOO_PACK_END
#endif

/** \endcond */

/*----------------------- utilities ------------------------*/

/** \brief calculate the size of a versioned struct */
#define AOO_STRUCT_SIZE(type, field) \
    (offsetof(type, field) + sizeof(((type *)NULL)->field))

/** \brief initialize a versioned struct */
#define AOO_STRUCT_INIT(ptr, type, field) \
    (ptr)->structSize = AOO_STRUCT_SIZE(type, field)

/** \brief check if a versioned struct has a specific field */
#define AOO_CHECK_FIELD(ptr, type, field) \
    (((ptr)->structSize) >= AOO_STRUCT_SIZE(type, field))

/*---------------------- versioning ------------------------*/

/** \brief the AOO major version */
#define kAooVersionMajor 2
/** \brief the AOO minor version */
#define kAooVersionMinor 0
/** \brief the AOO bugfix version */
#define kAooVersionPatch 0
/** \brief the AOO test version (0: stable release) */
#define kAooVersionTest 4

/*------------------ OSC address patterns ------------------*/

/** \cond DO_NOT_DOCUMENT */

#define kAooMsgDomain "/aoo"
#define kAooMsgDomainLen 4

#define kAooMsgSource "/src"
#define kAooMsgSourceLen 4

#define kAooMsgSink "/sink"
#define kAooMsgSinkLen 5

#define kAooMsgStart "/start"
#define kAooMsgStartLen 6

#define kAooMsgStop "/stop"
#define kAooMsgStopLen 5

#define kAooMsgData "/data"
#define kAooMsgDataLen 5

#define kAooMsgPing "/ping"
#define kAooMsgPingLen 5

#define kAooMsgPong "/pong"
#define kAooMsgPongLen 5

#define kAooMsgInvite "/invite"
#define kAooMsgInviteLen 7

#define kAooMsgUninvite "/uninvite"
#define kAooMsgUninviteLen 9

#define kAooMsgDecline "/decline"
#define kAooMsgDeclineLen 8

#define kAooMsgServer "/server"
#define kAooMsgServerLen 7

#define kAooMsgClient "/client"
#define kAooMsgClientLen 7

#define kAooMsgPeer "/peer"
#define kAooMsgPeerLen 5

#define kAooMsgRelay "/relay"
#define kAooMsgRelayLen 6

#define kAooMsgMessage "/msg"
#define kAooMsgMessageLen 4

#define kAooMsgAck "/ack"
#define kAooMsgAckLen 4

#define kAooMsgLogin "/login"
#define kAooMsgLoginLen 6

#define kAooMsgQuery "/query"
#define kAooMsgQueryLen 6

#define kAooMsgGroup "/group"
#define kAooMsgGroupLen 6

#define kAooMsgUser "/user"
#define kAooMsgUserLen 5

#define kAooMsgJoin "/join"
#define kAooMsgJoinLen 5

#define kAooMsgLeave "/leave"
#define kAooMsgLeaveLen 6

#define kAooMsgEject "/eject"
#define kAooMsgEjectLen 6

#define kAooMsgUpdate "/update"
#define kAooMsgUpdateLen 7

#define kAooMsgChanged "/changed"
#define kAooMsgChangedLen 8

#define kAooMsgRequest "/request"
#define kAooMsgRequestLen 8

/** \endcond */

/*------------------- binary messages ---------------------*/

/** \cond DO_NOT_DOCUMENT */

/* domain bit + type (uint8), size bit + cmd (uint8)
 * a) sink ID (uint8), source ID (uint8)
 * b) padding (uint16), sink ID (int32), source ID (int32)
 */

#define kAooBinMsgHeaderSize 4
#define kAooBinMsgLargeHeaderSize 12
#define kAooBinMsgDomainBit 0x80
#define kAooBinMsgSizeBit 0x80

/** \brief commands for 'data' binary message */
enum
{
    kAooBinMsgCmdData = 0
};

/** \brief flags for 'data' binary message */
enum
{
    kAooBinMsgDataSampleRate = 0x01,
    kAooBinMsgDataFrames = 0x02,
    kAooBinMsgDataStreamMessage = 0x04,
    kAooBinMsgDataXRun = 0x08,
    kAooBinMsgDataTimeStamp = 0x10
};

/** \brief commands for 'message' binary message */
enum
{
    kAooBinMsgCmdMessage = 0,
    kAooBinMsgCmdAck = 1
};

/** \brief flags for 'message' binary message */
enum
{
    kAooBinMsgMessageReliable = 0x01,
    kAooBinMsgMessageFrames = 0x02,
    kAooBinMsgMessageTimestamp = 0x04
};

/** \brief commands for 'relay' binary message */
enum
{
    kAooBinMsgCmdRelayIPv4 = 0,
    kAooBinMsgCmdRelayIPv6 = 1
};

/** \endcond */

/*---------------------------------------------------------*/
