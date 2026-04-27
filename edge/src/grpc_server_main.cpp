#include <string>

#include "cabinos/vehicle_api_grpc.hpp"

int main(int argc, char** argv) {
    std::string address = "0.0.0.0:50051";
    if (argc > 1) {
        address = argv[1];
    }
    cabinos::RunGrpcVehicleServer(address);
    return 0;
}
