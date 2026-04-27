#include "cabinos/intent_router.hpp"

#include <algorithm>

namespace cabinos {
namespace {

bool Contains(const std::string& text, const std::string& token) {
    return text.find(token) != std::string::npos;
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

}  // namespace

Tier IntentRouter::Classify(const std::string& utterance) const {
    const std::string text = ToLower(utterance);

    if (Contains(text, "hazard") || Contains(text, "defog") || Contains(text, "brake")) {
        return Tier::kSafetyCritical;
    }

    if (Contains(text, "cabin") || Contains(text, "temperature") || Contains(text, "light")) {
        return Tier::kComfort;
    }

    return Tier::kCognitive;
}

bool IntentRouter::RequiresCloud(const Tier tier, const bool cloud_online) const {
    if (tier == Tier::kSafetyCritical) {
        return false;
    }
    if (tier == Tier::kComfort) {
        return false;
    }
    return cloud_online;
}

}  // namespace cabinos
