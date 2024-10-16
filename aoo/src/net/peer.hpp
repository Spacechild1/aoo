#pragma once

#include "detail.hpp"
#include "message_buffer.hpp"
#include "ping_timer.hpp"

namespace aoo {
namespace net {

// OSC peer message:
const int32_t kMessageMaxAddrSize = kAooMsgDomainLen + kAooMsgPeerLen + 16 + kAooMsgDataLen;
// address pattern string: max 16 bytes
// typetag string: max. 12 bytes
// args (including type + blob size): max. 44 bytes
const int32_t kMessageHeaderSize = kMessageMaxAddrSize + 56;

// binary peer message:
// args: 28 bytes (max.)
const int32_t kBinMessageHeaderSize = kAooBinMsgLargeHeaderSize + 28;

class Client;

struct message;

struct message_packet {
    AooDataType type;
    int32_t size;
    const AooByte *data;
    time_tag tt;
    int32_t sequence;
    int32_t total_size;
    int32_t num_frames;
    int32_t frame_index;
    bool reliable;
};

struct message_ack {
    int32_t sequence;
    int32_t frame_index;
};

struct peer_args {
    std::string_view group_name;
    std::string_view user_name;
    AooId group_id;
    AooId user_id;
    AooId local_id;
    AooFlag flags;
    std::string_view version_string;
    const AooData *metadata;
    ip_address::ip_type address_family;
    bool use_ipv4_mapped;
    bool binary;
    ip_address_list address_list;
    ip_address_list user_relay;
    ip_address_list relay_list;
};

class peer {
public:
    // NB: be must set everything in the constructor to avoid race conditions!
    peer(peer_args&& args);

    ~peer();

    bool connected() const {
        return connected_.load(std::memory_order_acquire);
    }

    bool match(const ip_address& addr) const;

    bool match(std::string_view group) const;

    bool match(std::string_view group, std::string_view user) const;

    bool match(AooId group) const;

    bool match(AooId group, AooId user) const;

    bool match_wildcard(AooId group, AooId user) const;

    ip_address address() const;

    bool need_relay() const { return flags_ & kAooPeerNeedRelay; }

    const ip_address& relay_address() const { return relay_address_; }

    const ip_address_list& user_relay() const { return user_relay_;}

    const std::string& group_name() const { return group_name_; }

    AooId group_id() const { return group_id_; }

    const std::string& user_name() const { return user_name_; }

    AooId user_id() const { return user_id_; }

    AooId local_id() const { return local_id_; }

    const std::string& version() const { return version_; }

    AooFlag flags() const { return flags_; }

    const aoo::metadata& metadata() const { return metadata_; }

    void send(Client& client, const sendfn& fn, time_tag now,
              const AooPingSettings& settings);

    void send_message(const message& msg, const sendfn& fn, int32_t packet_size, bool binary);

    void handle_osc_message(Client& client, std::string_view pattern,
                            osc::ReceivedMessageArgumentIterator it,
                            int remaining, const ip_address& addr);

    void handle_bin_message(Client& client, const AooByte *data,
                            AooSize size, int onset, const ip_address& addr);
private:
    void handle_ping(Client& client, osc::ReceivedMessageArgumentIterator it,
                     const ip_address& addr);

    void handle_pong(Client& client, osc::ReceivedMessageArgumentIterator it,
                     const ip_address& addr);

    void handle_first_ping(Client& client, const aoo::ip_address& addr);

    void handle_client_message(Client& client, osc::ReceivedMessageArgumentIterator it);

    void handle_client_message(Client& client, const AooByte *data, AooSize size);

    void do_handle_client_message(Client& client, const message_packet& p, AooFlag flags);

    void handle_ack(Client& client, osc::ReceivedMessageArgumentIterator it, int remaining);

    void handle_ack(Client& client, const AooByte *data, AooSize size);

    void do_send(Client& client, const sendfn& fn, time_tag now,
                 const AooPingSettings& settings);

    void send_packet_osc(const message_packet& frame, const sendfn& fn) const;

    void send_packet_bin(const message_packet& frame, const sendfn& fn) const;

    void send_ack(const message_ack& ack, const sendfn& fn);

    void send(const osc::OutboundPacketStream& msg, const sendfn& fn) const {
        send((const AooByte *)msg.Data(), msg.Size(), fn);
    }

    void send(const osc::OutboundPacketStream& msg,
              const ip_address& addr, const sendfn& fn) const {
        send((const AooByte *)msg.Data(), msg.Size(), addr, fn);
    }

    // only called if peer is connected!
    void send(const AooByte *data, AooSize size, const sendfn &fn) const {
        assert(connected());
        send(data, size, real_address_, fn);
    }

    void send(const AooByte *data, AooSize size,
              const ip_address& addr, const sendfn &fn) const;

    const std::string group_name_;
    const std::string user_name_;
    const AooId group_id_;
    const AooId user_id_;
    const AooId local_id_;
    AooFlag flags_;
    std::string version_;
    ip_address::ip_type address_family_;
    bool binary_;
    bool use_ipv4_mapped_;
    bool timeout_ = false;
    bool active_ = true;
    aoo::metadata metadata_;
    ip_address_list addrlist_;
    ip_address_list user_relay_;
    ip_address_list relay_list_;
    ip_address real_address_; // IPv4-mapped if peer-to-peer, unmapped if relayed
    ip_address relay_address_;
    ping_timer ping_timer_;
    aoo::time_tag next_handshake_;
    aoo::time_tag handshake_deadline_;
    time_tag ping_tt1_;
    time_tag ping_tt2_;
    std::atomic<float> average_rtt_{0};
    std::atomic<bool> connected_{false};
    std::atomic<bool> got_ping_{false};
    std::atomic<bool> binary_ack_{false};
    int32_t next_sequence_reliable_ = 0;
    int32_t next_sequence_unreliable_ = 0;
    message_send_buffer send_buffer_;
    message_receive_buffer receive_buffer_;
    received_message current_msg_;
    aoo::unbounded_mpsc_queue<message_ack> send_acks_;
    aoo::unbounded_mpsc_queue<message_ack> received_acks_;
};

inline std::ostream& operator<<(std::ostream& os, const peer& p) {
    os << "" << p.group_name() << "|" << p.user_name()
       << " (" << p.group_id() << "|" << p.user_id() << ")";
    return os;
}

} // net
} // aoo
