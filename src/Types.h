#pragma once
#include <stdint.h>

// États de supervision du booster, transmis à l’EXCC par UART
enum class BoosterState : uint8_t {
    OK        = 0,
    FAULT     = 1,
    OVERHEAT  = 2,
    OFF       = 3,
    RETRY     = 4   // réservé essais locaux
};

struct BoosterTelemetry {
    uint16_t     current_mA = 0;
    uint16_t     voltage_mV = 0; // tension mesurée côté sortie alim, pas aux bornes voie
    uint16_t     railVoltage_mV = 0; // tension après DRV8874, avec correction configurée
    bool         railVoltageValid = false; // invalide tant que l'étalonnage n'est pas fait
    uint16_t     railcomAddress = 0; // 0 = aucune adresse détectée
    BoosterState state      = BoosterState::OFF;
};
