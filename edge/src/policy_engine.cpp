#include "cabinos/policy_engine.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace cabinos {
namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string Trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(b, e - b);
}

bool Contains(const std::string& text, const std::string& token) {
    return text.find(token) != std::string::npos;
}

}  // namespace

PolicyEngine::PolicyEngine(const std::string& policy_path) {
    LoadDefaults();
    TryLoadPolicyFile(policy_path);
}

void PolicyEngine::LoadDefaults() {
    tier_policies_[Tier::kSafetyCritical] = TierPolicy{false, {}};
    tier_policies_[Tier::kComfort] = TierPolicy{false, {}};
    tier_policies_[Tier::kCognitive] =
        TierPolicy{true, {"places.search", "vehicle.set_state_proposal", "maintenance.suggest"}};

    tier_keywords_[Tier::kSafetyCritical] = {"hazard", "defog", "brake"};
    tier_keywords_[Tier::kComfort] = {"cabin", "temperature", "light", "battery", "soc", "charge"};
}

void PolicyEngine::TryLoadPolicyFile(const std::string& policy_path) {
    std::ifstream in(policy_path);
    if (!in) {
        return;
    }

    enum class Section { kNone, kKeywords, kTiers };
    Section section = Section::kNone;
    Tier current_tier = Tier::kCognitive;
    bool in_tools = false;

    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        if (trimmed == "keywords:") {
            section = Section::kKeywords;
            in_tools = false;
            continue;
        }
        if (trimmed == "tiers:") {
            section = Section::kTiers;
            in_tools = false;
            continue;
        }
        if (trimmed == "safety_critical:") {
            current_tier = Tier::kSafetyCritical;
            in_tools = false;
            continue;
        }
        if (trimmed == "comfort:") {
            current_tier = Tier::kComfort;
            in_tools = false;
            continue;
        }
        if (trimmed == "cognitive:") {
            current_tier = Tier::kCognitive;
            in_tools = false;
            continue;
        }

        if (section == Section::kKeywords) {
            if (trimmed.rfind("- ", 0) == 0) {
                tier_keywords_[current_tier].push_back(ToLower(Trim(trimmed.substr(2))));
            }
            continue;
        }

        if (section == Section::kTiers) {
            if (trimmed == "tools:") {
                in_tools = true;
                tier_policies_[current_tier].tools.clear();
                continue;
            }
            if (trimmed.rfind("cloud_allowed:", 0) == 0) {
                const std::string v = ToLower(Trim(trimmed.substr(std::string("cloud_allowed:").size())));
                tier_policies_[current_tier].cloud_allowed = (v == "true");
                continue;
            }
            if (in_tools && trimmed.rfind("- ", 0) == 0) {
                tier_policies_[current_tier].tools.push_back(Trim(trimmed.substr(2)));
                continue;
            }
        }
    }
}

Tier PolicyEngine::Classify(const std::string& utterance) const {
    const std::string text = ToLower(utterance);
    for (const std::string& token : tier_keywords_.at(Tier::kSafetyCritical)) {
        if (Contains(text, token)) {
            return Tier::kSafetyCritical;
        }
    }
    for (const std::string& token : tier_keywords_.at(Tier::kComfort)) {
        if (Contains(text, token)) {
            return Tier::kComfort;
        }
    }
    return Tier::kCognitive;
}

bool PolicyEngine::CloudAllowed(const Tier tier) const {
    const auto it = tier_policies_.find(tier);
    if (it == tier_policies_.end()) {
        return false;
    }
    return it->second.cloud_allowed;
}

const std::vector<std::string>& PolicyEngine::AllowedTools(const Tier tier) const {
    static const std::vector<std::string> kEmpty;
    const auto it = tier_policies_.find(tier);
    if (it == tier_policies_.end()) {
        return kEmpty;
    }
    return it->second.tools;
}

}  // namespace cabinos
