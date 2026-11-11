#include "CanBooster.h"
#include <CanUniversal.h>
#include <string.h> // memcpy

// ============================================================
// Implémentation CanBooster
// ============================================================

void CanBooster::begin(BoosterCore &core, StatusLeds &leds) {
    this->core = &core;
    this->leds = &leds;
    CanInit::begin(cfgProvider);   // tolérant aux erreurs (bus invalide ≠ crash)
    bus = &CanBus::bus(0);         // référence toujours valide
}

void CanBooster::poll() {
    if (!bus->isValid()) return;
    CanMsg msg;
    while (bus->receive(msg)) {    // réception non bloquante
        leds->noteCanActivity(millis());
        // ID 11 bits : cast explicite en uint16_t (cf. piège CanMsg)
        core->onCanMessage((uint16_t)msg.id, msg.data, msg.dlc);
    }
}

// Callback de Telemetry : émission 0x102 via CanMsg (champs publics)
void CanBooster::sendFrame(uint16_t id, const uint8_t *data, uint8_t len) {
    CanMsg msg;
    msg.id = id;
    msg.dlc = len;
    memcpy(msg.data, data, len);
    CanBus::bus(0).send(msg);       // non bloquant (false = case TX pleine)
}
