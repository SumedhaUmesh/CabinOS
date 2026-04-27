#pragma once

#include <string>

namespace cabinos {

enum class Tier {
    kSafetyCritical,
    kComfort,
    kCognitive,
};

class IntentRouter {
public:
    Tier Classify(const std::string& utterance) const;
    bool RequiresCloud(Tier tier, bool cloud_online) const;
};

}  // namespace cabinos
