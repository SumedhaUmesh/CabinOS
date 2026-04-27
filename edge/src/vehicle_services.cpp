#include "cabinos/vehicle_services.hpp"

namespace cabinos {

bool HVACService::SetCabinTemperatureC(const int temp_c) {
    if (temp_c < 16 || temp_c > 30) {
        return false;
    }
    cabin_temperature_c_ = temp_c;
    return true;
}

void LightingService::SetCabinLightsLevel(const int level) {
    if (level < 0) {
        cabin_lights_level_ = 0;
        return;
    }
    if (level > 100) {
        cabin_lights_level_ = 100;
        return;
    }
    cabin_lights_level_ = level;
}

bool BatteryService::SetSocPercent(const int soc_percent) {
    if (soc_percent < 0 || soc_percent > 100) {
        return false;
    }
    soc_percent_ = soc_percent;
    return true;
}

bool BatteryService::UpdateFromCanPayload(const uint32_t can_id, const uint8_t byte0) {
    constexpr uint32_t kBatterySocCanId = 0x100;
    if (can_id != kBatterySocCanId) {
        return false;
    }
    return SetSocPercent(static_cast<int>(byte0));
}

}  // namespace cabinos
