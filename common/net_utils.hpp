#pragma once

#include "aoo/aoo_config.h"
#include "aoo/aoo_defines.h"
#include "aoo/aoo_types.h"

#include <cassert>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

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

//-------------- error codes -------------//

enum network_error {
#ifdef _WIN32
    timeout = WSAETIMEDOUT,
    abort = WSAECONNABORTED
#else
    timeout = ETIMEDOUT,
    abort = ECONNABORTED
#endif
};

//-------------- ip_address --------------//

class ip_address {
public:
    enum ip_type {
        Unspec,
        IPv4,
        IPv6
    };

    static const socklen_t max_length = 32;

    static std::vector<ip_address> resolve(const std::string& host, uint16_t port,
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

    ip_address(uint16_t port, ip_type type); // "any" address

    // TODO: make realtime safe!
    ip_address(const std::string& ip, uint16_t port, ip_type type = ip_type::Unspec);

    ip_address(const AooByte *bytes, AooSize size, uint16_t port, ip_type type);

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

    uint16_t port() const;

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

int socket_init();

int socket_udp(uint16_t port, bool reuse_port = false);

int socket_tcp(uint16_t port, bool reuse_port = false);

enum shutdown_method {
    shutdown_receive = 0,
    shutdown_send = 1,
    shutdown_both = 2
};

int socket_shutdown(int socket, shutdown_method how);

int socket_close(int socket);

int socket_connect(int socket, const ip_address& addr, double timeout);

int socket_address(int socket, ip_address& addr);

int socket_peer(int socket, ip_address& addr);

int socket_port(int socket);

ip_address::ip_type socket_family(int socket);

int socket_sendto(int socket, const void *buf, int size,
                  const ip_address& address);

int socket_receive(int socket, void *buf, int size,
                   ip_address* addr, double timeout);

int socket_set_sendbufsize(int socket, int bufsize);

int socket_set_recvbufsize(int socket, int bufsize);

int socket_set_int_option(int socket, int level, int opt, int value);

int socket_get_int_option(int socket, int level, int opt, int* value);

int socket_set_nonblocking(int socket, bool nonblocking);

bool socket_signal(int socket);

int socket_errno();

void socket_set_errno(int err);

int socket_error(int socket);

int socket_strerror(int err, char *buf, int size);

std::string socket_strerror(int err);

void socket_error_print(const char *label = nullptr);

AooSocketFlags socket_get_flags(int socket);

} // aoo
