#pragma once

#include <string>

#include "cabinos/intent_router.hpp"

namespace cabinos {

struct RouteResult {
    Tier tier;
    bool used_cloud;
    std::string message;
};

class ServiceBroker {
public:
    explicit ServiceBroker(IntentRouter router) : router_(router) {}

    RouteResult HandleTextCommand(const std::string& utterance, bool cloud_online) const;

private:
    IntentRouter router_;
};

}  // namespace cabinos
