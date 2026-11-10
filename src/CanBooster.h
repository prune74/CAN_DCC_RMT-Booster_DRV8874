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

    // Callback de Telemetry : émission 0x102 via CanMsg
    static void sendFrame(uint16_t id, const uint8_t *data, uint8_t len);

private:
    BoosterCanConfig cfgProvider;
    BoosterCore *core = nullptr;
    StatusLeds *leds = nullptr;
    CanBus *bus = nullptr;
};
