#include "AooClient.hpp"

#include <unordered_map>

namespace {

using scoped_lock = aoo::sync::scoped_lock<aoo::sync::shared_mutex>;
using scoped_shared_lock = aoo::sync::scoped_shared_lock<aoo::sync::shared_mutex>;

InterfaceTable *ft;

aoo::sync::shared_mutex gClientMutex;

using ClientMap = std::unordered_map<int, std::unique_ptr<sc::AooClient>>;
std::unordered_map<World*, ClientMap> gClientMap;

// called from NRT thread
void createClient(World* world, int port) {
    auto client = std::make_unique<sc::AooClient>(world, port);
    scoped_lock lock(gClientMutex);
    auto& clientMap = gClientMap[world];
    clientMap[port] = std::move(client);
}

// called from NRT thread
void freeClient(World* world, int port) {
    scoped_lock lock(gClientMutex);
    auto it = gClientMap.find(world);
    if (it != gClientMap.end()) {
        auto& clientMap = it->second;
        clientMap.erase(port);
    }
}

sc::AooClient* findClient(World* world, int port) {
    scoped_shared_lock lock(gClientMutex);
    auto it = gClientMap.find(world);
    if (it != gClientMap.end()) {
        auto& clientMap = it->second;
        auto it2 = clientMap.find(port);
        if (it2 != clientMap.end()) {
            return it2->second.get();
        }
    }
    return nullptr;
}

} // namespace

namespace sc {

// called in NRT thread
AooClient::AooClient(World *world, int32_t port)
    : world_(world) {
    auto node = INode::get(world, port);
    if (node) {
        if (node->registerClient(this)) {
            LOG_VERBOSE("new AooClient on port " << port);
            node_ = node;
        }
    }
}

// called in NRT thread
AooClient::~AooClient() {
    if (node_) {
        node_->unregisterClient(this);
    }

    LOG_DEBUG("~AooClient");
}

void AooClient::connect(int token, const char* host,
                        int port, const char *pwd) {
    if (!node_){
        sendError("/aoo/client/connect", token, kAooErrorInternal);
        return;
    }

    auto cb = [](void* x, const AooRequest *request,
            AooError result, const AooResponse *response) {
        auto data = (RequestData *)x;
        auto client = data->client;
        auto token = data->token;

        if (result == kAooErrorNone) {
            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/client/connect")
                << client->node_->port() << token << (int32_t)1
                << response->connect.clientId
                << osc::EndMessage;

            ::sendMsgNRT(client->world_, msg);
        } else {
            client->sendError("/aoo/client/connect", token, result, *response);
        }

        delete data;
    };

    node_->client()->connect(host, port, pwd, nullptr,
                             cb, new RequestData { this, token });
}

void AooClient::disconnect(int token) {
    if (!node_) {
        sendError("/aoo/client/disconnect", token, kAooErrorInternal);
        return;
    }

    auto cb = [](void* x, const AooRequest *request,
            AooError result, const AooResponse *response) {
        auto data = (RequestData *)x;
        auto client = data->client;
        auto token = data->token;

        if (result == kAooErrorNone) {
            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/client/disconnect")
                << client->node_->port() << token << (int32_t)1
                << osc::EndMessage;

            ::sendMsgNRT(client->world_, msg);
        } else {
            client->sendError("/aoo/client/disconnect", token, result, *response);
        }

        delete data;
    };

    node_->client()->disconnect(cb, new RequestData { this, token });
}

void AooClient::joinGroup(int token, const char* groupName, const char *groupPwd,
                          const char *userName, const char *userPwd) {
    if (!node_) {
        sendError("/aoo/client/group/join", token, kAooErrorInternal);
        return;
    }

    auto cb = [](void* x, const AooRequest *request,
            AooError result, const AooResponse* response) {
        auto data = (RequestData *)x;
        auto client = data->client;
        auto token = data->token;

        if (result == kAooErrorNone) {
            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/client/group/join")
                << client->node_->port() << token << (int32_t)1
                << response->groupJoin.groupId << response->groupJoin.userId
                << osc::EndMessage;

            ::sendMsgNRT(client->world_, msg);
        } else {
            client->sendError("/aoo/client/group/join", token, result, *response);
        }

        delete data;
    };

    node_->client()->joinGroup(groupName, groupPwd, nullptr,
                               userName, userPwd, nullptr, nullptr,
                               cb, new RequestData { this, token });
}

void AooClient::leaveGroup(int token, AooId group) {
    if (!node_) {
        sendError("/aoo/client/group/leave", token, kAooErrorInternal);
        return;
    }

    auto cb = [](void* x, const AooRequest *request,
            AooError result, const AooResponse* response) {
        auto data = (RequestData *)x;
        auto client = data->client;
        auto token = data->token;

        if (result == kAooErrorNone) {
            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/client/group/leave")
                << client->node_->port() << token << (int32_t)1
                << osc::EndMessage;

            ::sendMsgNRT(client->world_, msg);
        } else {
            client->sendError("/aoo/client/group/leave", token, result, *response);
        }

        delete data;
    };

    node_->client()->leaveGroup(group, cb, new RequestData { this, token });
}

// called from network thread
void AooClient::handleEvent(const AooEvent* event) {
    char buf[1024];
    osc::OutboundPacketStream msg(buf, sizeof(buf));
    msg << osc::BeginMessage("/aoo/client/event") << node_->port();

    switch (event->type) {
    case kAooEventPeerMessage:
    {
        auto& e = event->peerMessage;
        // TODO
        break;
    }
    case kAooEventDisconnect:
    {
        msg << "/disconnect" << event->disconnect.errorCode
            << event->disconnect.errorMessage;
        break;
    }
    case kAooEventPeerHandshake:
    {
        auto& e = event->peerHandshake;
        aoo::ip_address addr(e.address);
        msg << "/peer/handshake" << e.groupId << e.userId
            << e.groupName << e.userName << addr.name() << addr.port();
        break;
    }
    case kAooEventPeerTimeout:
    {
        auto& e = event->peerTimeout;
        aoo::ip_address addr(e.address);
        msg << "/peer/timeout" << e.groupId << e.userId
            << e.groupName << e.userName << addr.name() << addr.port();
        break;
    }
    case kAooEventPeerJoin:
    {
        auto& e = event->peerJoin;
        aoo::ip_address addr(e.address);
        msg << "/peer/join" << e.groupId << e.userId
            << e.groupName << e.userName << addr.name() << addr.port();
        break;
    }
    case kAooEventPeerLeave:
    {
        auto& e = event->peerLeave;
        aoo::ip_address addr(e.address);
        msg << "/peer/leave" << e.groupId << e.userId
            << e.groupName << e.userName << addr.name() << addr.port();
        break;
    }
    case kAooEventPeerPing:
    {
        auto& e = event->peerPing;
        auto delta1 = aoo_ntpTimeDuration(e.t1, e.t2);
        auto delta2 = aoo_ntpTimeDuration(e.t3, e.t4);
        auto total_rtt = aoo_ntpTimeDuration(e.t1, e.t4);
        auto network_rtt = total_rtt - aoo_ntpTimeDuration(e.t2, e.t3);
        msg << "/peer/ping" << e.group << e.user
            << delta1 << delta2 << network_rtt << total_rtt;
        break;
    }
    case kAooEventError:
    {
        msg << "/error" << event->error.errorCode << event->error.errorMessage;
        break;
    }
    default:
        LOG_DEBUG("AooClient: got unknown event " << event->type);
        return; // don't send event!
    }

    msg << osc::EndMessage;
    ::sendMsgNRT(world_, msg);
}

void AooClient::sendError(const char *cmd, AooId token, AooError error,
                          int errcode, const char *errmsg) {
    char buf[1024];
    osc::OutboundPacketStream msg(buf, sizeof(buf));
    msg << osc::BeginMessage(cmd) << node_->port() << token
        << error << aoo_strerror(error) << errcode << errmsg
        << osc::EndMessage;

    ::sendMsgNRT(world_, msg);
}

} // namespace sc

namespace {

template<typename T>
void doCommand(World* world, void *replyAddr, T* cmd, AsyncStageFn fn) {
    DoAsynchronousCommand(world, replyAddr, 0, cmd,
        fn, 0, 0, CmdData::free<T>, 0, 0);
}

void aoo_client_new(World* world, void* user, sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();

    auto cmdData = CmdData::create<sc::AooClientCmd>(world);
    if (cmdData) {
        cmdData->port = port;

        auto fn = [](World* world, void* data) {
            auto port = static_cast<sc::AooClientCmd*>(data)->port;
            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/client/new") << port;

            if (findClient(world, port)) {
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf),
                    "AooClient on port %d already exists.", port);
                msg << (int32_t)0 << errbuf;
            } else {
                createClient(world, port);
                msg << (int32_t)1;
            }

            msg << osc::EndMessage;
            ::sendMsgNRT(world, msg);

            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

void aoo_client_free(World* world, void* user, sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();

    auto cmdData = CmdData::create<sc::AooClientCmd>(world);
    if (cmdData) {
        cmdData->port = port;

        auto fn = [](World * world, void* data) {
            auto port = static_cast<sc::AooClientCmd*>(data)->port;

            freeClient(world, port);

            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/client/free")
                << port << osc::EndMessage;
            ::sendMsgNRT(world, msg);

            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

// called from the NRT
sc::AooClient* getClient(World *world, int port, const char *cmd)
{
    auto client = findClient(world, port);
    if (!client){
        char errbuf[256];
        snprintf(errbuf, sizeof(errbuf),
            "couldn't find AooClient on port %d", port);

        char buf[1024];
        osc::OutboundPacketStream msg(buf, sizeof(buf));

        msg << osc::BeginMessage(cmd) << port << (int32_t)0 << errbuf
            << osc::EndMessage;

        ::sendMsgNRT(world, msg);
    }
    return client;
}

void aoo_client_connect(World* world, void* user,
    sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();
    auto token = args->geti();
    auto serverHost = args->gets();
    auto serverPort = args->geti();
    auto password = args->gets();

    auto cmdData = CmdData::create<sc::ConnectCmd>(world);
    if (cmdData) {
        cmdData->port = port;
        cmdData->token = token;
        snprintf(cmdData->serverHost, sizeof(cmdData->serverHost),
            "%s", serverHost);
        cmdData->serverPort = serverPort;
        snprintf(cmdData->password, sizeof(cmdData->password),
            "%s", password);

        auto fn = [](World * world, void* cmdData) {
            auto data = (sc::ConnectCmd*)cmdData;
            auto client = getClient(world, data->port, "/aoo/client/connect");
            if (client) {
                client->connect(data->token, data->serverHost,
                                data->serverPort, data->password);
            }
            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

void aoo_client_disconnect(World* world, void* user,
    sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();
    auto token = args->geti();

    auto cmdData = CmdData::create<sc::AooClientCmd>(world);
    if (cmdData) {
        cmdData->port = port;
        cmdData->token = token;

        auto fn = [](World * world, void* cmdData) {
            auto data = (sc::AooClientCmd *)cmdData;
            auto client = getClient(world, data->port, "/aoo/client/disconnect");
            if (client) {
                client->disconnect(data->token);
            }

            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

void aoo_client_group_join(World* world, void* user,
    sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();
    auto token = args->geti();
    auto groupName = args->gets();
    auto groupPwd = args->gets();
    auto userName = args->gets();
    auto userPwd = args->gets();

    auto cmdData = CmdData::create<sc::GroupJoinCmd>(world);
    if (cmdData) {
        cmdData->port = port;
        cmdData->token = token;
        snprintf(cmdData->groupName, sizeof(cmdData->groupName),
            "%s", groupName);
        snprintf(cmdData->groupPwd, sizeof(cmdData->groupPwd),
            "%s", groupPwd);
        snprintf(cmdData->userName, sizeof(cmdData->userName),
            "%s", userName);
        snprintf(cmdData->userPwd, sizeof(cmdData->userPwd),
            "%s", userPwd);

        auto fn = [](World * world, void* cmdData) {
            auto data = (sc::GroupJoinCmd *)cmdData;
            auto client = getClient(world, data->port, "/aoo/client/group/join");
            if (client) {
                client->joinGroup(data->token, data->groupName, data->groupPwd,
                                  data->userName, data->userPwd);
            }

            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

void aoo_client_group_leave(World* world, void* user,
    sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();
    auto token = args->geti();
    auto group = args->geti();

    auto cmdData = CmdData::create<sc::GroupLeaveCmd>(world);
    if (cmdData) {
        cmdData->port = port;
        cmdData->token = token;
        cmdData->group = group;

        auto fn = [](World * world, void* cmdData) {
            auto data = (sc::GroupLeaveCmd *)cmdData;
            auto client = getClient(world, data->port, "/aoo/client/group/leave");
            if (client) {
                client->leaveGroup(data->token, data->group);
            }

            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

} // namespace

/*////////////// Setup /////////////////*/

void AooClientLoad(InterfaceTable *inTable){
    ft = inTable;

    AooPluginCmd(aoo_client_new);
    AooPluginCmd(aoo_client_free);
    AooPluginCmd(aoo_client_connect);
    AooPluginCmd(aoo_client_disconnect);
    AooPluginCmd(aoo_client_group_join);
    AooPluginCmd(aoo_client_group_leave);
}
