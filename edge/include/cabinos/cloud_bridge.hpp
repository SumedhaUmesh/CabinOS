#pragma once

#include <string>

namespace cabinos {

struct CloudInvokeResult {
    bool ok;
    bool used_cloud;
    std::string message;
    bool has_proposal = false;
    std::string proposal_action;
    int proposal_value = 0;
    long long proposal_timestamp_ms = 0;
    std::string proposal_nonce;
    std::string proposal_signature;
    bool proposal_schema_error = false;
    std::string proposal_schema_error_reason;
};

class CloudBridgeClient {
public:
    static CloudBridgeClient FromEnv();

    CloudInvokeResult InvokeCognitive(const std::string& utterance) const;

private:
    explicit CloudBridgeClient(std::string invoke_url, std::string session_id)
        : invoke_url_(std::move(invoke_url)), session_id_(std::move(session_id)) {}

    std::string invoke_url_;
    std::string session_id_;
};

}  // namespace cabinos
