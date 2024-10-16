/* Copyright (c) 2021 Christof Ressi
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/** \file
 * \brief C interface for AOO server
 */

#pragma once

#include "aoo_config.h"
#include "aoo_controls.h"
#include "aoo_defines.h"
#include "aoo_events.h"
#include "aoo_requests.h"
#include "aoo_types.h"

/*--------------------------------------------------------------*/

/** \cond DO_NOT_DOCUMENT */
typedef struct AooServer AooServer;
/** \endcond */

/** \brief create a new AOO server instance
 *
 * \return new AooServer instance on success; `NULL` on failure
 */
AOO_API AooServer * AOO_CALL AooServer_new(void);

/** \brief destroy AOO server instance */
AOO_API void AOO_CALL AooServer_free(AooServer *server);

/** \copydoc AooServer::setup() */
AOO_API AooError AOO_CALL AooServer_setup(
        AooServer *server, AooServerSettings *settings);

/** \copydoc AooServer::run() */
AOO_API AooError AOO_CALL AooServer_run(
        AooServer *server, AooSeconds timeout);

/** \copydoc AooServer::receive() */
AOO_API AooError AOO_CALL AooServer_receive(
        AooServer *server, AooSeconds timeout);

/** \copydoc AooServer::handlePacket() */
AOO_API AooError AOO_CALL AooServer_handlePacket(
    AooServer *server, const AooByte *data, AooInt32 size,
    const void *address, AooAddrSize addrlen);

/** \copydoc AooServer::stop() */
AOO_API AooError AOO_CALL AooServer_stop(AooServer *server);

/* event handling */

/** \copydoc AooServer::setEventHandler() */
AOO_API AooError AOO_CALL AooServer_setEventHandler(
        AooServer *server, AooEventHandler fn, void *user, AooEventMode mode);

/** \copydoc AooServer::eventsAvailable() */
AOO_API AooBool AOO_CALL AooServer_eventsAvailable(AooServer *server);

/** \copydoc AooServer::pollEvents() */
AOO_API AooError AOO_CALL AooServer_pollEvents(AooServer *server);

/* request handling */

/** \copydoc AooServer::setRequestHandler() */
AOO_API AooError AOO_CALL AooServer_setRequestHandler(
        AooServer *server, AooRequestHandler cb,
        void *user, AooFlag flags);

/** \copydoc AooServer::handleRequest */
AOO_API AooError AOO_CALL handleRequest(
        AooServer *server,
        AooId client, AooId token, const AooRequest *request,
        AooError result, const AooResponse *response);

/* push notifications */

/** \copydoc AooServer::notifyClient() */
AOO_API AooError AOO_CALL AooServer_notifyClient(
        AooServer *server, AooId client,
        const AooData *data);

/** \copydoc AooServer::notifyGroup() */
AOO_API AooError AOO_CALL AooServer_notifyGroup(
        AooServer *server, AooId group, AooId user,
        const AooData *data);

/* group management */

/** \copydoc AooServer::findGroup() */
AOO_API AooError AOO_CALL AooServer_findGroup(
        AooServer *server, const AooChar *name, AooId *id);

/** \copydoc AooServer::addGroup() */
AOO_API AooError AOO_CALL AooServer_addGroup(
        AooServer *server, const AooChar *name, const AooChar *password,
        const AooData *metadata, const AooIpEndpoint *relayAddress,
        AooFlag flags, AooId *groupId);

/** \copydoc AooServer::removeGroup() */
AOO_API AooError AOO_CALL AooServer_removeGroup(
        AooServer *server, AooId group);

/** \copydoc AooServer::findUserInGroup() */
AOO_API AooError AOO_CALL AooServer_findUserInGroup(
        AooServer *server, AooId group,
        const AooChar *userName, AooId *userId);

/** \copydoc AooServer::addUserToGroup() */
AOO_API AooError AOO_CALL AooServer_addUserToGroup(
        AooServer *server, AooId group,
        const AooChar *userName, const AooChar *userPwd,
        const AooData *metadata, AooFlag flags, AooId *userId);

/** \copydoc AooServer::removeUserFromGroup() */
AOO_API AooError AOO_CALL AooServer_removeUserFromGroup(
        AooServer *server, AooId group, AooId user);

/** \copydoc AooServer::groupControl() */
AOO_API AooError AOO_CALL AooServer_groupControl(
        AooServer *server, AooId group, AooCtl ctl,
        AooIntPtr index, void *data, AooSize size);

/** \copydoc AooServer::control() */
AOO_API AooError AOO_CALL AooServer_control(
        AooServer *server, AooCtl ctl, AooIntPtr index,
        void *data, AooSize size);

/*--------------------------------------------*/
/*         type-safe control functions        */
/*--------------------------------------------*/

/** \copydoc AooServer::setPassword() */
AOO_INLINE AooError AooServer_setPassword(AooServer *server, const AooChar *pwd)
{
    return AooServer_control(server, kAooCtlSetPassword, (AooIntPtr)pwd, NULL, 0);
}

/** \copydoc AooServer::setRelayHost() */
AOO_INLINE AooError AooServer_setRelayHost(
    AooServer *server, const AooIpEndpoint *ep)
{
    return AooServer_control(server, kAooCtlSetRelayHost, (AooIntPtr)ep, NULL, 0);
}

/** \copydoc AooServer::setUseInternalRelay() */
AOO_INLINE AooError AooServer_setUseInternalRelay(AooServer *server, AooBool b)
{
    return AooServer_control(server, kAooCtlSetUseInternalRelay, 0, AOO_ARG(b));
}

/** \copydoc AooServer::getUseInternalRelay() */
AOO_INLINE AooError AooServer_getUseInternalRelay(AooServer *server, AooBool* b)
{
    return AooServer_control(server, kAooCtlGetUseInternalRelay, 0, AOO_ARG(*b));
}

/** \copydoc AooServer::setGroupAutoCreate() */
AOO_INLINE AooError AooServer_setGroupAutoCreate(AooServer *server, AooBool b)
{
    return AooServer_control(server, kAooCtlSetGroupAutoCreate, 0, AOO_ARG(b));
}

/** \copydoc AooServer::getGroupAutoCreate() */
AOO_INLINE AooError AooServer_getGroupAutoCreate(AooServer *server, AooBool* b)
{
    return AooServer_control(server, kAooCtlGetGroupAutoCreate, 0, AOO_ARG(*b));
}

/** \copydoc AooServer::setPingSettings() */
AOO_INLINE AooError AooServer_setPingSettings(
    AooServer *server, const AooPingSettings *settings)
{
    return AooServer_control(server, kAooCtlSetPingSettings, 0, AOO_ARG(*settings));
}

/** \copydoc AooServer::getPingSettings() */
AOO_INLINE AooError AooServer_getPingSettings(
    AooServer *server, AooPingSettings *settings)
{
    return AooServer_control(server, kAooCtlGetPingSettings, 0, AOO_ARG(*settings));
}

/*--------------------------------------------------*/
/*         type-safe group control functions        */
/*--------------------------------------------------*/

/** \copydoc AooServer::updateGroup() */
AOO_INLINE AooError AooServer_updateGroup(
    AooServer *server, AooId group, const AooData *metadata)
{
    return AooServer_groupControl(server, group, kAooCtlUpdateGroup, 0, AOO_ARG(metadata));
}

/** \copydoc AooServer::updateUser() */
AOO_INLINE AooError AooServer_updateUser(
    AooServer *server, AooId group, AooId user, const AooData *metadata)
{
    return AooServer_groupControl(server, group, kAooCtlUpdateUser, user, AOO_ARG(metadata));
}

/*----------------------------------------------------------------------*/
