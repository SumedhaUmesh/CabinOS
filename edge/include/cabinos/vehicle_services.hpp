#pragma once

#include <cstdint>
#include <string>

namespace cabinos {

struct BatterySnapshot {
    int soc_percent;
};

class HVACService {
public:
    bool SetCabinTemperatureC(int temp_c);
    int CabinTemperatureC() const { return cabin_temperature_c_; }

private:
    int cabin_temperature_c_ = 22;
};

class LightingService {
public:
    void SetHazards(bool on) { hazards_on_ = on; }
    void SetCabinLightsLevel(int level);

    bool HazardsOn() const { return hazards_on_; }
    int CabinLightsLevel() const { return cabin_lights_level_; }

private:
    bool hazards_on_ = false;
    int cabin_lights_level_ = 50;
};

class BatteryService {
public:
    BatterySnapshot ReadSnapshot() const { return BatterySnapshot{soc_percent_}; }
    bool SetSocPercent(int soc_percent);
    bool UpdateFromCanPayload(uint32_t can_id, uint8_t byte0);

private:
    int soc_percent_ = 76;
};

}  // namespace cabinos
