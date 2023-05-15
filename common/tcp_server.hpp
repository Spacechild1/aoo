#pragma once

#include <functional>
#include <vector>
#include <atomic>

#ifdef _WIN32
# include <winsock2.h>
#else
# include <sys/poll.h>
# include <unistd.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <errno.h>
#endif

#include "common/net_utils.hpp"

namespace aoo {

class tcp_error : public std::exception {
public:
    tcp_error(int code, std::string msg)
        : code_(code), msg_(std::move(msg)) {}

    const char *what() const noexcept override {
        return msg_.c_str();
    }

    int code() const { return code_; }
private:
    int code_;
    std::string msg_;
};

class tcp_server
{
public:
#ifdef _WIN32
    static const AooSocket invalid_socket = (AooSocket)INVALID_SOCKET;
#else
    static const AooSocket invalid_socket = -1;
#endif

    tcp_server() {}
    ~tcp_server();

    tcp_server(const tcp_server&) = delete;
    tcp_server& operator=(const tcp_server&) = delete;

    using reply_func = std::function<int(const AooByte *data, AooSize size)>;

    // returns client ID
    using accept_handler = std::function<AooId(const aoo::ip_address& address, reply_func)>;

    using receive_handler = std::function<void(AooId client, int errorcode, const AooByte *data,
                                               AooSize size, const ip_address& address)>;

    void start(int port, accept_handler accept, receive_handler receive);
    bool run(double timeout = -1.0);
    bool running() const { return running_.load(std::memory_order_relaxed); }
    void stop();
    void notify();

    int send(AooId client, const AooByte *data, AooSize size);
    bool close(AooId client);
    int client_count() const { return client_count_; }
private:
    struct client {
        ip_address address;
        AooSocket socket;
        AooId id;
    };

    void accept_client();
    void handle_accept_error(int code, const ip_address& addr);
    void receive_from_clients();
    void handle_client_error(const client& c, int code) {
        receive_handler_(c.id, code, nullptr, 0, c.address);
    }
    void close_and_remove_client(int index);
    void do_close();

    int listen_socket_ = invalid_socket;
    int event_socket_ = invalid_socket;
    int last_error_ = 0;
    std::atomic<bool> running_{false};

    std::vector<pollfd> poll_array_;
    static const size_t listen_index = 0;
    static const size_t event_index = 1;
    static const size_t client_index = 2;

    accept_handler accept_handler_;
    receive_handler receive_handler_;

    std::vector<client> clients_;
    std::vector<size_t> stale_clients_;
    int32_t client_count_ = 0;
    static const int max_stale_clients = 100;
};

} // aoo
