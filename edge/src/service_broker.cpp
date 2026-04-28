#include "cabinos/service_broker.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <sstream>

#if defined(CABINOS_HAS_OPENSSL)
#include <openssl/hmac.h>
#endif

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

long long NowEpochMs() {
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count());
}

#if defined(CABINOS_HAS_OPENSSL)
std::string GetEnvOrDefault(const char* key, const std::string_view fallback) {
    const char* value = std::getenv(key);
    if (value == nullptr || std::string(value).empty()) {
        return std::string(fallback);
    }
    return std::string(value);
}

std::string HexEncode(const unsigned char* data, const size_t len) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        out << std::setw(2) << static_cast<int>(data[i]);
    }
    return out.str();
}

std::string HmacSha256Hex(const std::string& key, const std::string& message) {
    unsigned int out_len = 0;
    unsigned char out[EVP_MAX_MD_SIZE] = {0};
    if (HMAC(EVP_sha256(),
             key.data(),
             static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char*>(message.data()),
             message.size(),
             out,
             &out_len) == nullptr) {
        return "";
    }
    return HexEncode(out, out_len);
}
#endif

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
        if (cloud.ok && cloud.has_proposal) {
            const std::string proposal_result = ValidateAndApplyCloudProposal(cloud);
            return RouteResult{
                tier,
                cloud.used_cloud,
                cloud.message + " [proposal] " + proposal_result,
            };
        }
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

std::string ServiceBroker::ValidateAndApplyCloudProposal(const CloudInvokeResult& proposal) const {
    constexpr long long kMaxSkewMs = 120000;
    const long long now_ms = NowEpochMs();
    if (proposal.proposal_nonce.empty()) {
        return "rejected_nonce_missing";
    }
    if (used_nonces_.find(proposal.proposal_nonce) != used_nonces_.end()) {
        return "rejected_replay_nonce";
    }
    if (proposal.proposal_timestamp_ms <= 0 ||
        std::llabs(now_ms - proposal.proposal_timestamp_ms) > kMaxSkewMs) {
        return "rejected_stale_timestamp";
    }

#if defined(CABINOS_HAS_OPENSSL)
    const std::string secret = GetEnvOrDefault("CABINOS_PROPOSAL_SECRET", "cabinos-dev-secret");
    const std::string canonical = proposal.proposal_action + "|" + std::to_string(proposal.proposal_value) +
                                  "|" + std::to_string(proposal.proposal_timestamp_ms) + "|" +
                                  proposal.proposal_nonce;
    const std::string expected = HmacSha256Hex(secret, canonical);
    if (expected.empty() || expected != proposal.proposal_signature) {
        return "rejected_invalid_signature";
    }
#else
    (void)proposal;
    return "rejected_signature_unavailable";
#endif

    if (proposal.proposal_action == "set_temperature_c") {
        if (proposal.proposal_value < 16 || proposal.proposal_value > 30) {
            return "rejected_out_of_bounds_temperature";
        }
        const auto r = api_client_.SetTemperature(proposal.proposal_value);
        if (!r.ok) {
            return "rejected_apply_failed_temperature";
        }
    } else if (proposal.proposal_action == "set_cabin_lights_percent") {
        if (proposal.proposal_value < 0 || proposal.proposal_value > 100) {
            return "rejected_out_of_bounds_lights";
        }
        const auto r = api_client_.SetCabinLights(proposal.proposal_value);
        if (!r.ok) {
            return "rejected_apply_failed_lights";
        }
    } else if (proposal.proposal_action == "set_hazards") {
        if (proposal.proposal_value != 0 && proposal.proposal_value != 1) {
            return "rejected_out_of_bounds_hazards";
        }
        const auto r = api_client_.SetHazards(proposal.proposal_value == 1);
        if (!r.ok) {
            return "rejected_apply_failed_hazards";
        }
    } else {
        return "rejected_action_not_allowed";
    }

    used_nonces_.insert(proposal.proposal_nonce);
    return "accepted_applied";
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
