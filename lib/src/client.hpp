/* Copyright (c) 2010-Now Christof Ressi, Winfried Ritsch and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#pragma once

#include "aoo/aoo_net.hpp"
#include "aoo/aoo_utils.hpp"

#include "sync.hpp"
#include "time.hpp"
#include "lockfree.hpp"
#include "net_utils.hpp"
#include "SLIP.hpp"

#include "oscpack/osc/OscOutboundPacketStream.h"
#include "oscpack/osc/OscReceivedElements.h"

#define AOO_NET_CLIENT_PING_INTERVAL 1000
#define AOO_NET_CLIENT_REQUEST_INTERVAL 100
#define AOO_NET_CLIENT_REQUEST_TIMEOUT 5000

namespace aoo {
namespace net {

enum class client_state {
    disconnected,
    connecting,
    handshake,
    login,
    connected
};

enum class command_reason {
    none,
    user,
    timeout,
    error
};

class client final : public iclient {
public:
    struct icommand {
        virtual ~icommand(){}
        virtual void perform(client&) = 0;
    };

    client(void *udpsocket, aoo_sendfn fn, int port);
    ~client();

    int32_t run() override;

    int32_t quit() override;

    int32_t connect(const char *host, int port,
                    const char *username, const char *pwd) override;

    int32_t disconnect() override;

    int32_t group_join(const char *group, const char *pwd) override;

    int32_t group_leave(const char *group) override;

    int32_t handle_message(const char *data, int32_t n, void *addr) override;

    int32_t send() override;

    int32_t events_available() override;

    int32_t handle_events(aoo_eventhandler fn, void *user) override;

    void do_connect(const std::string& host, int port);

    int try_connect(const std::string& host, int port);

    void do_disconnect(command_reason reason = command_reason::none, int error = 0);

    void do_login();

    void do_group_join(const std::string& group, const std::string& pwd);

    void do_group_leave(const std::string& group);
private:
    void *udpsocket_;
    aoo_sendfn sendfn_;
    int udpport_;
    int tcpsocket_ = -1;
    ip_address remote_addr_;
    ip_address public_addr_;
    ip_address local_addr_;
    SLIP sendbuffer_;
    std::vector<uint8_t> pending_send_data_;
    SLIP recvbuffer_;
    // user
    std::string username_;
    std::string password_;
    // time
    time_tag start_time_;
    double last_tcp_ping_time_ = 0;
    // handshake
    std::atomic<client_state> state_{client_state::disconnected};
    double last_udp_ping_time_ = 0;
    double first_udp_ping_time_ = 0;
    // queue
    lockfree::queue<std::unique_ptr<icommand>> commands_;
    spinlock command_lock_;
    void push_command(std::unique_ptr<icommand>&& cmd){
        scoped_lock<spinlock> lock(command_lock_);
        if (commands_.write_available()){
            commands_.write(std::move(cmd));
        }
    }
    lockfree::queue<aoo_event> events_;
    // signal
    std::atomic<bool> quit_{false};
#ifdef _WIN32
    HANDLE waitevent_ = 0;
    HANDLE sockevent_ = 0;
#else
    int waitpipe_[2];
#endif
    // options
    std::atomic<float> ping_interval_{AOO_NET_CLIENT_PING_INTERVAL * 0.001};
    std::atomic<float> request_interval_{AOO_NET_CLIENT_REQUEST_INTERVAL * 0.001};
    std::atomic<float> request_timeout_{AOO_NET_CLIENT_REQUEST_TIMEOUT * 0.001};

    void send_ping_tcp();

    void send_ping_udp();

    void wait_for_event(float timeout);

    void receive_data();

    void send_server_message_tcp(const char *data, int32_t size);

    void send_server_message_udp(const char *data, int32_t size);

    void handle_server_message_tcp(const osc::ReceivedMessage& msg);

    void handle_server_message_udp(const osc::ReceivedMessage& msg);

    void handle_peer_message_udp(const osc::ReceivedMessage& msg,
                                 const ip_address& addr);

    void signal();
};

struct connect_cmd : client::icommand
{
    connect_cmd(const std::string& _host, int _port)
        : host(_host), port(_port){}

    void perform(client &obj) override {
        obj.do_connect(host, port);
    }
    std::string host;
    int port;
};

struct disconnect_cmd : client::icommand
{
    disconnect_cmd(command_reason _reason, int _error = 0)
        : reason(_reason), error(_error){}

    void perform(client &obj) override {
        obj.do_disconnect(reason, error);
    }
    command_reason reason;
    int error;
};

struct login_cmd : client::icommand
{
    void perform(client& obj) override {
        obj.do_login();
    }
};

struct group_join_cmd : client::icommand
{
    group_join_cmd(const std::string& _group, const std::string& _pwd)
        : group(_group), password(_pwd){}

    void perform(client &obj) override {
        obj.do_group_join(group, password);
    }
    std::string group;
    std::string password;
};

struct group_leave_cmd : client::icommand
{
    group_leave_cmd(const std::string& _group)
        : group(_group){}

    void perform(client &obj) override {
        obj.do_group_leave(group);
    }
    std::string group;
};

} // net
} // aoo
