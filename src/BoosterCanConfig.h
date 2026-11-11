#pragma once

/**
 * ============================================================================
 *  BoosterCanConfig Configuration CAN du booster (RMT)
 * ============================================================================
 *  Provider pour la bibliothèque CanUniversal.
 *  Le CAN est natif sur l'ESP32 (TWAI), aucune puce externe.
 *  Broches (voir Config.h) : TX = GPIO 5, RX = GPIO 4.
 * ============================================================================
 */

#include <CanUniversal.h>
#include "Config.h"

class BoosterCanConfig : public CanConfigProvider {
public:
    BoosterCanConfig() : cfg(true,                       // enabled
                              CFG_CAN_BITRATE,           // 500 kbit/s (identique au convertisseur)
                              (gpio_num_t)PIN_CAN_TX,    // TWAI natif (GPIO 5)
                              (gpio_num_t)PIN_CAN_RX,    // TWAI natif (GPIO 4)
                              GPIO_NUM_NC, GPIO_NUM_NC,  // CS, INT (inutilisées - TWAI natif)
                              GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, // SCK, MOSI, MISO
                              0,                          // quartz MCP2515 inutilisé
                              false)                      // loopback matériel (désactivé)
    {}

    uint8_t busCount() const override { return 1; }
    const CanBusConfig& bus(uint8_t) const override { return cfg; }

private:
    CanBusConfig cfg;
};
