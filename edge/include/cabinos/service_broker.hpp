#pragma once

#include <unordered_set>
#include <string>

#include "cabinos/actuation_guard.hpp"
#include "cabinos/cloud_bridge.hpp"
#include "cabinos/intent_router.hpp"
#include "cabinos/vehicle_api.hpp"
#include "cabinos/vehicle_services.hpp"

namespace cabinos {

struct RouteResult {
    Tier tier;
    bool used_cloud;
    std::string message;
};

struct RuntimeSnapshot {
    int cabin_temperature_c;
    int cabin_lights_level;
    bool hazards_on;
    int battery_soc_percent;
};

class ServiceBroker {
public:
    explicit ServiceBroker(IntentRouter router);

    RouteResult HandleTextCommand(const std::string& utterance, bool cloud_online) const;
    RuntimeSnapshot Snapshot() const;
    void Restore(const RuntimeSnapshot& snapshot);

private:
    std::string HandleLocalAction(const std::string& utterance, Tier tier) const;

    IntentRouter router_;
    mutable HVACService hvac_;
    mutable LightingService lighting_;
    mutable BatteryService battery_;
    mutable VehicleApiServer api_server_;
    mutable VehicleApiClient api_client_;
    mutable CloudBridgeClient cloud_bridge_;
    mutable ActuationGuard actuation_guard_;
    mutable std::unordered_set<std::string> used_nonces_;
};

}  // namespace cabinos
