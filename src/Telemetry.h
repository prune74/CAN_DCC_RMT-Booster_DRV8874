#pragma once
#include <Arduino.h>
#include "BoosterCore.h"
#include "Config.h"
#include "Types.h"

// ============================================================
// Telemetry — trame 0x102 sur changement d'état et heartbeat périodique (DLC 5)
//   data[0] = ID   (ID du canton attribué par le CC)
//   data[1] = TYPE (1 = booster)
//   data[2] = mA / 10        (2550 mA max → DRV8874 OK)
//   data[3] = mV / 100       (25.5 V max)
//   data[4] = état (0=OK 1=FAULT 2=OVERHEAT 3=OFF 4=RETRY)
// Envoyée via le callback sendFrame fourni par CanBooster.
// ============================================================

class Telemetry {
public:
    using SendFrame = bool (*)(uint16_t id, const uint8_t *data, uint8_t len);

    Telemetry(BoosterCore &core, SendFrame send);
    void update();

private:
    BoosterCore &core;
    SendFrame send;
    uint32_t lastSend = 0;
    uint8_t lastState = 0xFF;
    uint8_t lastBoosterId = CFG_CANTON_ID_UNASSIGNED;
    bool hasSentState = false;
};
