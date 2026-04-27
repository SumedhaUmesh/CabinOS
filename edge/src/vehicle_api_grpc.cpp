#include "cabinos/vehicle_api_grpc.hpp"

#include <iostream>

namespace cabinos {

grpc::Status GrpcVehicleServiceImpl::SetTemperature(
    grpc::ServerContext* /*context*/,
    const cabinos::services::v1::SetTemperatureRequest* request,
    cabinos::services::v1::SetTemperatureResponse* response) {
    const bool ok = hvac_->SetCabinTemperatureC(request->temp_c());
    response->set_ok(ok);
    response->set_applied_temp_c(hvac_->CabinTemperatureC());
    if (!ok) {
        response->set_message("Temperature rejected. Allowed range is 16-30C.");
        return grpc::Status::OK;
    }
    response->set_message("Cabin temperature set to " + std::to_string(hvac_->CabinTemperatureC()) + "C.");
    return grpc::Status::OK;
}

grpc::Status GrpcVehicleServiceImpl::SetHazards(
    grpc::ServerContext* /*context*/,
    const cabinos::services::v1::SetHazardsRequest* request,
    cabinos::services::v1::SetHazardsResponse* response) {
    lighting_->SetHazards(request->on());
    response->set_ok(true);
    response->set_message(std::string("Hazards ") + (request->on() ? "enabled." : "disabled."));
    return grpc::Status::OK;
}

grpc::Status GrpcVehicleServiceImpl::SetCabinLights(
    grpc::ServerContext* /*context*/,
    const cabinos::services::v1::SetCabinLightsRequest* request,
    cabinos::services::v1::SetCabinLightsResponse* response) {
    lighting_->SetCabinLightsLevel(request->level_percent());
    response->set_ok(true);
    response->set_applied_level_percent(lighting_->CabinLightsLevel());
    response->set_message("Cabin lights set to " + std::to_string(lighting_->CabinLightsLevel()) + "%.");
    return grpc::Status::OK;
}

grpc::Status GrpcVehicleServiceImpl::GetBatteryStatus(
    grpc::ServerContext* /*context*/,
    const cabinos::services::v1::GetBatteryStatusRequest* /*request*/,
    cabinos::services::v1::GetBatteryStatusResponse* response) {
    const BatterySnapshot snapshot = battery_->ReadSnapshot();
    response->set_ok(true);
    response->set_soc_percent(snapshot.soc_percent);
    response->set_message("Battery SoC is " + std::to_string(snapshot.soc_percent) + "%.");
    return grpc::Status::OK;
}

GrpcVehicleApiClient::GrpcVehicleApiClient(std::shared_ptr<grpc::Channel> channel)
    : hvac_stub_(cabinos::services::v1::HVACService::NewStub(channel)),
      lighting_stub_(cabinos::services::v1::LightingService::NewStub(channel)),
      battery_stub_(cabinos::services::v1::BatteryService::NewStub(channel)) {}

SetTemperatureResponse GrpcVehicleApiClient::SetTemperature(const int temp_c) const {
    grpc::ClientContext context;
    cabinos::services::v1::SetTemperatureRequest request;
    cabinos::services::v1::SetTemperatureResponse response;
    request.set_temp_c(temp_c);

    const grpc::Status status = hvac_stub_->SetTemperature(&context, request, &response);
    if (!status.ok()) {
        return SetTemperatureResponse{false, "gRPC error: " + status.error_message(), temp_c};
    }
    return SetTemperatureResponse{response.ok(), response.message(), response.applied_temp_c()};
}

SetHazardsResponse GrpcVehicleApiClient::SetHazards(const bool on) const {
    grpc::ClientContext context;
    cabinos::services::v1::SetHazardsRequest request;
    cabinos::services::v1::SetHazardsResponse response;
    request.set_on(on);

    const grpc::Status status = lighting_stub_->SetHazards(&context, request, &response);
    if (!status.ok()) {
        return SetHazardsResponse{false, "gRPC error: " + status.error_message()};
    }
    return SetHazardsResponse{response.ok(), response.message()};
}

SetCabinLightsResponse GrpcVehicleApiClient::SetCabinLights(const int level_percent) const {
    grpc::ClientContext context;
    cabinos::services::v1::SetCabinLightsRequest request;
    cabinos::services::v1::SetCabinLightsResponse response;
    request.set_level_percent(level_percent);

    const grpc::Status status = lighting_stub_->SetCabinLights(&context, request, &response);
    if (!status.ok()) {
        return SetCabinLightsResponse{false, "gRPC error: " + status.error_message(), level_percent};
    }
    return SetCabinLightsResponse{
        response.ok(),
        response.message(),
        response.applied_level_percent(),
    };
}

GetBatteryStatusResponse GrpcVehicleApiClient::GetBatteryStatus() const {
    grpc::ClientContext context;
    cabinos::services::v1::GetBatteryStatusRequest request;
    cabinos::services::v1::GetBatteryStatusResponse response;

    const grpc::Status status = battery_stub_->GetBatteryStatus(&context, request, &response);
    if (!status.ok()) {
        return GetBatteryStatusResponse{false, -1, "gRPC error: " + status.error_message()};
    }
    return GetBatteryStatusResponse{response.ok(), response.soc_percent(), response.message()};
}

void RunGrpcVehicleServer(const std::string& address) {
    HVACService hvac;
    LightingService lighting;
    BatteryService battery;
    GrpcVehicleServiceImpl service(&hvac, &lighting, &battery);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(static_cast<cabinos::services::v1::HVACService::Service*>(&service));
    builder.RegisterService(static_cast<cabinos::services::v1::LightingService::Service*>(&service));
    builder.RegisterService(static_cast<cabinos::services::v1::BatteryService::Service*>(&service));

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "CabinOS gRPC server listening on " << address << std::endl;
    server->Wait();
}

}  // namespace cabinos
