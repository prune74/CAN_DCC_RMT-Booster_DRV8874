#include "CanBooster.h"
#include <CanUniversal.h>
#include <string.h> // memcpy

// ============================================================
// Implémentation BoosterCanConfig
// ============================================================

BoosterCanConfig::BoosterCanConfig()
    : cfg(true,                       // enabled
          CFG_CAN_BITRATE,            // 500 kbit/s — identique au convertisseur
          (gpio_num_t)CFG_CAN_TX_PIN, // TWAI natif (GPIO 5)
          (gpio_num_t)CFG_CAN_RX_PIN, // TWAI natif (GPIO 4)
          GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC,
          0,                          // quartz MCP2515 inutilisé
          false)                      // loopback matériel (true = tests isolés)
{}

uint8_t BoosterCanConfig::busCount() const {
    return 1;
}

const CanBusConfig &BoosterCanConfig::bus(uint8_t) const {
    return cfg;
}

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
