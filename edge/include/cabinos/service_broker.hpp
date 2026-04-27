#pragma once

#include <string>

#include "cabinos/intent_router.hpp"
#include "cabinos/vehicle_api.hpp"
#include "cabinos/vehicle_services.hpp"

namespace cabinos {

struct RouteResult {
    Tier tier;
    bool used_cloud;
    std::string message;
};

class ServiceBroker {
public:
    explicit ServiceBroker(IntentRouter router);

    RouteResult HandleTextCommand(const std::string& utterance, bool cloud_online) const;

private:
    std::string HandleLocalAction(const std::string& utterance, Tier tier) const;

    IntentRouter router_;
    mutable HVACService hvac_;
    mutable LightingService lighting_;
    mutable BatteryService battery_;
    mutable VehicleApiServer api_server_;
    mutable VehicleApiClient api_client_;
};

}  // namespace cabinos
