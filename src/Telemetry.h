#pragma once
#include "Arduino.h"
#include "BoosterCore.h"
#include "Config.h"
#include "Types.h"

// ============================================================
// Telemetry — trame 0x102, toutes les 1 s (DLC 5)
//   data[0] = ID   (ID_B_xx de la carte, CFG_BOOSTER_ID)
//   data[1] = TYPE (1 = booster)
//   data[2] = mA / 10        (2550 mA max → DRV8874 OK)
//   data[3] = mV / 100       (25.5 V max)
//   data[4] = état (0=OK 1=FAULT 2=OVERHEAT 3=OFF 4=RETRY)
// Envoyée via le callback sendFrame fourni par CanBooster.
// ============================================================

class Telemetry {
public:
    using SendFrame = void (*)(uint16_t id, const uint8_t *data, uint8_t len);

    Telemetry(BoosterCore &core, SendFrame send) : core(core), send(send) {}

    void update() {
        const uint32_t now = millis();
        if (now - lastSend < 1000) return;
        lastSend = now;

        const BoosterTelemetry &t = core.getTelemetry();
        uint8_t data[5];
        data[0] = CFG_BOOSTER_ID;
        data[1] = CFG_BOOSTER_TYPE;
        data[2] = (uint8_t)(t.current_mA / 10);
        data[3] = (uint8_t)(t.voltage_mV / 100);
        data[4] = (uint8_t)t.state;
        send(0x102, data, 5);

        // RailCom : adresse détectée → 0x103, little-endian
        // (réservé côté convertisseur — à valider avec DCC_CAN_RMT-Booster)
        if (t.railcomAddress != 0 && t.railcomAddress != lastRailcomSent) {
            uint8_t rc[2] = { (uint8_t)(t.railcomAddress & 0xFF),
                              (uint8_t)(t.railcomAddress >> 8) };
            send(0x103, rc, 2);
            lastRailcomSent = t.railcomAddress;
        }
    }

private:
    BoosterCore &core;
    SendFrame   send;
    uint32_t    lastSend = 0;
    uint16_t    lastRailcomSent = 0;
};