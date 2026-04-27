#include <iostream>
#include <string>

#include "cabinos/can_bridge.hpp"
#include "cabinos/vehicle_services.hpp"

namespace {

void PrintUsage() {
    std::cout << "Usage:\n";
    std::cout << "  cabinos_can_ingest synthetic <soc_percent>\n";
    std::cout << "  cabinos_can_ingest socketcan <ifname>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    cabinos::BatteryService battery;
    cabinos::CanBridge bridge(&battery);
    std::string status;

    const std::string mode = argv[1];
    if (mode == "synthetic") {
        const int soc = std::stoi(argv[2]);
        const bool ok = bridge.IngestSyntheticFrame(0x100, static_cast<unsigned char>(soc), &status);
        std::cout << status << "\n";
        return ok ? 0 : 2;
    }

    if (mode == "socketcan") {
        const std::string ifname = argv[2];
        const bool ok = bridge.PollSocketCanOnce(ifname, &status);
        std::cout << status << "\n";
        return ok ? 0 : 2;
    }

    PrintUsage();
    return 1;
}
