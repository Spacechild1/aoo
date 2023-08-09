#include "AooNode.hpp"

// public methods

AooNode::AooNode(World *world, int port) {
    LOG_DEBUG("create AooClient on port " << port);

    client_ = AooClient::create(nullptr);

    AooClientSettings settings;
    AooClientSettings_init(&settings);
    settings.portNumber = port;
    if (auto err = client_->setup(settings); err != kAooOk) {
        // TODO: throw error
    }

    // set event handler *before* starting network threads
    client_->setEventHandler([](void *user, const AooEvent *event, AooThreadLevel level) {
        static_cast<AooNode *>(user)->handleEvent(event);
    }, this, kAooEventModeCallback);

#if NETWORK_THREAD_POLL
    // start network I/O thread
    LOG_DEBUG("start network thread");
    iothread_ = std::thread([this](){
        aoo::sync::lower_thread_priority();
        performNetworkIO();
    });
#else
    // start send thread
    LOG_DEBUG("start network send thread");
    sendThread_ = std::thread([this](){
        aoo::sync::lower_thread_priority();
        send();
    });
    // start receive thread
    LOG_DEBUG("start network receive thread");
    receiveThread_ = std::thread([this](){
        aoo::sync::lower_thread_priority();
        receive();
    });
#endif

    LOG_VERBOSE("new node on port " << port);
}

AooNode::~AooNode() {
    // notify and quit client thread
    client_->quit();
    if (clientThread_.joinable()){
        clientThread_.join();
    }

#if NETWORK_THREAD_POLL
    LOG_DEBUG("join network thread");
    // notify I/O thread
    quit_ = true;
    if (iothread_.joinable()) {
        iothread_.join();
    }
#else
    LOG_DEBUG("join network threads");
    // join threads
    if (sendThread_.joinable()) {
        sendThread_.join();
    }
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
#endif

    LOG_VERBOSE("released node on port " << port_);
}

using NodeMap = std::unordered_map<int, std::weak_ptr<AooNode>>;

static aoo::sync::mutex gNodeMapMutex;
static std::unordered_map<World *, NodeMap> gNodeMap;

static NodeMap& getNodeMap(World *world) {
    // we only have to protect against concurrent insertion;
    // the object reference is stable.
    aoo::sync::scoped_lock<aoo::sync::mutex> lock(gNodeMapMutex);
    return gNodeMap[world];
}

INode::ptr INode::get(World *world, int port){
    std::shared_ptr<AooNode> node;

    auto& nodeMap = getNodeMap(world);

    // find or create node
    auto it = nodeMap.find(port);
    if (it != nodeMap.end()){
        node = it->second.lock();
    }

    if (!node) {
        // finally create aoo node instance
        try {
            node = std::make_shared<AooNode>(world, port);
            nodeMap.emplace(port, node);
        } catch (const std::exception& e) {
            LOG_ERROR("AooNode: " << e.what());
        }
    }

    return node;
}

bool AooNode::registerClient(sc::AooClient *c){
    aoo::sync::unique_lock<aoo::sync::mutex> lock(clientObjectMutex_);
    if (clientObject_){
        LOG_ERROR("AooClient on port " << port()
                  << " already exists!");
        return false;
    }
    clientObject_ = c;
    lock.unlock();

    if (!clientThread_.joinable()){
        // lazily create client thread
        clientThread_ = std::thread([this](){
            client_->run(kAooFalse);
        });
    }

    return true;
}

void AooNode::unregisterClient(sc::AooClient *c) {
    assert(clientObject_ == c);
    aoo::sync::unique_lock<aoo::sync::mutex> lock(clientObjectMutex_);
    clientObject_ = nullptr;
}

// private methods

void AooNode::handleEvent(const AooEvent *event) {
    // forward event to client object, if registered.
    aoo::sync::unique_lock<aoo::sync::mutex> lock(clientObjectMutex_);
    if (clientObject_) {
        clientObject_->handleEvent(event);
    }
}

bool AooNode::getEndpointArg(sc_msg_iter *args, aoo::ip_address& addr,
                             int32_t *id, const char *what) const
{
    if (args->remain() < 2){
        LOG_ERROR("too few arguments for " << what);
        return false;
    }

    auto s = args->gets("");

    // first try peer (group|user)
    if (args->nextTag() == 's'){
        auto group = s;
        auto user = args->gets();
        // we can't use length_ptr() because socklen_t != int32_t on many platforms
        AooAddrSize addrlen = aoo::ip_address::max_length;
        if (client_->findPeerByName(group, user, nullptr, nullptr,
                                    addr.address_ptr(), &addrlen) == kAooOk) {
            addr.resize(addrlen);
        } else {
            LOG_ERROR("couldn't find peer " << group << "|" << user);
            return false;
        }
    } else {
        // otherwise try host|port
        auto host = s;
        int port = args->geti();
        auto result = aoo::ip_address::resolve(host, port, type_);
        if (!result.empty()){
            addr = result.front(); // pick the first result
        } else {
            LOG_ERROR("couldn't resolve hostname '"
                      << host << "' for " << what);
            return false;
        }
    }

    if (id){
        if (args->remain()){
            AooId i = args->geti(-1);
            if (i >= 0){
                *id = i;
            } else {
                LOG_ERROR("bad ID '" << i << "' for " << what);
                return false;
            }
        } else {
            LOG_ERROR("too few arguments for " << what);
            return false;
        }
    }

    return true;
}

#if NETWORK_THREAD_POLL
void AooNode::performNetworkIO() {
    try {
        while (!quit_.load(std::memory_order_relaxed)) {
            server_.run(POLL_INTERVAL);

            if (update_.exchange(false, std::memory_order_acquire)){
                scoped_lock lock(clientMutex_);
            #if DEBUG_THREADS
                std::cout << "send messages" << std::endl;
            #endif
                client_->send(send, this);
            }
        }
    } catch (const aoo::udp_error& e) {
        LOG_ERROR("AooNode: network error: " << e.what());
        // TODO handle error
    }

}
#else
void AooNode::send(){
    auto err = client_->send(kAooFalse);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket_strerror(aoo::socket_errno());
        } else {
            msg = aoo_strerror(err);
        }
        LOG_ERROR("AooNote: UDP send error: " << msg);
        // TODO: handle error
    }
}

void AooNode::receive() {
    auto err = client_->receive(kAooFalse);
    if (err != kAooOk) {
        std::string msg;
        if (err == kAooErrorSocket) {
            msg = aoo::socket_strerror(aoo::socket_errno());
        } else {
            msg = aoo_strerror(err);
        }
        LOG_ERROR("AooNote: UDP receive error: " << msg);
        // TODO: handle error
    }
}

#endif
