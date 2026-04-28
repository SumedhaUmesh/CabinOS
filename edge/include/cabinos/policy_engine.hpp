#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "cabinos/tier.hpp"

namespace cabinos {

struct TierPolicy {
    bool cloud_allowed = false;
    std::vector<std::string> tools;
};

class PolicyEngine {
public:
    explicit PolicyEngine(const std::string& policy_path = "config/policy.yaml");

    Tier Classify(const std::string& utterance) const;
    bool CloudAllowed(Tier tier) const;
    const std::vector<std::string>& AllowedTools(Tier tier) const;

private:
    void LoadDefaults();
    void TryLoadPolicyFile(const std::string& policy_path);

    std::unordered_map<Tier, TierPolicy> tier_policies_;
    std::unordered_map<Tier, std::vector<std::string>> tier_keywords_;
};

}  // namespace cabinos
