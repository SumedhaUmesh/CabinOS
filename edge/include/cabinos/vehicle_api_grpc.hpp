#pragma once

#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "cabinos/vehicle_api.hpp"
#include "cabinos/vehicle_services.hpp"
#include "services.grpc.pb.h"

namespace cabinos {

class GrpcVehicleServiceImpl final : public cabinos::services::v1::HVACService::Service,
                                     public cabinos::services::v1::LightingService::Service,
                                     public cabinos::services::v1::BatteryService::Service {
public:
    GrpcVehicleServiceImpl(HVACService* hvac, LightingService* lighting, BatteryService* battery)
        : hvac_(hvac), lighting_(lighting), battery_(battery) {}

    grpc::Status SetTemperature(grpc::ServerContext* context,
                                const cabinos::services::v1::SetTemperatureRequest* request,
                                cabinos::services::v1::SetTemperatureResponse* response) override;

    grpc::Status SetHazards(grpc::ServerContext* context,
                            const cabinos::services::v1::SetHazardsRequest* request,
                            cabinos::services::v1::SetHazardsResponse* response) override;

    grpc::Status SetCabinLights(grpc::ServerContext* context,
                                const cabinos::services::v1::SetCabinLightsRequest* request,
                                cabinos::services::v1::SetCabinLightsResponse* response) override;

    grpc::Status GetBatteryStatus(grpc::ServerContext* context,
                                  const cabinos::services::v1::GetBatteryStatusRequest* request,
                                  cabinos::services::v1::GetBatteryStatusResponse* response) override;

private:
    HVACService* hvac_;
    LightingService* lighting_;
    BatteryService* battery_;
};

class GrpcVehicleApiClient {
public:
    explicit GrpcVehicleApiClient(std::shared_ptr<grpc::Channel> channel);

    SetTemperatureResponse SetTemperature(int temp_c) const;
    SetHazardsResponse SetHazards(bool on) const;
    SetCabinLightsResponse SetCabinLights(int level_percent) const;
    GetBatteryStatusResponse GetBatteryStatus() const;

private:
    std::unique_ptr<cabinos::services::v1::HVACService::Stub> hvac_stub_;
    std::unique_ptr<cabinos::services::v1::LightingService::Stub> lighting_stub_;
    std::unique_ptr<cabinos::services::v1::BatteryService::Stub> battery_stub_;
};

void RunGrpcVehicleServer(const std::string& address);

}  // namespace cabinos
