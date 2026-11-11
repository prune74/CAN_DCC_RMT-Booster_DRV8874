#pragma once
#include <Arduino.h>
#include "Pins.h"
#include "Config.h"

// ============================================================
// Drv8874 — matériel du booster (interface PH/EN)
//
//  PMODE = 0 (PH/EN) :
//    EN=1, PH=1 → OUT1=H, OUT2=L     EN=1, PH=0 → OUT1=L, OUT2=H
//    → le biphasé DCC est fait par PH seul (une seule sortie RMT).
//    EN=0 → sorties en haute impédance (coast) = arrêt d'urgence.
//
//  Cutout RailCom : EN=0 pendant 454 µs (nominal), PH inchangé.
//  Pendant la fenêtre, le module RailCom capture l'ADC en DMA I2S
//  (2 MHz) et décode CH1/CH2 — cf. src/RailCom.h.
// ============================================================

class Drv8874 {
public:
    void begin();
    void enableOutput();
    void disableOutput();
    bool isEnabled() const;
    uint16_t readCurrent_mA();
    uint16_t readVoltage_mV();

private:
    bool enabled = false;
};
