#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>

#include "cabinos/vehicle_api_grpc.hpp"

namespace {

void PrintUsage() {
    std::cout << "Usage:\n";
    std::cout << "  cabinos_grpc_client [address] hazards <on|off>\n";
    std::cout << "  cabinos_grpc_client [address] temp <celsius>\n";
    std::cout << "  cabinos_grpc_client [address] lights <0-100>\n";
    std::cout << "  cabinos_grpc_client [address] battery\n\n";
    std::cout << "Examples:\n";
    std::cout << "  cabinos_grpc_client 127.0.0.1:50051 hazards on\n";
    std::cout << "  cabinos_grpc_client 127.0.0.1:50051 temp 23\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    const std::string address = argv[1];
    const std::string action = argv[2];

    cabinos::GrpcVehicleApiClient client(
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));

    if (action == "hazards") {
        if (argc < 4) {
            PrintUsage();
            return 1;
        }
        const std::string value = argv[3];
        const bool on = (value == "on");
        const auto response = client.SetHazards(on);
        std::cout << response.message << std::endl;
        return response.ok ? 0 : 2;
    }

    if (action == "temp") {
        if (argc < 4) {
            PrintUsage();
            return 1;
        }
        const int temp_c = std::stoi(argv[3]);
        const auto response = client.SetTemperature(temp_c);
        std::cout << response.message << std::endl;
        return response.ok ? 0 : 2;
    }

    if (action == "lights") {
        if (argc < 4) {
            PrintUsage();
            return 1;
        }
        const int level = std::stoi(argv[3]);
        const auto response = client.SetCabinLights(level);
        std::cout << response.message << std::endl;
        return response.ok ? 0 : 2;
    }

    if (action == "battery") {
        const auto response = client.GetBatteryStatus();
        std::cout << response.message << std::endl;
        return response.ok ? 0 : 2;
    }

    PrintUsage();
    return 1;
}
