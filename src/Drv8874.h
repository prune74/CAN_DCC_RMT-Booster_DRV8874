#pragma once
#include <Arduino.h>
#include "Arduino.h"
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
    void begin() {
        pinMode(CFG_PIN_DRV_EN, OUTPUT);
        pinMode(CFG_PIN_DRV_NSLEEP, OUTPUT);
        digitalWrite(CFG_PIN_DRV_EN, LOW);      // sortie coupée au boot
        digitalWrite(CFG_PIN_DRV_NSLEEP, HIGH);
        analogReadResolution(12);
        // Atténuation 11 dB → pleine échelle ~3.3 V (IPROPI, pont voie)
        analogSetPinAttenuation(CFG_PIN_IPROPI, ADC_11db);
        analogSetPinAttenuation(CFG_PIN_VRAIL, ADC_11db);
        enabled = false;
    }

    void enableOutput()  { digitalWrite(CFG_PIN_DRV_EN, HIGH); enabled = true; }
    void disableOutput() { digitalWrite(CFG_PIN_DRV_EN, LOW);  enabled = false; }
    bool isEnabled() const { return enabled; }
    // Pas de timer ici : le cutout est piloté par BoosterCore, qui
    // garde EN=0 pendant la capture RailCom (454 µs) puis rétablit.

    // --- Mesures ---
    // TODO calibration : IPROPI du DRV8874 : I_charge = V_IPROPI / RIPROPI / AIPR.
    // Régler RIPROPI pour que le seuil de coupure (ILIM) corresponde au réseau CFC.
    uint16_t readCurrent_mA() {
        const uint32_t adc = analogRead(CFG_PIN_IPROPI);
        return (uint16_t)((adc * 3300UL) / 4095UL); // mV IPROPI → échelle mA à calibrer
    }

    // TODO calibration : pont diviseur voie (ex. 10k/1k → ×11)
    uint16_t readVoltage_mV() {
        const uint32_t adc = analogRead(CFG_PIN_VRAIL);
        return (uint16_t)((adc * 3300UL * 11UL) / 4095UL);
    }

private:
    bool enabled = false;
};