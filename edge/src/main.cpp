#include <cctype>
#include <iostream>
#include <string>

#include "cabinos/service_broker.hpp"
#if defined(CABINOS_HAS_SQLITE)
#include "cabinos/state_store.hpp"
#endif

namespace {

std::string Trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string ToLower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

}  // namespace

int main() {
    cabinos::IntentRouter router;
    cabinos::ServiceBroker broker(router);

#if defined(CABINOS_HAS_SQLITE)
    cabinos::SqliteStateStore state_store("cabinos_state.db");
    std::string state_status;
    cabinos::RuntimeSnapshot restored{};
    if (state_store.Load(&restored, &state_status)) {
        broker.Restore(restored);
    }
    std::cout << "[state] " << state_status << "\n";
#endif

    std::cout << "CabinOS CLI (type 'exit' to quit)\n";
    std::cout << "Try commands like:\n";
    std::cout << "  - turn on hazards\n";
    std::cout << "  - turn hazards off\n";
    std::cout << "  - set cabin temperature to 24\n";
    std::cout << "  - dim lights to 15\n";
    std::cout << "  - what is battery status\n";
    std::cout << "  - show state\n";
    std::cout << "  - find coffee on my route\n";
    std::cout << "Cloud online? [y/n]: ";
    char online = 'n';
    std::cin >> online;
    std::cin.ignore();
    const bool cloud_online = (online == 'y' || online == 'Y');

    for (;;) {
        std::cout << "\ncommand> ";
        std::string utterance;
        std::getline(std::cin, utterance);
        utterance = Trim(utterance);
        if (utterance == "exit") {
            break;
        }
        if (utterance.empty()) {
            continue;
        }

        const std::string cmd = ToLower(utterance);
        if (cmd == "show state" || cmd == "state") {
            const auto s = broker.Snapshot();
            std::cout << "Runtime state:\n";
            std::cout << "  cabin_temperature_c: " << s.cabin_temperature_c << "\n";
            std::cout << "  cabin_lights_level: " << s.cabin_lights_level << "\n";
            std::cout << "  hazards_on: " << (s.hazards_on ? "true" : "false") << "\n";
            std::cout << "  battery_soc_percent: " << s.battery_soc_percent << "\n";
            continue;
        }

        const auto result = broker.HandleTextCommand(utterance, cloud_online);
        std::cout << result.message << "\n";
#if defined(CABINOS_HAS_SQLITE)
        const auto snapshot = broker.Snapshot();
        if (state_store.Save(snapshot, &state_status)) {
            std::cout << "[state] " << state_status << "\n";
        }
#endif
    }

    return 0;
}
