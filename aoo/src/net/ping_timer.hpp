#pragma once

#include "common/copyable_atomic.hpp"
#include "common/time.hpp"

namespace aoo {
namespace net {

enum class ping_state {
    ping,
    probe,
    inactive
};

struct ping_result {
    ping_state state;
    bool ping;
    double wait;
};

class ping_timer {
public:
    void reset() {
        reset_.store(true);
    }

    void pong() {
        pong_.store(true);
    }

    ping_result update(aoo::time_tag now, const AooPingSettings& settings,
                       bool ping_when_inactive = false) {
        if (reset_.exchange(false)) {
            // reset timer
            next_ping_ = now;
            deadline_ = now + aoo::time_tag::from_seconds(settings.probeTime);
            probe_count_ = 0;
            state_ = ping_state::ping;
        } else if (pong_.exchange(false)) {
            // if we received a pong, extend deadline and go back to ping state
            deadline_ = now + aoo::time_tag::from_seconds(settings.probeTime);
            state_ = ping_state::ping;
        } else if (state_ == ping_state::ping && now >= deadline_) {
            // if deadline is reached, go to probe state
            state_ = ping_state::probe;
            next_ping_ = now; // force probe ping
        } else if (!ping_when_inactive && state_ == ping_state::inactive) {
            // inactive and no pings
            return { ping_state::inactive, false, -1.0 };
        }

        bool ping = false;

        if (now >= next_ping_) {
            // ping
            double interval;
            if (state_ == ping_state::probe) {
                probe_count_++;
                if (probe_count_ > settings.probeCount) {
                    // probe limit reached
                    state_ = ping_state::inactive;
                    if (ping_when_inactive) {
                        // continue with normal pings
                        interval = settings.interval;
                    } else {
                        // no ping!
                        return { state_, false, -1.0 };
                    }
                } else {
                    interval = settings.probeInterval;
                }
            } else {
                // ping or inactive
                interval = settings.interval;
            }
            next_ping_ += aoo::time_tag::from_seconds(interval);
            ping = true;
        }

        auto wait = std::max(0.0, aoo::time_tag::duration(now, next_ping_));
        return { state_, ping, wait };
    }
private:
    aoo::time_tag next_ping_;
    aoo::time_tag deadline_;
    int32_t probe_count_ = 0;
    ping_state state_;
    aoo::copyable_atomic<bool> reset_{true}; // start in reset state
    aoo::copyable_atomic<bool> pong_{false};
};

} // aoo
} // net
