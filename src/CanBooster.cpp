#include "CanBooster.h"
#include <CanUniversal.h>

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
        if (!msg.is11()) continue;
        core->onCanMessage(static_cast<uint16_t>(msg.id), msg.data, msg.dlc);
    }
}
