#include <iostream>
#include <string>

#include "cabinos/service_broker.hpp"

int main() {
    cabinos::IntentRouter router;
    cabinos::ServiceBroker broker(router);

    std::cout << "CabinOS CLI (type 'exit' to quit)\n";
    std::cout << "Try commands like:\n";
    std::cout << "  - turn on hazards\n";
    std::cout << "  - turn hazards off\n";
    std::cout << "  - set cabin temperature to 24\n";
    std::cout << "  - dim lights to 15\n";
    std::cout << "  - what is battery status\n";
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
        if (utterance == "exit") {
            break;
        }
        const auto result = broker.HandleTextCommand(utterance, cloud_online);
        std::cout << result.message << "\n";
    }

    return 0;
}
