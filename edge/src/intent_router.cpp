#include "cabinos/intent_router.hpp"

namespace cabinos {
IntentRouter::IntentRouter(const std::string& policy_path) : policy_engine_(policy_path) {}

Tier IntentRouter::Classify(const std::string& utterance) const {
    return policy_engine_.Classify(utterance);
}

bool IntentRouter::RequiresCloud(const Tier tier, const bool cloud_online) const {
    return cloud_online && policy_engine_.CloudAllowed(tier);
}

}  // namespace cabinos
