#include "Telemetry.h"

Telemetry::Telemetry(BoosterCore &core, SendFrame send) : core(core), send(send) {}

void Telemetry::update() {
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
