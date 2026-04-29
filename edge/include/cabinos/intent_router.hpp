#pragma once

#include <string>

#include "cabinos/policy_engine.hpp"
#include "cabinos/tier.hpp"

namespace cabinos {

class IntentRouter {
public:
    explicit IntentRouter(const std::string& policy_path = "config/policy.yaml");

    Tier Classify(const std::string& utterance) const;
    bool RequiresCloud(Tier tier, bool cloud_online) const;
    bool ToolAllowed(Tier tier, const std::string& tool_name) const;

private:
    PolicyEngine policy_engine_;
};

}  // namespace cabinos
