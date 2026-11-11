#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "Pins.h"

// ============================================================
// RailCom — lecture de l'adresse RailCom de la loco pendant le cutout
// (principe Locoduino — obligatoire Discovery 2026)
//
//  Le front-end de la carte fait tout l'analogique (pont BAT54,
//  ampli TLV9151, opto TLP2362) et sort RAILCOM_ISO : signal
//  LOGIQUE 3,3 V isolé galvaniquement.
//
//  Le RailCom est transmis à 250 kbit/s (NMRA S-9.3.2), 8 bits de
//  données : on le décode au périphérique UART de l'ESP32 — zéro
//  gigue, le CPU ne cadence rien (même philosophie que le RMT
//  pour le DCC côté convertisseur).
//
//  Reconstruction de l'adresse (principe Locoduino, identique à
//  CAN_DCC-Booster) :
//    CH1 seul     → adresse courte (1..127)
//    CH1 + CH2    → adresse longue = (CH2 << 7) | CH1
//    CH1 = 0/0xFF → trame invalide
// ============================================================

class RailCom {
public:
    bool begin();

    // Appelé par BoosterCore pendant le cutout (EN=0, ~488 µs).
    // Retourne l'adresse (0 = rien détecté).
    uint16_t capture(uint32_t window_us);

    // Statistiques (diagnostic série)
    uint32_t frames = 0;   // fenêtres avec ≥ 1 octet reçu
    uint32_t valid  = 0;   // adresses valides décodées

private:
    static constexpr uint8_t MAX_BYTES = 4; // jusqu'à 2 paquets par cutout
    static uint16_t decode(uint8_t ch1, uint8_t ch2);
};