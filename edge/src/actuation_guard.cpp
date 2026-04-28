#include "cabinos/actuation_guard.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string_view>

#if defined(CABINOS_HAS_OPENSSL)
#include <openssl/hmac.h>
#endif

namespace cabinos {
namespace {

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

std::string ActuationGuard::ValidateAndApply(const CloudInvokeResult& proposal,
                                             VehicleApiClient* api_client,
                                             std::unordered_set<std::string>* used_nonces) const {
    constexpr long long kMaxSkewMs = 120000;
    const long long now_ms = NowEpochMs();

    if (proposal.proposal_nonce.empty()) {
        return "rejected_nonce_missing";
    }
    if (used_nonces->find(proposal.proposal_nonce) != used_nonces->end()) {
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
    (void)api_client;
    (void)used_nonces;
    return "rejected_signature_unavailable";
#endif

    if (proposal.proposal_action == "set_temperature_c") {
        if (proposal.proposal_value < 16 || proposal.proposal_value > 30) {
            return "rejected_out_of_bounds_temperature";
        }
        const auto r = api_client->SetTemperature(proposal.proposal_value);
        if (!r.ok) {
            return "rejected_apply_failed_temperature";
        }
    } else if (proposal.proposal_action == "set_cabin_lights_percent") {
        if (proposal.proposal_value < 0 || proposal.proposal_value > 100) {
            return "rejected_out_of_bounds_lights";
        }
        const auto r = api_client->SetCabinLights(proposal.proposal_value);
        if (!r.ok) {
            return "rejected_apply_failed_lights";
        }
    } else if (proposal.proposal_action == "set_hazards") {
        if (proposal.proposal_value != 0 && proposal.proposal_value != 1) {
            return "rejected_out_of_bounds_hazards";
        }
        const auto r = api_client->SetHazards(proposal.proposal_value == 1);
        if (!r.ok) {
            return "rejected_apply_failed_hazards";
        }
    } else {
        return "rejected_action_not_allowed";
    }

    used_nonces->insert(proposal.proposal_nonce);
    return "accepted_applied";
}

}  // namespace cabinos
