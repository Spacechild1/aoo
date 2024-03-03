#include "AooServer.hpp"

#include <unordered_map>
#include <stdexcept>

namespace {

InterfaceTable *ft;

using scoped_lock = aoo::sync::scoped_lock<aoo::sync::shared_mutex>;
using scoped_shared_lock = aoo::sync::scoped_shared_lock<aoo::sync::shared_mutex>;

aoo::sync::shared_mutex gServerMutex;

using ServerMap = std::unordered_map<int, std::unique_ptr<sc::AooServer>>;
std::unordered_map<World*, ServerMap> gServerMap;

// called from NRT thread
void addServer(World* world, int port, std::unique_ptr<sc::AooServer> server) {
    scoped_lock lock(gServerMutex);
    auto& serverMap = gServerMap[world];
    serverMap[port] = std::move(server);
}

// called from NRT thread
void freeServer(World* world, int port) {
    scoped_lock lock(gServerMutex);
    auto it = gServerMap.find(world);
    if (it != gServerMap.end()) {
        auto& serverMap = it->second;
        serverMap.erase(port);
    }
}

sc::AooServer* findServer(World* world, int port) {
    scoped_shared_lock lock(gServerMutex);
    auto it = gServerMap.find(world);
    if (it != gServerMap.end()) {
        auto& serverMap = it->second;
        auto it2 = serverMap.find(port);
        if (it2 != serverMap.end()) {
            return it2->second.get();
        }
    }
    return nullptr;
}

} // namespace

namespace sc {

// called in NRT thread
AooServer::AooServer(World *world, int port, const char *password)
    : world_(world), port_(port)
{
    server_ = ::AooServer::create(); // does not really fail

    // first set event handler!
    server_->setEventHandler([](void *x, const AooEvent *e, AooThreadLevel) {
        static_cast<sc::AooServer *>(x)->handleEvent(e);
    }, this, kAooEventModeCallback);

    if (password) {
        server_->setPassword(password);
    }

    // setup server
    AooServerSettings settings = AOO_SERVER_SETTINGS_INIT();
    settings.portNumber = port;
    if (auto err = server_->setup(settings);
            err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket_strerror(aoo::socket_errno());
        } else {
            msg = aoo_strerror(err);
        }
        throw std::runtime_error(msg);
    }

    // finally start server threads
    thread_ = std::thread([this]() {
        aoo::sync::lower_thread_priority();
        run();
    });
    udp_thread_ = std::thread([this]() {
        aoo::sync::lower_thread_priority();
        receive();
    });

    LOG_VERBOSE("AooServer: listening on port " << port);
}

AooServer::~AooServer() {
    server_->stop();
    if (thread_.joinable()) {
        thread_.join();
    }
    if (udp_thread_.joinable()) {
        udp_thread_.join();
    }
}

void AooServer::handleEvent(const AooEvent *event){
    char buf[1024];
    osc::OutboundPacketStream msg(buf, sizeof(buf));
    msg << osc::BeginMessage("/aoo/server/event") << port_;

    switch (event->type) {
    case kAooEventClientLogin:
    {
        auto& e = event->clientLogin;

        if (e.error == kAooOk) {
            // TODO: socket address + metadata
            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/server/event") << port_
                << "/client/add" << e.id
                << osc::EndMessage;

            ::sendMsgNRT(world_, msg);
        } else {
            LOG_WARNING("AooServer: client " << e.id << " failed to login: "
                        << aoo_strerror(e.error));
        }

        break;
    }
    case kAooEventClientLogout:
    {
        auto& e = event->clientLogout;

        // TODO: socket address
        char buf[1024];
        osc::OutboundPacketStream msg(buf, sizeof(buf));
        msg << osc::BeginMessage("/aoo/server/event") << port_
            << "/client/remove" << e.id << e.errorCode << e.errorMessage
            << osc::EndMessage;

        ::sendMsgNRT(world_, msg);

        break;
    }
    case kAooEventGroupAdd:
    {
        auto& e = event->groupAdd;
        msg << "/group/add" << e.id << e.name; // TODO metadata
        break;
    }
    case kAooEventGroupRemove:
    {
        auto& e = event->groupRemove;
        msg << "/group/remove" << e.id;
        break;
    }
    case kAooEventGroupJoin:
    {
        auto& e = event->groupJoin;
        msg << "/group/join" << e.groupId << e.userId
            << e.userName << e.clientId;
        break;
    }
    case kAooEventGroupLeave:
    {
        auto& e = event->groupLeave;
        msg << "/group/leave" << e.groupId << e.userId;
        break;
    }
    case kAooEventError:
    {
        auto& e = event->error;
        msg << "/error" << e.errorCode << e.errorMessage;
        break;
    }
    default:
        LOG_DEBUG("AooServer: got unknown event " << event->type);
        return; // don't send event!
    }

    msg << osc::EndMessage;
    ::sendMsgNRT(world_, msg);
}

void AooServer::run() {
    auto err = server_->run(kAooFalse);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket_strerror(aoo::socket_errno());
        } else {
            msg = aoo_strerror(err);
        }
        LOG_ERROR("AooServer: server error: " << msg);
        // TODO: handle error
    }
}

void AooServer::receive() {
    auto err = server_->receiveUDP(kAooFalse);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket_strerror(aoo::socket_errno());
        } else {
            msg = aoo_strerror(err);
        }
        LOG_ERROR("AooServer: UDP error: " << msg);
        // TODO: handle error
    }
}

} // sc

namespace {

template<typename T>
void doCommand(World* world, void *replyAddr, T* cmd, AsyncStageFn fn) {
    DoAsynchronousCommand(world, replyAddr, 0, cmd,
        fn, 0, 0, CmdData::free<T>, 0, 0);
}

void aoo_server_new(World* world, void* user,
    sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();
    auto pwd = args->gets("");

    auto cmdData = CmdData::create<sc::AooServerCreateCmd>(world);
    if (cmdData) {
        cmdData->port = port;
        snprintf(cmdData->password, sizeof(cmdData->password), "%s", pwd);

        auto fn = [](World* world, void* x) {
            auto data = (sc::AooServerCreateCmd *)x;
            auto port = data->port;
            auto pwd = data->password;

            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/server/new") << port;

            if (findServer(world, port)) {
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf),
                    "AooServer on port %d already exists.", port);
                msg << 0 << errbuf;
            } else {
                try {
                    auto server = std::make_unique<sc::AooServer>(world, port, pwd);
                    addServer(world, port, std::move(server));
                    msg << 1; // success
                } catch (const std::runtime_error& e){
                    msg << 0 << e.what();
                }
            }

            msg << osc::EndMessage;
            ::sendMsgNRT(world, msg);

            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

void aoo_server_free(World* world, void* user,
    sc_msg_iter* args, void* replyAddr)
{
    auto port = args->geti();

    auto cmdData = CmdData::create<sc::AooServerCmd>(world);
    if (cmdData) {
        cmdData->port = port;

        auto fn = [](World * world, void* data) {
            auto port = static_cast<sc::AooServerCmd*>(data)->port;

            freeServer(world, port);

            char buf[1024];
            osc::OutboundPacketStream msg(buf, sizeof(buf));
            msg << osc::BeginMessage("/aoo/server/free")
                << port << osc::EndMessage;
            ::sendMsgNRT(world, msg);

            return false; // done
        };

        doCommand(world, replyAddr, cmdData, fn);
    }
}

} // namespace

/*////////////// Setup /////////////////*/

void AooServerLoad(InterfaceTable *inTable){
    ft = inTable;

    AooPluginCmd(aoo_server_new);
    AooPluginCmd(aoo_server_free);
}
