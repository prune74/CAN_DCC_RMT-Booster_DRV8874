#pragma once
#include "BoosterCanConfig.h"
#include "BoosterCore.h"
#include "StatusLeds.h"

// ============================================================
// CanBooster — Gestion du bus CAN pour le booster
// Architecture CFC : l'application fournit un provider,
// la bibliothèque fait tout le reste.
// ============================================================

class CanBooster {
public:
    void begin(BoosterCore &core, StatusLeds &leds);
    void poll();
    bool announceDiscoveryId(uint8_t boosterId);

private:
    BoosterCanConfig cfgProvider;
    BoosterCore *core = nullptr;
    StatusLeds *leds = nullptr;
    CanBus *bus = nullptr;
};
