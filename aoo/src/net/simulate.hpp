#pragma once

#include "common/priority_queue.hpp"

#include "detail.hpp"

#include <random>

namespace aoo {
namespace net {

class Client;

class network_simulator {
public:
    sendfn wrap(const sendfn& fn, time_tag tt);

    void set_packet_loss(float pct) {
        packet_loss_.store(pct);
    }

    void set_packet_reorder(AooSeconds s) {
        packet_reorder_.store(s);
    }

    void set_packet_jitter(bool b) {
        packet_jitter_.store(b);
    }
private:
    int32_t do_send(const AooByte *data, AooInt32 size,
                    const void *address, AooAddrSize addrlen,
                    AooFlag flag);

    // packet loss percentage
    parameter<float> packet_loss_{0};
    // max. delay for packet reordering
    parameter<float> packet_reorder_{0};
    // true: store packets in the queue instead of sending them out.
    // false: send all stored packets, followed by new packets
    // you can effectively simulate jitter by turning this option
    // on and off at the desired rate.
    parameter<bool> packet_jitter_{false};
    sendfn fn_;
    time_tag tt_;

    struct netpacket {
        std::vector<AooByte> data;
        aoo::ip_address addr;
        time_tag tt;

        bool operator> (const netpacket& other) const {
            return tt > other.tt;
        }
    };

    using packet_queue = aoo::priority_queue<netpacket, std::greater<netpacket>>;
    packet_queue packet_queue_;
};

inline sendfn network_simulator::wrap(const sendfn& fn, time_tag tt) {
    auto drop = packet_loss_.load();
    auto reorder = packet_reorder_.load();
    auto jitter = packet_jitter_.load();

    // dispatch delayed packets - unless we want to simulate jitter
    if (!jitter) {
        while (!packet_queue_.empty()) {
            auto& p = packet_queue_.top();
            if (p.tt <= tt) {
                fn(p.data.data(), p.data.size(), p.addr);
                packet_queue_.pop();
            } else {
                break;
            }
        }
    }

    if (drop > 0 || reorder > 0 || jitter) {
        // wrap send function
        fn_ = fn;
        tt_ = tt;

        auto wrapfn = [](void *user, const AooByte *data, AooInt32 size,
                         const void *address, AooAddrSize addrlen, AooFlag flag) -> AooInt32 {
            return static_cast<network_simulator*>(user)->do_send(data, size, address, addrlen, flag);
        };

        return sendfn(wrapfn, this);
    } else {
        // just pass original function
        return fn;
    }
}

inline int32_t network_simulator::do_send(const AooByte *data, AooInt32 size,
                                          const void *address, AooAddrSize addrlen,
                                          AooFlag flag) {
    thread_local std::default_random_engine gen(std::random_device{}());
    std::uniform_real_distribution dist;

    if (auto drop = packet_loss_.load(); drop > 0) {
        if (dist(gen) <= drop) {
            // LOG_DEBUG("AooClient: drop packet");
            return 0; // drop packet
        }
    }

    aoo::ip_address addr((const struct sockaddr *)address, addrlen);

    auto jitter = packet_jitter_.load();
    auto reorder = packet_reorder_.load();
    if (jitter || (reorder > 0)) {
        // queue for later
        netpacket p;
        p.data.assign(data, data + size);
        p.addr = addr;
        p.tt = tt_;
        if (reorder > 0) {
            // add random delay
            auto delay = dist(gen) * reorder;
            p.tt += time_tag::from_seconds(delay);
        }
        // LOG_DEBUG("AooClient: delay packet (tt: " << p.tt << ")");
        packet_queue_.push(std::move(p));
    } else {
        // send immediately
        fn_(data, size, addr);
    }

    return 0;
}

} // net
} // aoo
