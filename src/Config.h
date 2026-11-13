#pragma once
#include <cstdint>
#include "Pins.h"

// ======================================================================================
// CAN_DCC_RMT-BoosterDRV8874 — Configuration carte des Chemins de Fer du Chablais (CFC)
// Tout ce qui est PARAMETRAGE (valeurs) est ICI, nulle part ailleurs.
// Le brochage est dans Pins.h
// ======================================================================================

// --- Identité reçue du CC via l'EXCC ---
static constexpr uint8_t CFG_MAX_CANTON_ID = 252;
static constexpr uint8_t CFG_CANTON_ID_UNASSIGNED = 0xFF;
static const uint8_t  CFG_BOOSTER_TYPE = 1;     // TYPE B = booster (nommage ID_B_xx)

// --- Bus CAN (identique au convertisseur) ---
static const uint32_t CFG_CAN_BITRATE = 500000; // 500 kbit/s

// --- Sécurité voie ---
static const uint32_t CFG_FAILSAFE_MS    = 500;   // Pas de bloc 0x100 depuis 500 ms → OFF
static const uint16_t CFG_MAX_CURRENT_MA = 2000;  // Limite voie 2 A (RIPROPI 3,6 kΩ = pleine échelle ADC)
static const uint16_t CFG_MIN_VOLTAGE_MV = 9000;  // Sous-tension → FAULT

// Mesure courant via IPROPI (datasheet SLVSF66A eq.1/2) :
//   I = V_IPROPI / (R_IPROPI * AIPROPI)
static constexpr uint16_t CFG_RIPROPI_OHMS        = 3600; // R IPROPI→GND sur la carte
static constexpr uint16_t CFG_IPROPI_GAIN_UA_PER_A = 455; // 450 typ. datasheet, 455 calibré ancien projet

// Mesure tension voie (pont diviseur) : Vrail = Vadc * (R1 + R2) / R2
static constexpr uint32_t CFG_VRAIL_R1_OHMS = 68000; // haut
static constexpr uint32_t CFG_VRAIL_R2_OHMS = 10000; // bas

// --- Protection globale (reprise de l'ancien projet « Gestion Canton 2026 ») ---
// Coupe si un autre booster signale un défaut (état FAULT/OVERHEAT dans son 0x102)
static const bool CFG_ENABLE_GLOBAL_PROTECTION = true;
// Honore le cutout global (flag « global » du 0x101) ; false = l'ignore
static const bool CFG_ENABLE_GLOBAL_CUTOUT = true;
// Fenêtre de validité d'un défaut distant (les 0x102 arrivent toutes les 1 s)
static const uint32_t CFG_GLOBAL_FAULT_TIMEOUT_MS = 2000;

// --- Cutout RailCom régénéré ---
// Calé sur la centrale LaBox (Locoduino) : TCS = 29 µs + TCE = 459 µs = 488 µs
// (max NMRA S-9.3.2 ; 454 µs resterait conforme mais 488 = identique à l'amont).
static const uint32_t CFG_CUTOUT_US = 488;

// --- LED d'état ---
static const uint8_t CFG_WS2812_BRIGHTNESS = 40; // Luminosité (0-255)

// --- Digipot (MCP4551/4561) ---
static constexpr uint8_t DIGIPOT_ADDR = 0x2E; // Adresse I2C du digipot
static constexpr uint8_t DIGIPOT_REG  = 0x00; // Registre du wiper
static constexpr uint8_t DIGIPOT_HO   = 23;   // Profil HO (15V) — à ajuster après calibration
static constexpr uint8_t DIGIPOT_N    = 30;   // Profil N (12V)  — à ajuster après calibration