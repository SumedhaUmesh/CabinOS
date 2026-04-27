#include "cabinos/can_bridge.hpp"

#include <string>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

namespace cabinos {

bool CanBridge::IngestSyntheticFrame(const unsigned int can_id,
                                     const unsigned char byte0,
                                     std::string* status_out) const {
    if (battery_->UpdateFromCanPayload(can_id, byte0)) {
        *status_out = "Battery SoC updated from synthetic CAN frame to " +
                      std::to_string(battery_->ReadSnapshot().soc_percent) + "%.";
        return true;
    }
    *status_out = "Synthetic frame ignored (unknown CAN ID).";
    return false;
}

bool CanBridge::PollSocketCanOnce(const std::string& ifname, std::string* status_out) const {
#if defined(__linux__)
    const int sock = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        *status_out = "Socket creation failed: " + std::string(std::strerror(errno));
        return false;
    }

    struct ifreq ifr {};
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname.c_str());
    if (::ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        *status_out = "ioctl(SIOCGIFINDEX) failed for " + ifname + ": " + std::strerror(errno);
        ::close(sock);
        return false;
    }

    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        *status_out = "bind() failed for " + ifname + ": " + std::strerror(errno);
        ::close(sock);
        return false;
    }

    struct can_frame frame {};
    const ssize_t bytes = ::read(sock, &frame, sizeof(frame));
    ::close(sock);

    if (bytes < 0) {
        *status_out = "read() failed: " + std::string(std::strerror(errno));
        return false;
    }
    if (bytes < static_cast<ssize_t>(sizeof(frame))) {
        *status_out = "read() returned partial CAN frame.";
        return false;
    }

    const bool updated = battery_->UpdateFromCanPayload(frame.can_id & CAN_EFF_MASK, frame.data[0]);
    if (updated) {
        *status_out = "Battery SoC updated from " + ifname + " to " +
                      std::to_string(battery_->ReadSnapshot().soc_percent) + "%.";
    } else {
        *status_out = "Frame received on " + ifname + " but ignored by mapping.";
    }
    return updated;
#else
    (void)ifname;
    *status_out = "SocketCAN polling is only available on Linux.";
    return false;
#endif
}

}  // namespace cabinos
