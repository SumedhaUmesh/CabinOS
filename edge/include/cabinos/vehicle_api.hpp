#pragma once

#include <string>

#include "cabinos/vehicle_services.hpp"

namespace cabinos {

struct SetTemperatureRequest {
    int temp_c;
};

struct SetTemperatureResponse {
    bool ok;
    std::string message;
    int applied_temp_c;
};

struct SetHazardsRequest {
    bool on;
};

struct SetHazardsResponse {
    bool ok;
    std::string message;
};

struct SetCabinLightsRequest {
    int level_percent;
};

struct SetCabinLightsResponse {
    bool ok;
    std::string message;
    int applied_level_percent;
};

struct GetBatteryStatusResponse {
    bool ok;
    int soc_percent;
    std::string message;
};

// Mirrors an RPC server implementation boundary.
class VehicleApiServer {
public:
    VehicleApiServer(HVACService* hvac, LightingService* lighting, BatteryService* battery)
        : hvac_(hvac), lighting_(lighting), battery_(battery) {}

    SetTemperatureResponse SetTemperature(const SetTemperatureRequest& req) const;
    SetHazardsResponse SetHazards(const SetHazardsRequest& req) const;
    SetCabinLightsResponse SetCabinLights(const SetCabinLightsRequest& req) const;
    GetBatteryStatusResponse GetBatteryStatus() const;

private:
    HVACService* hvac_;
    LightingService* lighting_;
    BatteryService* battery_;
};

// Mirrors an RPC client boundary (in-process transport for now).
class VehicleApiClient {
public:
    explicit VehicleApiClient(const VehicleApiServer* server) : server_(server) {}

    SetTemperatureResponse SetTemperature(int temp_c) const;
    SetHazardsResponse SetHazards(bool on) const;
    SetCabinLightsResponse SetCabinLights(int level_percent) const;
    GetBatteryStatusResponse GetBatteryStatus() const;

private:
    const VehicleApiServer* server_;
};

}  // namespace cabinos
