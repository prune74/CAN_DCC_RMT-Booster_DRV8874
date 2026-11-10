#pragma once
#include <CanUniversal.h>
#include <Arduino.h>
#include "Config.h"

// ============================================================
// BoosterCanConfig — Provider pour CanUniversal v3
// Configure un bus TWAI natif (IDs 11 bits, tout accepter)
// ============================================================

class BoosterCanConfig : public CanConfigProvider {
public:
    BoosterCanConfig();
    uint8_t busCount() const override;
    const CanBusConfig &bus(uint8_t) const override;

private:
    CanBusConfig cfg;
};
