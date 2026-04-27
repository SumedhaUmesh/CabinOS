#include "cabinos/cloud_bridge.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

#if defined(CABINOS_HAS_CURL)
#include <curl/curl.h>
#endif

namespace cabinos {
namespace {

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char c : value) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

std::string GetEnvOrDefault(const char* key, const std::string& fallback) {
    const char* v = std::getenv(key);
    if (v == nullptr || std::string(v).empty()) {
        return fallback;
    }
    return std::string(v);
}

bool EndsWithPathInvoke(const std::string& url) {
    if (url.size() < 7) {
        return false;
    }
    return url.rfind("/invoke") == url.size() - 7;
}

#if defined(CABINOS_HAS_CURL)
size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

bool TryExtractJsonStringField(const std::string& json, const std::string& field, std::string* out_value) {
    const std::string key = "\"" + field + "\"";
    const size_t key_pos = json.find(key);
    if (key_pos == std::string::npos) {
        return false;
    }

    size_t colon = json.find(':', key_pos + key.size());
    if (colon == std::string::npos) {
        return false;
    }

    size_t i = colon + 1;
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
        ++i;
    }
    if (i >= json.size() || json[i] != '"') {
        return false;
    }
    ++i;  // opening quote

    std::string value;
    while (i < json.size()) {
        const char c = json[i];
        if (c == '\\') {
            if (i + 1 >= json.size()) {
                return false;
            }
            const char n = json[i + 1];
            if (n == 'n') {
                value.push_back('\n');
            } else if (n == 'r') {
                value.push_back('\r');
            } else if (n == 't') {
                value.push_back('\t');
            } else if (n == '\\' || n == '"') {
                value.push_back(n);
            } else {
                value.push_back(n);
            }
            i += 2;
            continue;
        }
        if (c == '"') {
            break;
        }
        value.push_back(c);
        ++i;
    }
    *out_value = std::move(value);
    return true;
}
#endif

}  // namespace

CloudBridgeClient CloudBridgeClient::FromEnv() {
    std::string url = GetEnvOrDefault("CABINOS_CLOUD_URL", "http://127.0.0.1:3000/invoke");
    if (!EndsWithPathInvoke(url)) {
        if (!url.empty() && url.back() == '/') {
            url.pop_back();
        }
        url += "/invoke";
    }
    std::string session_id = GetEnvOrDefault("CABINOS_SESSION_ID", "cli-session");
    return CloudBridgeClient(url, session_id);
}

CloudInvokeResult CloudBridgeClient::InvokeCognitive(const std::string& utterance) const {
#if defined(CABINOS_HAS_CURL)
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return CloudInvokeResult{false, false, "Cloud bridge unavailable (curl init failed)."};
    }

    std::ostringstream body;
    body << "{"
         << "\"session_id\":\"" << JsonEscape(session_id_) << "\","
         << "\"utterance\":\"" << JsonEscape(utterance) << "\""
         << "}";

    const std::string payload = body.str();
    std::string response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, invoke_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        return CloudInvokeResult{false, true, std::string("Cloud bridge request failed: ") + curl_easy_strerror(rc)};
    }
    if (http_code < 200 || http_code >= 300) {
        return CloudInvokeResult{false, true, "Cloud bridge HTTP error: " + std::to_string(http_code) + " body=" + response};
    }

    std::string reply;
    if (!TryExtractJsonStringField(response, "reply", &reply)) {
        return CloudInvokeResult{false, true, "Cloud bridge returned unexpected JSON: " + response};
    }
    return CloudInvokeResult{true, true, reply};
#else
    (void)utterance;
    (void)invoke_url_;
    (void)session_id_;
    return CloudInvokeResult{
        false,
        false,
        "Cloud bridge not compiled in (libcurl not found). Build with CURL available, or use offline mode.",
    };
#endif
}

}  // namespace cabinos
