/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#pragma once

#include "aoo/aoo_client.hpp"

#include "osc_stream_receiver.hpp"

#include "common/utils.hpp"
#include "common/sync.hpp"
#include "common/time.hpp"
#include "common/lockfree.hpp"
#include "common/net_utils.hpp"

#include "../imp.hpp"
#include "../binmsg.hpp"

#include "oscpack/osc/OscOutboundPacketStream.h"
#include "oscpack/osc/OscReceivedElements.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <vector>
#include <unordered_map>

#ifndef AOO_NET_CLIENT_PING_INTERVAL
 #define AOO_NET_CLIENT_PING_INTERVAL 5000
#endif

#ifndef AOO_NET_CLIENT_QUERY_INTERVAL
 #define AOO_NET_CLIENT_QUERY_INTERVAL 100
#endif

#ifndef AOO_NET_CLIENT_QUERY_TIMEOUT
 #define AOO_NET_CLIENT_QUERY_TIMEOUT 5000
#endif

struct AooSource;
struct AooSink;

namespace aoo {
namespace net {

class Client;

#if 0
using ip_address_list = std::vector<ip_address, aoo::allocator<ip_address>>;
#else
using ip_address_list = std::vector<ip_address>;
#endif

//---------------------- peer -----------------------------------//

class peer {
public:
    peer(const std::string& groupname, AooId groupid,
         const std::string& username, AooId userid, AooId localid,
         ip_address_list&& addrlist, ip_address_list&& user_relay,
         const ip_address_list& group_relay);

    ~peer();

    bool connected() const {
        return connected_.load(std::memory_order_acquire);
    }

    bool match(const ip_address& addr) const;

    bool match(const std::string& group) const;

    bool match(const std::string& group, const std::string& user) const;

    bool match(AooId group) const;

    bool match(AooId group, AooId user) const;

    bool match_wildcard(AooId group, AooId user) const;

    bool relay() const { return relay_; }

    const ip_address& relay_address() const { return relay_address_; }

    const ip_address_list& user_relay() const { return user_relay_;}

    const std::string& group_name() const { return group_name_; }

    AooId group_id() const { return group_id_; }

    const std::string& user_name() const { return user_name_; }

    AooId user_id() const { return user_id_; }

    AooId local_id() const { return local_id_; }

    const ip_address& address() const {
        return real_address_;
    }

    void send(Client& client, const sendfn& fn, time_tag now);

    void send_message(const osc::OutboundPacketStream& msg, const sendfn& fn);

    void handle_message(Client& client, const char *pattern,
                        osc::ReceivedMessageArgumentIterator it,
                        const ip_address& addr);
private:
    void handle_ping(Client& client, const ip_address& addr, bool reply);

    void send_message(const osc::OutboundPacketStream &msg,
                      const ip_address& addr, const sendfn &fn);

    const std::string group_name_;
    const std::string user_name_;
    const AooId group_id_;
    const AooId user_id_;
    const AooId local_id_;
    bool relay_ = false;
    ip_address_list addrlist_;
    ip_address_list user_relay_;
    ip_address_list group_relay_;
    ip_address real_address_;
    ip_address relay_address_;
    time_tag start_time_;
    double last_pingtime_ = 0;
    std::atomic<bool> connected_{false};
    std::atomic<bool> got_ping_{false};
    bool timeout_ = false;
};

inline std::ostream& operator<<(std::ostream& os, const peer& p) {
    os << "" << p.group_name() << "|" << p.user_name()
       << " (" << p.group_id() << "|" << p.user_id() << ")";
    return os;
}

//---------------------------- udp_client ---------------------------------//

// peer/group messages
struct peer_message {
    peer_message()
        : group_(kAooIdInvalid), user_(kAooIdInvalid),
          tt_(time_tag::immediate()), data_(nullptr) {}

    peer_message(AooId group, AooId user, time_tag tt, const AooDataView& data)
        : group_(group), user_(user), tt_(tt) {
        data_ = (AooDataView *)aoo::allocate(aoo::flat_metadata_size(data));
        aoo::flat_metadata_copy(data, *data_);
    }

    ~peer_message() {
        if (data_) {
            aoo::deallocate(data_, aoo::flat_metadata_size(*data_));
        }
    }

    peer_message(peer_message&& other)
        : group_(other.group_), user_(other.user_), tt_(other.tt_), data_(other.data_) {
        other.data_ = nullptr;
    }

    peer_message& operator=(peer_message&& other) {
        group_ = other.group_; user_ = other.user_; tt_ = other.tt_; data_ = other.data_;
        other.data_ = nullptr;
        return *this;
    }
    // data
    AooId group_;
    AooId user_;
    time_tag tt_;
    AooDataView *data_;
};

class udp_client {
public:
    udp_client(int socket, int port, ip_address::ip_type type)
        : socket_(socket), port_(port), type_(type) {}

    int port() const { return port_; }

    ip_address::ip_type type() const { return type_; }

    AooError handle_osc_message(Client& client, const AooByte *data, int32_t n,
                                const ip_address& addr, int32_t type, AooMsgType onset);

    AooError handle_bin_message(Client& client, const AooByte *data, int32_t n,
                                const ip_address& addr, int32_t type, AooMsgType onset);

    void update(Client& client, const sendfn& fn, time_tag now);

    void start_handshake(ip_address_list&& remote);

    void queue_message(peer_message&& msg);
private:
    void send_server_message(const osc::OutboundPacketStream& msg, const sendfn& fn);

    void handle_server_message(Client& client, const osc::ReceivedMessage& msg, int onset);

    bool is_server_address(const ip_address& addr);

    using scoped_lock = sync::scoped_lock<sync::shared_mutex>;
    using scoped_shared_lock = sync::scoped_shared_lock<sync::shared_mutex>;

    int socket_;
    int port_;
    ip_address::ip_type type_;

    ip_address_list server_addrlist_;
    ip_address_list tcp_addrlist_;
    ip_address_list public_addrlist_;
    ip_address local_address_;
    sync::shared_mutex mutex_;

    double last_ping_time_ = 0;
    std::atomic<double> first_ping_time_{0};

    using message_queue = aoo::unbounded_mpsc_queue<peer_message>;
    message_queue peer_messages_;
};

//------------------------- Client ----------------------------//

enum class client_state {
    disconnected,
    handshake,
    connecting,
    connected
};

class Client final : public AooClient
{
public:
    struct icommand {
        virtual ~icommand(){}
        virtual void perform(Client&) = 0;
    };

    struct ievent {
        virtual ~ievent() {}

        union {
            AooEvent event_;
            AooNetEventError error_event_;
            AooNetEventPeer peer_event_;
            AooNetEventPeerMessage message_event_;
        };
    };

    Client(int socket, const ip_address& address,
           AooFlag flags, AooError *err);

    ~Client();

    AooError AOO_CALL run(AooBool nonBlocking) override;

    AooError AOO_CALL quit() override;

    AooError AOO_CALL addSource(AooSource *src, AooId id) override;

    AooError AOO_CALL removeSource(AooSource *src) override;

    AooError AOO_CALL addSink(AooSink *sink, AooId id) override;

    AooError AOO_CALL removeSink(AooSink *sink) override;

    AooError AOO_CALL connect(
            const AooChar *hostName, AooInt32 port, const AooChar *password,
            const AooDataView *metadata, AooNetCallback cb, void *context) override;

    AooError AOO_CALL disconnect(AooNetCallback cb, void *context) override;

    AooError AOO_CALL joinGroup(
            const AooChar *groupName, const AooChar *groupPwd, const AooDataView *groupMetadata,
            const AooChar *userName, const AooChar *userPwd, const AooDataView *userMetadata,
            const AooIpEndpoint *relayAddress, AooNetCallback cb, void *context) override;

    AooError AOO_CALL leaveGroup(AooId group, AooNetCallback cb, void *context) override;

    AooError AOO_CALL getPeerByName(
            const AooChar *group, const AooChar *user,
            void *address, AooAddrSize *addrlen) override;

    AooError AOO_CALL getPeerById(
            AooId group, AooId user,
            void *address, AooAddrSize *addrlen) override;

    AooError AOO_CALL getPeerByAddress(
            const void *address, AooAddrSize addrlen,
            AooId *groupId, AooId *userId,
            AooChar *groupNameBuf, AooSize *groupNameSize,
            AooChar *userNameBuf, AooSize *userNameSize) override;

    AooError AOO_CALL sendMessage(
            AooId group, AooId user, const AooDataView& msg,
            AooNtpTime timeStamp, AooFlag flags) override;


    AooError AOO_CALL handleMessage(
            const AooByte *data, AooInt32 n,
            const void *addr, AooAddrSize len) override;

    AooError AOO_CALL send(AooSendFunc fn, void *user) override;

    AooError AOO_CALL setEventHandler(
            AooEventHandler fn, void *user, AooEventMode mode) override;

    AooBool AOO_CALL eventsAvailable() override;

    AooError AOO_CALL pollEvents() override;

    AooError AOO_CALL sendRequest(
            const AooNetRequest& request, AooNetCallback callback,
            void *user, AooFlag flags) override;

    AooError AOO_CALL control(
            AooCtl ctl, intptr_t index, void *ptr, size_t size) override;

    //---------------------------------------------------------------------//

    bool handle_peer_message(const osc::ReceivedMessage& msg, int onset,
                             const ip_address& addr);

    struct connect_cmd;
    void perform(const connect_cmd& cmd);

    int try_connect(const ip_address& remote);

    struct login_cmd;
    void perform(const login_cmd& cmd);

    struct timeout_cmd;
    void perform(const timeout_cmd& cmd);

    struct disconnect_cmd;
    void perform(const disconnect_cmd& cmd);

    struct group_join_cmd;
    void perform(const group_join_cmd& cmd);

    void handle_response(const group_join_cmd& cmd, const osc::ReceivedMessage& msg);

    struct group_leave_cmd;
    void perform(const group_leave_cmd& cmd);

    void handle_response(const group_leave_cmd& cmd, const osc::ReceivedMessage& msg);

    struct custom_request_cmd;
    void perform(const custom_request_cmd& cmd);

    void handle_response(const custom_request_cmd& cmd, const osc::ReceivedMessage& msg);

    void perform(const peer_message& msg, const sendfn& fn);

    double ping_interval() const { return ping_interval_.load(); }

    double query_interval() const { return query_interval_.load(); }

    double query_timeout() const { return query_timeout_.load(); }

    void send_event(std::unique_ptr<ievent> e);

    void push_command(std::unique_ptr<icommand>&& cmd);

    double elapsed_time_since(time_tag now) const {
        return time_tag::duration(start_time_, now);
    }

    client_state current_state() const { return state_.load(); }
private:
    // networking
    int socket_ = -1;
    udp_client udp_client_;
    osc_stream_receiver receiver_;
    ip_address local_addr_;
    std::atomic<bool> quit_{false};
    int eventsocket_ = -1;
    bool server_relay_ = false;
    std::vector<char> sendbuffer_;
    // dependants
    struct source_desc {
        AooSource *source;
        AooId id;
    };
    aoo::vector<source_desc> sources_;
    struct sink_desc {
        AooSink *sink;
        AooId id;
    };
    aoo::vector<sink_desc> sinks_;
    // peers
    using peer_list = aoo::rcu_list<peer>;
    using peer_lock = std::unique_lock<peer_list>;
    peer_list peers_;
    // time
    time_tag start_time_;
    double last_ping_time_ = 0;
    // connect/login
    std::atomic<client_state> state_{client_state::disconnected};
    std::unique_ptr<connect_cmd> connection_;
    struct group_membership {
        std::string group_name;
        std::string user_name;
        AooId group_id;
        AooId user_id;
        ip_address_list relay_list;
    };
    std::vector<group_membership> memberships_;
    // commands
    using icommand_ptr = std::unique_ptr<icommand>;
    using command_queue = aoo::unbounded_mpsc_queue<icommand_ptr>;
    command_queue commands_;
    // pending request
    struct callback_cmd : icommand
    {
        callback_cmd(AooNetCallback cb, void *user)
            : cb_(cb), user_(user) {}

        virtual void handle_response(Client& client, const osc::ReceivedMessage& msg) = 0;

        virtual void reply(AooError result, const AooNetResponse *response) const = 0;

        void reply_error(AooError result, const char *msg, int32_t code) const {
            AooNetResponseError response;
            response.type = kAooNetRequestError;
            response.errorCode = code;
            response.errorMessage = msg;
            reply(result, (AooNetResponse*)&response);
        }
    protected:
        void callback(const AooNetRequest& request, AooError result, const AooNetResponse* response) const {
            if (cb_) {
                cb_(user_, &request, result, response);
            }
        }
    private:
        AooNetCallback cb_;
        void *user_;
    };
    using request_map = std::unordered_map<AooId, std::unique_ptr<callback_cmd>>;
    request_map pending_requests_;
    AooId next_token_ = 0;
    // events
    using event_queue = aoo::unbounded_mpsc_queue<std::unique_ptr<ievent>>;
    event_queue events_;
    AooEventHandler eventhandler_ = nullptr;
    void *eventcontext_ = nullptr;
    AooEventMode eventmode_ = kAooEventModeNone;
    // options
    parameter<AooSeconds> ping_interval_{AOO_NET_CLIENT_PING_INTERVAL * 0.001};
    parameter<AooSeconds> query_interval_{AOO_NET_CLIENT_QUERY_INTERVAL * 0.001};
    parameter<AooSeconds> query_timeout_{AOO_NET_CLIENT_QUERY_TIMEOUT * 0.001};

    // methods
    bool wait_for_event(float timeout);

    void receive_data();

    bool signal();

    osc::OutboundPacketStream start_server_message(size_t extra = 0);

    void send_server_message(const osc::OutboundPacketStream& msg);

    void handle_server_message(const osc::ReceivedMessage& msg, int32_t n);

    void handle_login(const osc::ReceivedMessage& msg);

    void handle_peer_add(const osc::ReceivedMessage& msg);

    void handle_peer_remove(const osc::ReceivedMessage& msg);

    void on_socket_error(int err);

    void on_exception(const char *what, const osc::Exception& err,
                      const char *pattern = nullptr);

    void close(bool manual = false);

    group_membership * find_group_membership(const std::string& name);

    group_membership * find_group_membership(AooId id);
public:
    //------------------------ events ----------------------------//

    struct event : ievent
    {
        event(int32_t type){
            event_.type = type;
        }
    };

    struct error_event : ievent
    {
        error_event(int32_t code, const char *msg);
        ~error_event();
    };

    struct peer_event : ievent
    {
        peer_event(int32_t type, peer& p);
        ~peer_event();
    };

    struct message_event : ievent
    {
        message_event(AooId group, AooId user, time_tag tt, const AooDataView& msg);
        ~message_event();
    };

    //---------------------- commands ------------------------//

    struct connect_cmd : callback_cmd
    {
        connect_cmd(const std::string& hostname, int port, const char * pwd,
                    const AooDataView *metadata, AooNetCallback cb, void *user)
            : callback_cmd(cb, user),
              host_(hostname, port), pwd_(pwd ? pwd : ""), metadata_(metadata) {}

        void perform(Client& obj) override {
            obj.perform(*this);
        }

        void handle_response(Client &client, const osc::ReceivedMessage &msg) override {
            // dummy
        }

        void reply(AooError result, const AooNetResponse *response) const override {
            AooNetRequestConnect request;
            request.type = kAooNetRequestConnect;
            request.flags = 0; // TODO
            request.address.hostName = host_.name.c_str();
            request.address.port = host_.port;
            request.password = pwd_.empty() ? nullptr : pwd_.c_str();
            AooDataView md { metadata_.type(), metadata_.data(), metadata_.size() };
            request.metadata = (md.size > 0) ? &md : nullptr;

            callback((AooNetRequest&)request, result, response);
        }
    public:
        ip_host host_;
        std::string pwd_;
        aoo::metadata metadata_;
    };

    struct disconnect_cmd : callback_cmd
    {
        disconnect_cmd(AooNetCallback cb, void *user)
            : callback_cmd(cb, user) {}

        void perform(Client& obj) override {
            obj.perform(*this);
        }

        void handle_response(Client &client, const osc::ReceivedMessage &msg) override {
            // dummy
        }

        void reply(AooError result, const AooNetResponse* response) const override {
            AooNetRequest request;
            request.type = kAooNetRequestConnect;

            callback((AooNetRequest&)request, result, response);
        }
    };

    struct login_cmd : icommand
    {
        login_cmd(ip_address_list&& server_ip, ip_address_list&& public_ip)
            : server_ip_(std::move(server_ip)),
              public_ip_(std::move(public_ip)) {}

        void perform(Client& obj) override {
            obj.perform(*this);
        }
    public:
        ip_address_list server_ip_;
        ip_address_list public_ip_;
    };

    struct timeout_cmd : icommand
    {
        void perform(Client& obj) override {
            obj.perform(*this);
        }
    };

    struct group_join_cmd : callback_cmd
    {
        group_join_cmd(const std::string& group_name, const char * group_pwd, const AooDataView *group_md,
                       const std::string& user_name, const char * user_pwd, const AooDataView *user_md,
                       const ip_host& relay, AooNetCallback cb, void *user)
            : callback_cmd(cb, user),
              group_name_(group_name), group_pwd_(group_pwd ? group_pwd : ""), group_md_(group_md),
              user_name_(user_name), user_pwd_(user_pwd ? user_pwd : ""), user_md_(user_md), relay_(relay) {}

        void perform(Client& obj) override {
            obj.perform(*this);
        }

        void handle_response(Client &client, const osc::ReceivedMessage &msg) override {
            client.handle_response(*this, msg);
        }

        void reply(AooError result, const AooNetResponse* response) const override {
            AooNetRequestGroupJoin request;
            request.type = kAooNetRequestGroupJoin;
            request.flags = 0; // TODO

            request.groupName = group_name_.c_str();
            request.groupPwd = group_pwd_.empty() ? nullptr : group_pwd_.c_str();
            AooDataView group_md { group_md_.type(), group_md_.data(), group_md_.size() };
            request.groupMetadata = (group_md.size > 0) ? &group_md : nullptr;

            request.userName = user_name_.c_str();
            request.userPwd = user_pwd_.empty() ? nullptr : user_pwd_.c_str();
            AooDataView user_md { user_md_.type(), user_md_.data(), user_md_.size() };
            request.userMetadata = (user_md.size > 0) ? &user_md : nullptr;

            AooIpEndpoint relay;
            if (relay_.valid()) {
                relay.hostName = relay_.name.c_str();
                relay.port = relay_.port;
                request.relayAddress = &relay;
            } else {
                request.relayAddress = nullptr;
            }

            callback((AooNetRequest&)request, result, response);
        }
    public:
        std::string group_name_;
        std::string group_pwd_;
        aoo::metadata group_md_;
        std::string user_name_;
        std::string user_pwd_;
        aoo::metadata user_md_;
        ip_host relay_;
    };

    struct group_leave_cmd : callback_cmd
    {
        group_leave_cmd(AooId group, AooNetCallback cb, void *user)
            : callback_cmd(cb, user), group_(group) {}

        void perform(Client& obj) override {
            obj.perform(*this);
        }

        void handle_response(Client &client, const osc::ReceivedMessage &msg) override {
            client.handle_response(*this, msg);
        }

        void reply(AooError result, const AooNetResponse* response) const override {
            AooNetRequestGroupLeave request;
            request.type = kAooNetRequestGroupLeave;
            request.group = group_;

            callback((AooNetRequest&)request, result, response);
        }
    public:
        AooId group_;
    };

    struct custom_request_cmd : callback_cmd
    {
        custom_request_cmd(const AooDataView& data, AooFlag flags, AooNetCallback cb, void *user)
            : callback_cmd(cb, user), data_(&data), flags_(flags) {}

        void perform(Client& obj) override {
            obj.perform(*this);
        }

        void handle_response(Client &client, const osc::ReceivedMessage &msg) override {
            auto it = msg.ArgumentsBegin();
            auto token = (it++)->AsInt32(); // skip
            auto result = (it++)->AsInt32();
            if (result == kAooOk) {
                AooNetResponseCustom response;
                response.type = kAooNetRequestCustom;
                response.flags = (AooFlag)(it++)->AsInt32();
                response.data = osc_read_metadata(it);

                reply(result, (AooNetResponse*)&response);
            } else {
                auto code = (it++)->AsInt32();
                auto msg = (it++)->AsString();
                reply_error(result, msg, code);
            }
        }

        void reply(AooError result, const AooNetResponse* response) const override {
            AooNetRequestCustom request;
            request.type = kAooNetRequestCustom;
            request.flags = flags_;
            request.data.type = data_.type();
            request.data.data = data_.data();
            request.data.size = data_.size();

            callback((AooNetRequest&)request, result, response);
        }
    public:
        aoo::metadata data_;
        AooFlag flags_;
    };
};

} // net
} // aoo
