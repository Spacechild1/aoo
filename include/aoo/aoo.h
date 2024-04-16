/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief main library API
 */

#pragma once

#include "aoo_config.h"
#include "aoo_defines.h"
#include "aoo_types.h"

AOO_PACK_BEGIN

/*------------------------------------------------------*/

/** \brief settings for aoo_initialize()
 *
 * \details Example:
 *
 *      AooSettings settings = AOO_SETTINGS_INIT();
 *      settings.logFunc = myLogFunc; // set custom log function
 *      aoo_initialize(&settings);
 */
typedef struct AooSettings
{
    AooSize structSize;
    /** custom allocator function, or `NULL` */
    AooAllocFunc allocFunc;
    /** custom log function, or `NULL` */
    AooLogFunc logFunc;
    /** size of RT memory pool */
    AooSize memPoolSize;
} AooSettings;

/** \brief default initializer for AooSettings struct */
#define AOO_SETTINGS_INIT() {                   \
    AOO_STRUCT_SIZE(AooSettings, memPoolSize),  \
    NULL, NULL, AOO_MEM_POOL_SIZE               \
}

/**
 * \brief initialize AOO library settings
 *
 * \note Call before any other AOO function!
 * \param settings (optional) settings struct
 */
AOO_API AooError AOO_CALL aoo_initialize(const AooSettings *settings);

/**
 * \brief terminate AOO library
 *
 * \note Call before program exit.
 */
AOO_API void AOO_CALL aoo_terminate(void);

/**
 * \brief get the AOO version number
 *
 * \param [out] major major version
 * \param [out] minor minor version
 * \param [out] patch bugfix version
 * \param [out] test test or pre-release version
 */
AOO_API void AOO_CALL aoo_getVersion(
        AooInt32 *major, AooInt32 *minor, AooInt32 *patch, AooInt32 *test);

/**
 * \brief get the AOO version string
 *
 * Format: `<major>[.<minor>][.<patch>][-test<test>]`
 * \return the version as a C string
 */
AOO_API const AooChar * AOO_CALL aoo_getVersionString(void);

/**
 * \brief get a textual description for an error code
 *
 * \param err the error code
 * \return a C string describing the error; if the error code is unknown,
 *         an empty string is returned.
 */
AOO_API const AooChar * AOO_CALL aoo_strerror(AooError err);

/**
 * \brief get the current NTP time
 *
 * \return NTP time stamp
 */
AOO_API AooNtpTime AOO_CALL aoo_getCurrentNtpTime(void);

/**
 * \brief convert NTP time to seconds
 *
 * \param t NTP time stamp
 * \return seconds
 */
AOO_API AooSeconds AOO_CALL aoo_ntpTimeToSeconds(AooNtpTime t);

/**
 * \brief convert seconds to NTP time
 *
 * \param s seconds
 * \return NTP time stamp
 */
AOO_API AooNtpTime AOO_CALL aoo_ntpTimeFromSeconds(AooSeconds s);

/**
 * \brief get time difference in seconds between two NTP time stamps
 *
 * \param t1 the first time stamp
 * \param t2 the second time stamp
 * \return the time difference in seconds
 */
AOO_API AooSeconds AOO_CALL aoo_ntpTimeDuration(
        AooNtpTime t1, AooNtpTime t2);

/**
 * \brief parse an AOO message
 *
 * Tries to obtain the AOO message type and ID from the address pattern,
 * like in `/aoo/src/<id>/data`.
 *
 * \param msg the OSC message data
 * \param size the OSC message size
 * \param [out] type the AOO message type
 * \param [out] id the source/sink ID
 * \param [out] offset pointer to the start of the remaining address pattern
 * \return error code
 */
AOO_API AooError AOO_CALL aoo_parsePattern(
        const AooByte *msg, AooInt32 size,
        AooMsgType *type, AooId *id, AooInt32 *offset);

#if AOO_NET
/**
 * \brief handle relay message
 *
 * This function can be used to implement a basic AOO relay server.
 *
 * \note You don't need to parse the incoming message with `aoo_parsePattern`;
 *       if the message is not a valid AOO relay message, the function will
 *       simply ignore it and return #kAooErrorBadFormat.
 *
 * \param data the message data
 * \param size the message size
 * \param address the source socket address
 * \param addrlen the source socket address length
 * \param sendFunc the send function
 * \param userData user data passed to the send function
 * \param socketType the socket type; one of the following values:
 *        #kAooSocketIPv4, #kAooSocketIPv6 or #kAooSocketDualStack
 * \return error code
 *
 * \note In case of a "real" dual stack setup (i.e. dedicated
 *       IPv6 socket + IPv4 socket), you must pass the flags
 *       `kAooSocketIPv4 | kAooSocketIPv6` to `socketType`.
 *       The send function will be called with either an
 *       IPv4 or IPv6 address and you must forward it to
 *       the corresponding socket.
 */
AOO_API AooError aoo_handleRelayMessage(
        const AooByte *data, AooInt32 size,
        const void *address, AooAddrSize addrlen,
        AooSendFunc sendFunc, void *userData,
        AooSocketFlags socketType);

#endif /* AOO_NET */

/**
 * \brief get AooData type from string representation
 *
 * \param str the string
 * \return the data type on success, kAooDataUnspecified on failure
 */
AOO_API AooDataType AOO_CALL aoo_dataTypeFromString(const AooChar *str);

/**
 * \brief convert AooData type to string representation
 *
 * \param type the data type
 * \return a C string on success; if the data type is unknown or invalid,
 *         an empty string is returned.
 */
AOO_API const AooChar * AOO_CALL aoo_dataTypeToString(AooDataType type);

/** \brief socket address storage
 *
 * This struct is large enough (and properly aligned) to hold both
 * IPv4 and IPv6 socket addresses. For example, this can be used
 * to safely copy `AooSockAddr` members in AOO events.
 * (The socket address data can be simply copied with `memcpy`.)
 *
 * \note With 32 bytes it is significantly smaller than
 * `sockaddr_storage` (which is typically 128 bytes large).
 */
typedef struct AooSockAddrStorage
{
#if AOO_HAVE_SA_LEN
    /* e.g. macOS, BSD and ESP32 */
    AooByte len;    /**< address length */
    AooByte family; /**< address family */
#else
    AooUInt16 family; /**< address family */
#endif
    /** \cond DO_NOT_DOCUMENT */
    AooByte pad1[6];
    AooUInt64 align;
    AooByte pad2[16];
    /** \endcond */
} AooSockAddrStorage;

/**
 * \brief compare two socket addresses for equality
 *
 * \param address 1
 * \param size of address 1
 * \param address 2
 * \param size of address 2
 * \return #kAooTrue if both addresses are equal
 *
 * \note Both addresses must be valid!
 * Otherwise the result is undefined.
 *
 * \attention You must never attempt to compare socket addresses
 * with `memcmp` because the padding bytes may contain garbage!
 */
AOO_API AooBool AOO_CALL aoo_sockAddrEqual(
        const void *sockaddr1, AooAddrSize addrlen1,
        const void *sockaddr2, AooAddrSize addrlen2);

/**
 * \brief compute the hash for a socket address
 *
 * For example, this allows to use socket addresses
 * as keys in a hashtable.
 *
 * \param address
 * \param address size
 * \return the hash value
 *
 * \note the socket address must be valid!
 * Otherwise the result is undefined.
 */
AOO_API AooSize AOO_CALL aoo_sockAddrHash(
        const void *sockaddr, AooAddrSize addrlen);

/**
 * \brief make sockaddr from IP endpoint
 *
 * Tries to convert the IP address string to one of the specified types
 * if possible; returns an error otherwise.
 *
 * \param ipAddress IP address string
 * \param port port number
 * \param type combination of supported IP families
 * \param [out] sockaddr socket address buffer,
 *        e.g. `sockaddr_storage` or `AooSockAddrStorage`
 * \param [in,out] addrlen buffer size; updated to actual size
 */
AOO_API AooError aoo_ipEndpointToSockAddr(const AooChar *ipAddress, AooUInt16 port,
        AooSocketFlags type, void *sockaddr, AooAddrSize *addrlen);

/**
 * \brief get IP endpoint from sockaddr
 *
 * \param sockaddr sockaddr struct
 * \param addrlen sockaddr size
 * \param [out] ipAddressBuffer buffer for IP address string
 * \param [in,out] ipAddressSize IP address buffer size; updated to actual size (excluding the 0 character)
 * \param [out] port port Number
 * \param [out] type (optional) IP type
 */
AOO_API AooError aoo_sockAddrToIpEndpoint(const void *sockaddr, AooSize addrlen,
        AooChar *ipAddressBuffer, AooSize *ipAddressSize, AooUInt16 *port, AooSocketFlags *type);

/**
 * \brief get last socket error
 *
 * Typically used to obtain more detailed information about kAooErrorSocket.
 *
 * \param [out] errorCode the error code
 * \param [out] errorMessageBuffer (optional) error message buffer
 * \param [in,out] errorMessageSize (optional) error message buffer size;
 *                updated to actual size (excluding the 0 character)
 */
AOO_API AooError aoo_getLastSocketError(AooInt32 *errorCode,
        AooChar *errorMessageBuffer, AooSize *errorMessageSize);

/**
 * \brief get last system/OS error
 *
 * Typically used to obtain more detailed information about kAooErrorSystem.
 *
 * \param [out] errorCode the error code
 * \param [out] errorMessageBuffer (optional) error message buffer
 * \param [in,out] errorMessageSize (optional) error message buffer size;
 *                updated to actual size (excluding the 0 character)
 */
AOO_API AooError aoo_getLastSystemError(AooInt32 *errorCode,
        AooChar *errorMessageBuffer, AooSize *errorMessageSize);

/*------------------------------------------------------*/

AOO_PACK_END
