/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief C interface for AOO client
 */

#pragma once

#include "aoo_config.h"
#include "aoo_controls.h"
#include "aoo_defines.h"
#include "aoo_events.h"
#include "aoo_requests.h"
#include "aoo_types.h"

struct AooSource;
struct AooSink;

typedef struct AooClient AooClient;

/** \brief create a new AOO source instance
 *
 * \param[out] err (optional) error code on failure
 * \return new AooClient instance on success; `NULL` on failure
 */
AOO_API AooClient * AOO_CALL AooClient_new(AooError *err);

/** \brief destroy AOO client */
AOO_API void AOO_CALL AooClient_free(AooClient *client);

/** \copydoc AooClient::setup() */
AOO_API AooError AOO_CALL AooClient_setup(
        AooClient *client, AooClientSettings *settings);

/** \copydoc AooClient::run() */
AOO_API AooError AOO_CALL AooClient_run(
        AooClient *client, AooBool nonBlocking);

/** \copydoc AooClient::quit() */
AOO_API AooError AOO_CALL AooClient_quit(AooClient *client);

/** \copydoc AooClient::send() */
AOO_API AooError AOO_CALL AooClient_send(
        AooClient *client, AooBool nonBlocking);

/** \copydoc AooClient::receive() */
AOO_API AooError AOO_CALL AooClient_receive(
    AooClient *client, AooBool nonBlocking);

/** \copydoc AooClient::notify() */
AOO_API AooError AOO_CALL AooClient_notify(AooClient *client);

/** \copydoc AooClient::handlePacket() */
AOO_API AooError AOO_CALL AooClient_handlePacket(
        AooClient *client, const AooByte *data, AooInt32 size,
        const void *address, AooAddrSize addrlen);

/** \copydoc AooClient::setEventHandler() */
AOO_API AooError AOO_CALL AooClient_setEventHandler(
        AooClient *client, AooEventHandler fn, void *user, AooEventMode mode);

/** \copydoc AooClient::eventsAvailable() */
AOO_API AooBool AOO_CALL AooClient_eventsAvailable(AooClient *client);

/** \copydoc AooClient::pollEvents() */
AOO_API AooError AOO_CALL AooClient_pollEvents(AooClient *client);

/** \copydoc AooClient::addSource() */
AOO_API AooError AOO_CALL AooClient_addSource(
        AooClient *client, AooSource *source, AooId id);

/** \copydoc AooClient::removeSource() */
AOO_API AooError AOO_CALL AooClient_removeSource(
        AooClient *client, AooSource *source);

/** \copydoc AooClient::addSink() */
AOO_API AooError AOO_CALL AooClient_addSink(
        AooClient *client, AooSink *sink, AooId id);

/** \copydoc AooClient::removeSink() */
AOO_API AooError AOO_CALL AooClient_removeSink(
        AooClient *client, AooSink *sink);

/** \copydoc AooClient::connect() */
AOO_API AooError AooClient_connect(
        AooClient *client,
        const AooChar *hostName, AooInt32 port,
        const AooChar *password, const AooData *metadata,
        AooResponseHandler cb, void *context);

/** \copydoc AooClient::disconnect() */
AOO_API AooError AooClient_disconnect(
        AooClient *client, AooResponseHandler cb, void *context);

/** \copydoc AooClient::joinGroup() */
AOO_API AooError AooClient_joinGroup(
        AooClient *client,
        const AooChar *groupName, const AooChar *groupPwd,
        const AooData *groupMetadata,
        const AooChar *userName, const AooChar *userPwd,
        const AooData *userMetadata,
        const AooIpEndpoint *relayAddress,
        AooResponseHandler cb, void *context);

/** \copydoc AooClient::leaveGroup() */
AOO_API AooError AooClient_leaveGroup(
        AooClient *client, AooId group,
        AooResponseHandler cb, void *context);

/** \copydoc AooClient::updateGroup() */
AOO_API AooError AOO_CALL AooClient_updateGroup(
        AooClient *client,
        AooId groupId, const AooData *groupMetadata,
        AooResponseHandler cb, void *context);

/** \copydoc AooClient::updateUser() */
AOO_API AooError AOO_CALL AooClient_updateUser(
        AooClient *client, AooId groupId,
        const AooData *userMetadata,
        AooResponseHandler cb, void *context);

/** \copydoc AooClient::customRequest() */
AOO_API AooError AOO_CALL AooClient_customRequest(
        AooClient *client, const AooData *data, AooFlag flags,
        AooResponseHandler cb, void *context);

/** \copydoc AooClient::findGroupByName() */
AOO_API AooError AOO_CALL AooClient_findGroupByName(
        AooClient *client, const AooChar *groupName, AooId *groupId);

/** \copydoc AooClient::getGroupName() */
AOO_API AooError AOO_CALL AooClient_getGroupName(
    AooClient *client, AooId group, AooChar *buffer, AooSize *size);

/** \copydoc AooClient::findPeerByName() */
AOO_API AooError AOO_CALL AooClient_findPeerByName(
        AooClient *client, const AooChar *groupName, const AooChar *userName,
        AooId *groupId, AooId *userId, void *address, AooAddrSize *addrlen);

/** \copydoc AooClient::findPeerByAddress() */
AOO_API AooError AOO_CALL AooClient_findPeerByAddress(
        AooClient *client, const void *address, AooAddrSize addrlen,
        AooId *groupId, AooId *userId);

/** \copydoc AooClient::getPeerName() */
AOO_API AooError AOO_CALL AooClient_getPeerName(
        AooClient *client, AooId group, AooId user,
        AooChar *groupNameBuffer, AooSize *groupNameSize,
        AooChar *userNameBuffer, AooSize *userNameSize);

/** \copydoc AooClient::sendMessage() */
AOO_API AooError AOO_CALL AooClient_sendMessage(
        AooClient *client, AooId group, AooId user,
        const AooData *msg, AooNtpTime timeStamp, AooMessageFlags flags);

/** \copydoc AooClient::sendRequest() */
AOO_API AooError AOO_CALL AooClient_sendRequest(
        AooClient *client, const AooRequest *request,
        AooResponseHandler callback, void *user, AooFlag flags);

/** \copydoc AooClient::control */
AOO_API AooError AOO_CALL AooClient_control(
        AooClient *client, AooCtl ctl, AooIntPtr index, void *data, AooSize size);

/*--------------------------------------------*/
/*         type-safe control functions        */
/*--------------------------------------------*/

/** \copydoc AooClient::setBinaryMsg() */
AOO_INLINE AooError AooClient_setBinaryMsg(AooClient *client, AooBool b)
{
    return AooClient_control(client, kAooCtlSetBinaryClientMsg, 0, AOO_ARG(b));
}

/** \copydoc AooClient::getBinaryMsg() */
AOO_INLINE AooError AooClient_getBinaryMsg(AooClient *client, AooBool *b)
{
    return AooClient_control(client, kAooCtlGetBinaryClientMsg, 0, AOO_ARG(*b));
}

/** \copydoc AooClient::setPeerPingInterval() */
AOO_INLINE AooError AooClient_setPeerPingInterval(
    AooClient *client, AooSeconds interval)
{
    return AooClient_control(client, kAooCtlSetPingInterval, 0, AOO_ARG(interval));
}

/** \copydoc AooClient::getPeerPingInterval() */
AOO_INLINE AooError AooClient_getPeerPingInterval(
    AooClient *client, AooSeconds *interval)
{
    return AooClient_control(client, kAooCtlGetPingInterval, 0, AOO_ARG(*interval));
}

/** \copydoc AooClient::setServerPingSettings() */
AOO_INLINE AooError AooClient_setServerPingSettings(
    AooClient *client, const AooPingSettings *settings)
{
    return AooClient_control(client, kAooCtlSetPingSettings, 1, AOO_ARG(*settings));
}

/** \copydoc AooClient::getServerPingSettings() */
AOO_INLINE AooError AooClient_getServerPingSettings(
    AooClient *client, AooPingSettings *settings)
{
    return AooClient_control(client, kAooCtlGetPingSettings, 1, AOO_ARG(*settings));
}

/** \copydoc AooClient::addInterfaceAddress() */
AOO_INLINE AooError AooClient_addInterfaceAddress(
    AooClient *client, const AooChar *address)
{
    return AooClient_control(client, kAooCtlAddInterfaceAddress, (AooIntPtr)address, NULL, 0);
}

/** \copydoc AooClient::removeInterfaceAddress() */
AOO_INLINE AooError removeInterfaceAddress(
    AooClient *client, const AooChar *address)
{
    return AooClient_control(client, kAooCtlRemoveInterfaceAddress, (AooIntPtr)address, NULL, 0);
}

/** \copydoc AooClient::clearInterfaceAddresses() */
AOO_INLINE AooError clearInterfaceAddresses(AooClient *client)
{
    return AooClient_control(client, kAooCtlRemoveInterfaceAddress, 0, NULL, 0);
}

/*--------------------------------------------*/
/*         type-safe request functions        */
/*--------------------------------------------*/

/* (empty) */
