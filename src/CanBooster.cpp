#include "CanBooster.h"
#include "Config.h"
#include <CanUniversal.h>
#include <CfcDccProtocol.h>

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

bool CanBooster::announceDiscoveryId(uint8_t boosterId)
{
    if (!bus || !bus->isValid() || boosterId == 0 || boosterId > CFG_MAX_CANTON_ID)
        return false;
    CanMsg msg(static_cast<uint16_t>(cfc::dcc::CAN_BOOSTER_DISCOVERY_ID),
               {cfc::dcc::BOOSTER_DISCOVERY_FORMAT_VERSION, boosterId, 0, 0, 0, 0, 0, 0});
    return bus->send(msg);
}
