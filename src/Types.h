#pragma once
#include <stdint.h>

// États de la trame 0x102 — codes imposés par DCC_CAN_RMT-Booster
enum class BoosterState : uint8_t {
    OK        = 0,
    FAULT     = 1,
    OVERHEAT  = 2,
    OFF       = 3,
    RETRY     = 4   // réservé essais locaux
};

struct BoosterTelemetry {
    uint16_t     current_mA = 0;
    uint16_t     voltage_mV = 0;
    uint16_t     railcomAddress = 0; // 0 = aucune adresse détectée
    BoosterState state      = BoosterState::OFF;
};