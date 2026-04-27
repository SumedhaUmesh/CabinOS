#include "cabinos/service_broker.hpp"

namespace cabinos {
namespace {

std::string TierName(const Tier tier) {
    switch (tier) {
    case Tier::kSafetyCritical:
        return "safety_critical";
    case Tier::kComfort:
        return "comfort";
    case Tier::kCognitive:
        return "cognitive";
    }
    return "unknown";
}

}  // namespace

RouteResult ServiceBroker::HandleTextCommand(const std::string& utterance, const bool cloud_online) const {
    const Tier tier = router_.Classify(utterance);
    const bool use_cloud = router_.RequiresCloud(tier, cloud_online);

    if (tier == Tier::kCognitive && !cloud_online) {
        return RouteResult{
            tier,
            false,
            "Cloud unavailable. Returning graceful offline fallback for cognitive request.",
        };
    }

    return RouteResult{
        tier,
        use_cloud,
        "Routed command as " + TierName(tier) + (use_cloud ? " (cloud)." : " (edge)."),
    };
}

}  // namespace cabinos
