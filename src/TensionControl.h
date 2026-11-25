#pragma once
#include <Wire.h>
#include <Arduino.h>
#include "Pins.h"
#include "Config.h"

// ============================================================
// TensionControl — gestion du MCP45HV51-103 sur bus I2C.
//
// Permet de commuter entre deux profils de tension voie :
//   - HO : consigne nominale 15 V → code de départ configurable
//   - N  : consigne nominale 12 V → code de départ configurable
//
// La commande CAN envoie simplement HO ou N, les valeurs sont
// fixes dans Config.h et ne changent pas.
// ============================================================

class TensionControl {
public:
    void begin() noexcept;
    bool setTrackProfile(bool ho) noexcept;  // true=HO, false=N

private:
    bool writeDigipot(uint8_t value) noexcept;
    bool readDigipot(uint8_t &value) noexcept;
};
