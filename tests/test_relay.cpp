#include "aoo.h"

#include "aoo/src/binmsg.hpp"
#include "aoo/src/detail.hpp"
#include "common/net_utils.hpp"

#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>

#include "oscpack/osc/OscOutboundPacketStream.h"
#include "oscpack/osc/OscReceivedElements.h"

using namespace aoo;

AooByte inbuf[AOO_MAX_PACKET_SIZE];
AooInt32 insize = 0;

AooError AOO_CALL send_func(void *user, const AooByte *msg, AooInt32 size,
                            const void *, AooAddrSize, AooFlag)
{
    memcpy(inbuf, msg, size);
    insize = size;
    return kAooOk;
}

bool check_ping_message(const void *msg, int size,
                        AooId group, AooId user, AooNtpTime time) {
    try {
        osc::ReceivedPacket packet((const char *)msg, size);
        osc::ReceivedMessage msg(packet);

        if (strcmp(msg.AddressPattern(), kAooMsgDomain kAooMsgPeer kAooMsgPing)) {
            std::cout << "wrong address pattern" << std::endl;
            return false;
        }

        auto it = msg.ArgumentsBegin();
        if ((it++)->AsInt32() != group)
            return false;
        if ((it++)->AsInt32() != user)
            return false;
        if ((it++)->AsTimeTag() != time)
            return false;

        return true;
    } catch (const osc::Exception& e) {
        std::cout << "osc exception: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, char *argv[])
{
    socket::init();

    struct params {
        ip_address src;
        ip_address dst;
        AooSocketFlags flags;
        AooError expected;
    };

    std::array param_list = {
        params {
            ip_address("127.0.0.1", 9999),
            ip_address("127.0.0.1", 9998),
            kAooSocketIPv4, kAooOk
        },
        params {
            ip_address("127.0.0.1", 9999),
            ip_address("127.0.0.1", 9998),
            kAooSocketDualStack, kAooOk
        },
        params {
            ip_address("127.0.0.1", 9999),
            ip_address("127.0.0.1", 9998),
            kAooSocketIPv6, kAooErrorNotPermitted
        },
        params {
            ip_address("127.0.0.1", 9999),
            ip_address("127.0.0.1", 9998),
            kAooSocketIPv6 | kAooSocketIPv4, kAooOk
        },
        params {
            ip_address("::1", 9999),
            ip_address("::1", 9998),
            kAooSocketIPv4, kAooErrorNotPermitted
        },
        params {
            ip_address("::1", 9999),
            ip_address("::1", 9998),
            kAooSocketIPv6, kAooOk
        },
        params {
            ip_address("::1", 9999),
            ip_address("::1", 9998),
            kAooSocketIPv6 | kAooSocketIPv4, kAooOk
        }
    };

    // create test message (/aoo/peer/ping ...)
    AooId group = 5;
    AooId user = 7;
    AooNtpTime time = aoo_getCurrentNtpTime();

    AooByte msgbuf[AOO_MAX_PACKET_SIZE];
    osc::OutboundPacketStream msg((char *)msgbuf, sizeof(msgbuf));
    msg << osc::BeginMessage(kAooMsgDomain kAooMsgPeer kAooMsgPing)
        << group << user << osc::TimeTag(time)
        << osc::EndMessage;

    for (auto& p : param_list) {
        for (auto binary : { true, false }) {
            std::cout << "from " << p.src << " to " << p.dst;
            if (p.flags & kAooSocketIPv6) {
                if (p.flags & kAooSocketIPv4Mapped)
                    std::cout << " (IPv6 dual stack)";
                else
                    std::cout << " (IPv6 only)";
            } else {
                std::cout << " (IPv4)";
            }
            if (binary)
                std::cout << " (binary)";
            std::cout << std::endl;

            // 1) create relay message
            AooByte outbuf[AOO_MAX_PACKET_SIZE];
            auto outsize = net::write_relay_message(outbuf, sizeof(outbuf),
                                                    msgbuf, msg.Size(), p.dst, binary);
            if (outsize == 0) {
                std::cout << "write_relay_message() failed" << std::endl;
                return EXIT_FAILURE;
            }

            // 2) forward relay message
            auto result = aoo_handleRelayMessage(outbuf, outsize, p.src.address(), p.src.length(),
                                                 send_func, nullptr, p.flags);
            if (result != p.expected) {
                std::cout << "wrong result for aoo_handleRelayMessage(); "
                          << "expected: " << p.expected << ", result: " << result << std::endl;
                return EXIT_FAILURE;
            }

            if (result != kAooOk) {
                continue;
            }

            // 3) parse relay message
            AooMsgType type;
            AooId id;
            AooInt32 offset;

            if (auto err = aoo_parsePattern(inbuf, insize, &type, &id, &offset); err != kAooOk) {
                std::cout << "aoo_parsePattern() failed: " << aoo_strerror(err) << std::endl;
                return EXIT_FAILURE;
            }
            if (type != kAooMsgTypeRelay) {
                std::cout << "not a relay message" << std::endl;
                return EXIT_FAILURE;
            }

            // 4) check if embedded message matches the original
            if (binary) {
                ip_address addr;
                auto onset = binmsg_read_relay(inbuf, insize, addr);
                if (addr != p.src) {
                    std::cout << addr << " and " << p.src << " don't match" << std::endl;
                    return EXIT_FAILURE;
                }

                if (!check_ping_message(inbuf + onset, insize - onset, group, user, time)) {
                    return EXIT_FAILURE;
                }
            } else {
                try {
                    osc::ReceivedPacket packet((char *)inbuf, insize);
                    osc::ReceivedMessage msg(packet);
                    auto it = msg.ArgumentsBegin();

                    auto addr = osc_read_address(it);
                    if (addr != p.src) {
                        std::cout << addr << " and " << p.src << " don't match" << std::endl;
                        return EXIT_FAILURE;
                    }

                    const void *blob;
                    osc::osc_bundle_element_size_t blobsize;
                    (it++)->AsBlob(blob, blobsize);

                    if (!check_ping_message(blob, blobsize, group, user, time)) {
                        return EXIT_FAILURE;
                    }
                } catch (const osc::Exception& e) {
                    std::cout << "osc exception: " << e.what() << std::endl;
                    return EXIT_FAILURE;
                }
            }
        }
    }

    std::cout << "relay test succeeded!" << std::endl;

    return EXIT_SUCCESS;
}
