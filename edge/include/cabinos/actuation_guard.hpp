#pragma once

#include <string>
#include <unordered_set>

#include "cabinos/cloud_bridge.hpp"
#include "cabinos/vehicle_api.hpp"

namespace cabinos {

class ActuationGuard {
public:
    std::string ValidateAndApply(const CloudInvokeResult& proposal,
                                 VehicleApiClient* api_client,
                                 std::unordered_set<std::string>* used_nonces) const;
};

}  // namespace cabinos
