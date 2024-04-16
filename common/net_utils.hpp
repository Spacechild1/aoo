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
#if AOO_USE_IPV6
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

//-------------------- socket --------------------//

class socket_error;

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

    socket_error() = default;

    socket_error(int err)
        : err_(err), msg_(socket::strerror(err)) {}

    socket_error(int err, std::string msg)
        : err_(err), msg_(std::move(msg)) {}

    const char* what() const noexcept override {
        return msg_.c_str();
    }

    int code() const {
        return err_;
    }
private:
    int err_ = 0;
    std::string msg_;
};

//-------------- ip_address --------------//

using port_type = uint16_t;

class resolve_error : public socket_error {
public:
    resolve_error(int err, std::string msg)
        : socket_error(err, std::move(msg)) {}
};

class ip_address {
public:
    enum ip_type {
        Unspec,
        IPv4,
        IPv6
    };

    static const socklen_t max_length = 32;

    // throws resolve_error on failure!
    static std::vector<ip_address> resolve(std::string_view host, port_type port,
                                           ip_type type, bool ipv4mapped = false);

    ip_address();

    // NB: for convenience 'sa' may be NULL!
    ip_address(const struct sockaddr *sa, socklen_t len) {
        if (sa) {
            assert(len > 0 && len <= max_length);
            memcpy(data_, sa, len);
            // keep clang-analyzer happy...
            addr_.sa_family = sa->sa_family;
            length_ = len;
        } else {
            assert(len == 0);
            clear();
        }
    }

    ip_address(const AooSockAddr &addr)
        : ip_address((const struct sockaddr *)addr.data, addr.size) {}

    ip_address(port_type port, ip_type type); // "any" address

    ip_address(std::string_view ip, port_type port, ip_type type = ip_type::Unspec,
               bool ipv4mapped = true);

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

    void reserve();

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
#if AOO_USE_IPV6
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

//------------------------ base_socket --------------------//

enum shutdown_method {
    shutdown_receive = 0,
    shutdown_send = 1,
    shutdown_both = 2
};

struct socket_tag {};
struct port_tag {};
struct family_tag {};

class base_socket {
public:
    ip_address address() const;

    port_type port() const;

    ip_address::ip_type family() const;

    AooSocketFlags flags() const;

    socket_type native_handle() const {
        return socket_;
    }

    void close() noexcept;

    bool is_open() const {
        return socket_ != invalid_socket;
    }

    bool shutdown(shutdown_method method) noexcept;

    void connect(const ip_address& addr, double timeout = -1);

    ip_address peer() const;

    void bind(const ip_address& addr);

    int send(const void *buf, int size);

    int receive(void *buf, int size) {
        return do_receive(buf, size, nullptr, -1).second;
    }

    int receive(void *buf, int size, ip_address& address) {
        return do_receive(buf, size, &address, -1).second;
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

#ifndef _WIN32
    bool non_blocking() const;
#endif

    void set_reuse_port(bool b);

    bool reuse_port() const;

    int error() const;
protected:
    base_socket(socket_type sock = invalid_socket)
        : socket_(sock) {}

    ~base_socket() { close(); }

    base_socket(const base_socket&) = delete;

    base_socket& operator=(const base_socket& other) = delete;

    base_socket(base_socket&& other) noexcept
        : socket_(std::exchange(other.socket_, invalid_socket)) {}

    base_socket& operator=(base_socket&& other) noexcept {
        socket_ = std::exchange(other.socket_, invalid_socket);
        return *this;
    }

    std::pair<bool, int> do_receive(void *buf, int size, ip_address* addr, double timeout);

    socket_type socket_;
};

//-------------------------- udp_socket ------------------------//

class udp_socket : public base_socket {
public:
    udp_socket() = default;

    udp_socket(socket_tag, socket_type sock)
        : base_socket(sock) {}

    udp_socket(family_tag, ip_address::ip_type family, bool dualstack = true);

    udp_socket(port_tag, port_type port, bool reuse_port = false);

    explicit udp_socket(const ip_address& addr, bool reuse_port = false);

    udp_socket(udp_socket&& other)
        : base_socket(std::forward<base_socket>(other)) {}

    udp_socket& operator=(udp_socket&& other) {
        base_socket::operator=(std::forward<base_socket>(other));
        return *this;
    }

    int send(const void *buf, int size, const ip_address& address);

    bool signal() noexcept;
};

//-------------------------- tcp_socket ------------------------//

class accept_error : public socket_error {
public:
    accept_error(int err, const ip_address& addr)
        : socket_error(err), addr_(addr) {}

    const ip_address& address() const {
        return addr_;
    }
private:
    ip_address addr_;
};

class tcp_socket : public base_socket {
public:
    using from_port = port_tag;

    tcp_socket() = default;

    tcp_socket(socket_tag, socket_type sock)
        : base_socket(sock) {}

    tcp_socket(family_tag, ip_address::ip_type family, bool dualstack = true);

    tcp_socket(port_tag, port_type port, bool reuse_port = false);

    explicit tcp_socket(const ip_address& addr, bool reuse_port = false);

    tcp_socket(tcp_socket&& other)
        : base_socket(std::forward<base_socket>(other)) {}

    tcp_socket& operator=(tcp_socket&& other) {
        base_socket::operator=(std::forward<base_socket>(other));
        return *this;
    }

    void listen(int backlog = SOMAXCONN);

    void set_nodelay(bool b);

    bool nodelay() const;

    // throws accept error
    std::pair<tcp_socket, ip_address> accept();
};

} // aoo
