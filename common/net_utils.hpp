#pragma once

#include "aoo/aoo_config.h"
#include "aoo/aoo_defines.h"
#include "aoo/aoo_types.h"

#include <cassert>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#if AOO_USE_IPv6
#include <ws2ipdef.h>
#endif
typedef int socklen_t;
struct sockaddr;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#endif // _WIN32

namespace aoo {

//-------------- ip_address --------------//

using port_type = uint16_t;

class ip_address {
public:
    enum ip_type {
        Unspec,
        IPv4,
        IPv6
    };

    static const socklen_t max_length = 32;

    static std::vector<ip_address> resolve(const std::string& host, port_type port,
                                           ip_type type, bool ipv4mapped = false);

    ip_address();

    ip_address(socklen_t size);

    // NB: for convenience 'sa' may be NULL!
    ip_address(const struct sockaddr *sa, socklen_t len) {
        if (sa) {
            assert(len > 0 && len <= max_length);
            memcpy(data_, sa, len);
            length_ = len;
        } else {
            assert(len == 0);
            clear();
        }
    }

    ip_address(const AooSockAddr &addr)
        : ip_address((const struct sockaddr *)addr.data, addr.size) {}

    ip_address(port_type port, ip_type type); // "any" address

    // TODO: make realtime safe!
    ip_address(const std::string& ip, port_type port, ip_type type = ip_type::Unspec);

    ip_address(const AooByte *bytes, AooSize size, port_type port, ip_type type);

    ip_address(const ip_address& other){
        if (other.length_ > 0) {
            memcpy(data_, &other.data_, other.length_);
            length_ = other.length_;
        } else {
            clear();
        }
    }

    ip_address& operator=(const ip_address& other){
        if (other.length_ > 0) {
            memcpy(data_, other.data_, other.length_);
            length_ = other.length_;
        } else {
            clear();
        }
        return *this;
    }

    void clear();

    void resize(socklen_t size);

    bool operator==(const ip_address& other) const;

    bool operator !=(const ip_address& other) const {
        return !(*this == other);
    }

    size_t hash() const;

    const char* name() const;

    const char* name_unmapped() const;

    port_type port() const;

    const AooByte* address_bytes() const;

    size_t address_size() const;

    bool valid() const;

    ip_type type() const;

    bool is_ipv4_mapped() const;

    ip_address ipv4_mapped() const;

    ip_address unmapped() const;

    void unmap();

    const struct sockaddr *address() const {
        return (const struct sockaddr *)data_;
    }

    struct sockaddr *address_ptr() {
        return (struct sockaddr *)data_;
    }

    socklen_t length() const {
        return length_;
    }

    socklen_t *length_ptr() {
        return &length_;
    }

    friend std::ostream& operator<<(std::ostream& os, const ip_address& addr);

private:
    static const char *get_name(const struct sockaddr *addr);
    // large enough to hold both sockaddr_in
    // and sockaddr_in6 (max. 32 bytes)
    union {
        sockaddr_in addr_in_;
#if AOO_USE_IPv6
        sockaddr_in6 addr_in6_;
#endif
        // NB: on some systems (macOS, BSD, ESP32) 'sockaddr' starts with a
        // 'sa_len` field and 'sa_family' is only uint8_t! Therefore we must
        // always access the family through 'sockaddr'.
        sockaddr addr_;
        uint64_t align_;
        AooByte data_[max_length];
    };
    socklen_t length_;

    void check();
};

//-------------------- socket --------------------//

namespace socket {

int init();

int get_last_error();

void set_last_error(int err);

int strerror(int err, char *buf, int size);

std::string strerror(int err);

void print_error(int err, const char *label = nullptr);

void print_last_error(const char *label = nullptr);

} // socket

#ifdef _WIN32
using socket_type = SOCKET;
constexpr socket_type invalid_socket = INVALID_SOCKET;
#else
using socket_type = int;
constexpr socket_type invalid_socket = -1;
#endif

//-------------- socket_error --------------//

class socket_error : public std::exception {
public:
    enum socket_error_code {
#ifdef _WIN32
        timeout = WSAETIMEDOUT,
        abort = WSAECONNABORTED,
        would_block = WSAEWOULDBLOCK
#else
        timeout = ETIMEDOUT,
        abort = ECONNABORTED,
        would_block = EWOULDBLOCK
#endif
    };

    socket_error(int err)
        : err_(err), msg_(socket::strerror(err)) {}

    socket_error(int err, std::string_view msg)
        : err_(err), msg_(msg) {}

    const char* what() const noexcept override {
        return msg_.c_str();
    }

    int error() const {
        return err_;
    }
private:
    std::string msg_;
    int err_;
};

enum shutdown_method {
    shutdown_receive = 0,
    shutdown_send = 1,
    shutdown_both = 2
};

class base_socket {
public:
    ip_address address() const;

    port_type port() const;

    ip_address::ip_type family() const;

    AooSocketFlags flags() const;

    bool close() noexcept;

    bool shutdown(shutdown_method method) noexcept;

    void connect(const ip_address& addr, double timeout = -1);

    ip_address peer() const;

    void bind(port_type port);

    void bind(const ip_address& addr);

    int send(const void *buf, int size);

    int receive(void *buf, int size) {
        return do_receive(buf, size, nullptr, -1).first;
    }

    int receive(void *buf, int size, ip_address& address) {
        return do_receive(buf, size, &address, -1).first;
    }

    std::pair<bool, int> receive(void *buf, int size, double timeout) {
        return do_receive(buf, size, nullptr, timeout);
    }

    std::pair<bool, int> receive(void *buf, int size, ip_address& address, double timeout) {
        return do_receive(buf, size, &address, timeout);
    }

    void set_send_buffer_size(int bufsize);

    int send_buffer_size() const;

    void set_receive_buffer_size(int bufsize);

    int receive_buffer_size() const;

    void set_non_blocking(bool b);

    bool non_blocking() const;

    void set_reuse_port(bool b);

    bool reuse_port() const;

    int error() const;

    socket_type native_handle() const;
protected:
    base_socket() {}

    ~base_socket() { close(); }

    base_socket(base_socket&& other) noexcept
        : socket_(std::exchange(other.socket_, invalid_socket)) {}

    base_socket& operator=(base_socket&& other) noexcept {
        socket_ = std::exchange(other.socket_, invalid_socket);
        return *this;
    }

    std::pair<bool, int> do_receive(void *buf, int size, ip_address* addr, double timeout);

    socket_type socket_ = invalid_socket;
};

class udp_socket : public base_socket {
public:
    udp_socket() = default;

    udp_socket(ip_address::ip_type family);

    udp_socket(port_type port, bool reuse_port = false);

    udp_socket(const ip_address& addr, bool reuse_port = false);

    udp_socket(udp_socket&& other)
        : base_socket(std::forward<base_socket>(other)) {}

    udp_socket& operator=(udp_socket&& other) {
        base_socket::operator=(std::forward<base_socket>(other));
        return *this;
    }

    int send(const void *buf, int size, const ip_address& address);

    bool signal() noexcept;
};

class tcp_socket : public base_socket {
public:
    tcp_socket() = default;

    tcp_socket(ip_address::ip_type family);

    tcp_socket(port_type port, bool reuse_port = false);

    tcp_socket(const ip_address& addr, bool reuse_port = false);

    tcp_socket(tcp_socket&& other)
        : base_socket(std::forward<base_socket>(other)) {}

    tcp_socket& operator=(tcp_socket&& other) {
        base_socket::operator=(std::forward<base_socket>(other));
        return *this;
    }

    void listen(int backlog);

    ip_address accept();
};

} // aoo
