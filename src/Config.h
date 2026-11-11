#pragma once
#include <cstdint>
#include "Pins.h"

// ======================================================================================
// CAN_DCC_RMT-BoosterDRV8874 — Configuration carte des Chemins de Fer du Chablais (CFC)
// Tout ce qui est PARAMETRAGE (valeurs) est ICI, nulle part ailleurs.
// Le brochage est dans Pins.h
// ======================================================================================

// --- Identité sur le bus (trame 0x102) ---
static const uint8_t  CFG_BOOSTER_ID   = 1;     // ID_B_01 (1..32, unique sur le bus)
static const uint8_t  CFG_BOOSTER_TYPE = 1;     // TYPE B = booster (nommage ID_B_xx)

// --- Bus CAN (identique au convertisseur) ---
static const uint32_t CFG_CAN_BITRATE = 500000; // 500 kbit/s

// --- RailCom HF ---
static const uint16_t CFG_RAILCOM_ADC_ZERO = 2048; // Zéro HF à calibrer (12 bits)

// --- Sécurité ---
static const uint32_t CFG_FAILSAFE_MS   = 500;  // Pas de bloc 0x100 depuis 500 ms → OFF
static const uint16_t CFG_MAX_CURRENT_MA = 2000; // Seuil coupure (à calibrer avec RIPROPI)
static const uint16_t CFG_MIN_VOLTAGE_MV = 9000; // Sous-tension → FAULT
static const uint32_t CFG_CUTOUT_US     = 454;  // Cutout nominal régénéré (RailCom)

// --- LED d'état ---
static const uint8_t CFG_WS2812_BRIGHTNESS = 40; // Luminosité (0-255)

// --- Digipot (MCP4551/4561) ---
// Adresse I2C du digipot
static constexpr uint8_t DIGIPOT_ADDR = 0x2E;
// Registre du wiper
static constexpr uint8_t DIGIPOT_REG = 0x00;
// Valeurs à ajuster après calibration
static constexpr uint8_t DIGIPOT_HO = 23;   // Profil HO (15V)
static constexpr uint8_t DIGIPOT_N  = 30;   // Profil N (12V)
