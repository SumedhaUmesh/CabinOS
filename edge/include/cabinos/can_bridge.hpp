#pragma once

#include <string>

#include "cabinos/vehicle_services.hpp"

namespace cabinos {

// Reads SocketCAN frames and updates BatteryService from known CAN IDs.
class CanBridge {
public:
    explicit CanBridge(BatteryService* battery) : battery_(battery) {}

    // Linux-only implementation reads vcan/can frames from interface.
    bool PollSocketCanOnce(const std::string& ifname, std::string* status_out) const;

    // Portable helper for tests or synthetic ingest.
    bool IngestSyntheticFrame(unsigned int can_id, unsigned char byte0, std::string* status_out) const;

private:
    BatteryService* battery_;
};

}  // namespace cabinos
