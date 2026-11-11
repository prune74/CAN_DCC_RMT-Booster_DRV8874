#pragma once
#include <Wire.h>
#include <Arduino.h>
#include "Pins.h"
#include "Config.h"

// ============================================================
// TensionControl — Gestion du digipot pour profil de voie
// (MCP4551 / MCP4561 sur bus I2C)
//
// Permet de commuter entre deux profils de tension voie :
//   - HO : 15V (échelle HO) → DIGIPOT_HO
//   - N  : 12V (échelle N)  → DIGIPOT_N
//
// La commande CAN envoie simplement HO ou N, les valeurs sont
// fixes dans Config.h et ne changent pas.
// ============================================================

class TensionControl {
public:
    void begin() noexcept;
    void setTrackProfile(bool ho) noexcept;  // true=HO, false=N

private:
    void writeDigipot(uint8_t value) noexcept;
};
