#include "cabinos/vehicle_api.hpp"

namespace cabinos {

SetTemperatureResponse VehicleApiServer::SetTemperature(const SetTemperatureRequest& req) const {
    const bool ok = hvac_->SetCabinTemperatureC(req.temp_c);
    if (!ok) {
        return SetTemperatureResponse{
            false,
            "Temperature rejected. Allowed range is 16-30C.",
            hvac_->CabinTemperatureC(),
        };
    }
    return SetTemperatureResponse{
        true,
        "Cabin temperature set to " + std::to_string(hvac_->CabinTemperatureC()) + "C.",
        hvac_->CabinTemperatureC(),
    };
}

SetHazardsResponse VehicleApiServer::SetHazards(const SetHazardsRequest& req) const {
    lighting_->SetHazards(req.on);
    return SetHazardsResponse{
        true,
        std::string("Hazards ") + (req.on ? "enabled." : "disabled."),
    };
}

SetCabinLightsResponse VehicleApiServer::SetCabinLights(const SetCabinLightsRequest& req) const {
    lighting_->SetCabinLightsLevel(req.level_percent);
    return SetCabinLightsResponse{
        true,
        "Cabin lights set to " + std::to_string(lighting_->CabinLightsLevel()) + "%.",
        lighting_->CabinLightsLevel(),
    };
}

GetBatteryStatusResponse VehicleApiServer::GetBatteryStatus() const {
    const BatterySnapshot snapshot = battery_->ReadSnapshot();
    return GetBatteryStatusResponse{
        true,
        snapshot.soc_percent,
        "Battery SoC is " + std::to_string(snapshot.soc_percent) + "%.",
    };
}

SetTemperatureResponse VehicleApiClient::SetTemperature(const int temp_c) const {
    return server_->SetTemperature(SetTemperatureRequest{temp_c});
}

SetHazardsResponse VehicleApiClient::SetHazards(const bool on) const {
    return server_->SetHazards(SetHazardsRequest{on});
}

SetCabinLightsResponse VehicleApiClient::SetCabinLights(const int level_percent) const {
    return server_->SetCabinLights(SetCabinLightsRequest{level_percent});
}

GetBatteryStatusResponse VehicleApiClient::GetBatteryStatus() const {
    return server_->GetBatteryStatus();
}

}  // namespace cabinos
