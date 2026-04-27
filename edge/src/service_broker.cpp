#include "cabinos/service_broker.hpp"

#include <cctype>
#include <sstream>

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

std::string ToLower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool TryExtractFirstInt(const std::string& text, int* value_out) {
    std::stringstream ss(text);
    std::string token;
    while (ss >> token) {
        try {
            size_t processed = 0;
            const int value = std::stoi(token, &processed);
            if (processed > 0) {
                *value_out = value;
                return true;
            }
        } catch (...) {
            continue;
        }
    }
    return false;
}

}  // namespace

ServiceBroker::ServiceBroker(IntentRouter router)
    : router_(router),
      hvac_(),
      lighting_(),
      battery_(),
      api_server_(&hvac_, &lighting_, &battery_),
      api_client_(&api_server_),
      cloud_bridge_(CloudBridgeClient::FromEnv()) {}

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

    if (!use_cloud) {
        return RouteResult{
            tier,
            false,
            HandleLocalAction(utterance, tier),
        };
    }

    if (tier == Tier::kCognitive) {
        const auto cloud = cloud_bridge_.InvokeCognitive(utterance);
        return RouteResult{
            tier,
            cloud.used_cloud,
            cloud.ok ? cloud.message : ("Cloud bridge failed: " + cloud.message),
        };
    }

    return RouteResult{
        tier,
        true,
        "Routed command as " + TierName(tier) + " (cloud).",
    };
}

RuntimeSnapshot ServiceBroker::Snapshot() const {
    return RuntimeSnapshot{
        hvac_.CabinTemperatureC(),
        lighting_.CabinLightsLevel(),
        lighting_.HazardsOn(),
        battery_.ReadSnapshot().soc_percent,
    };
}

void ServiceBroker::Restore(const RuntimeSnapshot& snapshot) {
    (void)hvac_.SetCabinTemperatureC(snapshot.cabin_temperature_c);
    lighting_.SetCabinLightsLevel(snapshot.cabin_lights_level);
    lighting_.SetHazards(snapshot.hazards_on);
    (void)battery_.SetSocPercent(snapshot.battery_soc_percent);
}

std::string ServiceBroker::HandleLocalAction(const std::string& utterance, const Tier tier) const {
    const std::string text = ToLower(utterance);

    if (tier == Tier::kSafetyCritical) {
        if (text.find("hazard") != std::string::npos) {
            const bool enable = (text.find("off") == std::string::npos);
            return api_client_.SetHazards(enable).message;
        }
        if (text.find("defog") != std::string::npos) {
            return "Rear defogger enabled.";
        }
        return "Safety-critical command accepted locally.";
    }

    if (tier == Tier::kComfort) {
        if (text.find("battery") != std::string::npos || text.find("soc") != std::string::npos ||
            text.find("charge") != std::string::npos) {
            return api_client_.GetBatteryStatus().message;
        }
        if (text.find("temp") != std::string::npos || text.find("temperature") != std::string::npos) {
            int temp_c = 0;
            if (!TryExtractFirstInt(text, &temp_c)) {
                return "Please include a target temperature in Celsius.";
            }
            return api_client_.SetTemperature(temp_c).message;
        }
        if (text.find("light") != std::string::npos || text.find("dim") != std::string::npos) {
            int level = 30;
            (void)TryExtractFirstInt(text, &level);
            return api_client_.SetCabinLights(level).message;
        }
        return "Comfort command accepted locally.";
    }

    return "Cognitive request requires cloud processing.";
}

}  // namespace cabinos
