#include "udp_server.hpp"

#include "common/utils.hpp"

namespace aoo {

void udp_server::start(int port, receive_handler receive, bool threaded) {
    do_close();

    receive_handler_ = std::move(receive);

    auto sock = socket_udp(port);
    if (sock < 0) {
        auto e = socket_errno();
        throw udp_error(e, socket_strerror(e));
    }

    if (socket_address(sock, bind_addr_) < 0) {
        auto e = socket_errno(); // cache error
        socket_close(sock);
        throw udp_error(e, socket_strerror(e));
    }

    if (send_buffer_size_ > 0) {
        if (aoo::socket_set_sendbufsize(sock, send_buffer_size_) < 0){
            aoo::socket_error_print("setsendbufsize");
        }
    }

    if (receive_buffer_size_ > 0) {
        if (aoo::socket_set_recvbufsize(sock, receive_buffer_size_) < 0){
            aoo::socket_error_print("setrecvbufsize");
        }
    }

    socket_ = sock;
    running_.store(true);
    threaded_ = threaded;
    if (threaded_) {
        packet_queue_.clear();
        // TODO: lower thread priority?
        thread_ = std::thread([this](){
            try {
                this->receive(-1.0);
            } catch (const udp_error& e) {
                LOG_DEBUG("udp_server: thread function failed: " << e.what());
                // TODO: report error to main thread
            }
            running_.store(false);
        });
    }
}

bool udp_server::run(double timeout) {
    if (timeout >= 0) {
        if (threaded_) {
            // TODO: implement timed wait for semaphore
            return packet_queue_.try_consume([this](auto& packet){
                receive_handler_(packet.data.data(), packet.data.size(), packet.address);
            });
        } else {
            return receive(timeout);
        }
    } else {
        if (threaded_) {
            while (running_.load()) {
                packet_queue_.consume_all([&](auto& packet){
                    receive_handler_(packet.data.data(), packet.data.size(), packet.address);
                });
                // wait for packets
                event_.wait();
            }
        } else {
            while (running_.load()) {
                receive(-1.0);
            }
        }

        do_close();

        return true;
    }
}

void udp_server::stop() {
    bool running = running_.exchange(false);
    if (running) {
        // wake up receive
        if (!socket_signal(socket_)) {
            // force wakeup by closing the socket.
            // this is not nice and probably undefined behavior,
            // the MSDN docs explicitly forbid it!
            socket_close(socket_);
        }
        if (threaded_) {
            // wake up main thread
            event_.set();
            // join receive thread
            if (thread_.joinable()) {
                thread_.join();
            }
        }
    }
}

void udp_server::notify() {
    if (threaded_) {
        event_.set(); // wake up main thread
    } else {
        socket_signal(socket_);
    }
}

void udp_server::do_close() {
    if (socket_ != invalid_socket) {
        socket_close(socket_);
    }
    socket_ = invalid_socket;
    bind_addr_.clear();
    if (thread_.joinable()) {
        thread_.join();
    }
}

udp_server::~udp_server() {
    stop();
    do_close();
}

int udp_server::send(const aoo::ip_address& addr, const AooByte *data, AooSize size) {
    return ::sendto(socket_, (const char *)data, size, 0, addr.address(), addr.length());
}

bool udp_server::receive(double timeout) {
    aoo::ip_address address;
    auto result = socket_receive(socket_, buffer_.data(), buffer_.size(),
                                 &address, timeout);
    if (result > 0) {
        if (threaded_) {
            packet_queue_.produce([&](auto& packet){
                packet.data.assign(buffer_.data(), buffer_.data() + result);
                packet.address = address;
            });
            event_.set(); // notify main thread (if blocking)
        } else {
            receive_handler_(buffer_.data(), result, address);
        }
        return true;
    } else if (result < 0) {
        int e = socket_errno();
    #ifdef _WIN32
        // ignore ICMP Port Unreachable message!
        if (e == WSAECONNRESET) {
            return true; // continue
        } else if (e == WSAEWOULDBLOCK) {
            return false; // timeout
        }
    #else
        if (e == EINTR){
            return true; // continue
        } else if (e == EWOULDBLOCK) {
            return false; // timeout
        }
    #endif
        // notify main thread (if blocking)
        if (threaded_) {
            running_.store(false);
            event_.set();
        }

        throw udp_error(e, socket_strerror(e));
    } else {
        // ignore timeout or empty packet (used for signalling)
        return true;
    }
}

} // aoo
