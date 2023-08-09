#pragma once

#include "Aoo.hpp"
#include "AooClient.hpp"

#include "common/lockfree.hpp"
#include "common/net_utils.hpp"
#include "common/sync.hpp"
#include "common/time.hpp"
#include "common/udp_server.hpp"

#include <unordered_map>
#include <cstring>
#include <stdio.h>
#include <errno.h>
#include <iostream>

#include <thread>
#include <atomic>

#define NETWORK_THREAD_POLL 0

#if NETWORK_THREAD_POLL
#define POLL_INTERVAL 0.001 // seconds
#endif

#define DEBUG_THREADS 0

class AooNode final : public INode {
    friend class INode;
public:
    AooNode(World *world, int port);

    ~AooNode() override;

    int port() const override { return port_; }

    AooClient * client() override {
        return client_.get();
    }

    bool registerClient(sc::AooClient *c) override;

    void unregisterClient(sc::AooClient *c) override;

    void notify() override {
        client_->notify();
    }

    bool getSinkArg(sc_msg_iter *args, aoo::ip_address& addr,
                    AooId &id) const override {
        return getEndpointArg(args, addr, &id, "sink");
    }

    bool getSourceArg(sc_msg_iter *args, aoo::ip_address& addr,
                      AooId &id) const override {
        return getEndpointArg(args, addr, &id, "source");
    }

    bool getPeerArg(sc_msg_iter *args, aoo::ip_address& addr) const override {
        return getEndpointArg(args, addr, nullptr, "peer");
    }
private:
    using unique_lock = aoo::sync::unique_lock<aoo::sync::mutex>;
    using scoped_lock = aoo::sync::scoped_lock<aoo::sync::mutex>;

    int port_ = 0;
    aoo::ip_address::ip_type type_ = aoo::ip_address::ip_type::Unspec; // TODO
    AooClient::Ptr client_;
    std::thread clientThread_;
    sc::AooClient *clientObject_ = nullptr;
    aoo::sync::mutex clientObjectMutex_;
    // threading
#if NETWORK_THREAD_POLL
    std::thread iothread_;
    std::atomic<bool> quit_{false};
#else
    std::thread sendThread_;
    std::thread receiveThread_;
#endif

    void handleEvent(const AooEvent *event);

    // private methods
    bool getEndpointArg(sc_msg_iter *args, aoo::ip_address& addr,
                        int32_t *id, const char *what) const;

#if NETWORK_THREAD_POLL
    void performNetworkIO();
#else
    void send();

    void receive();
#endif
};
