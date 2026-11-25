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

// --- Bus CAN (identique au convertisseur) ---
static const uint32_t CFG_CAN_BITRATE = 500000; // 500 kbit/s

// --- Sécurité voie ---
static const uint32_t CFG_FAILSAFE_MS    = 50;    // v0.4.0 : aucun enregistrement 0x100 depuis 50 ms → OFF
static const uint16_t CFG_MAX_CURRENT_MA = 2000;  // Limite voie 2 A (RIPROPI 3,6 kΩ = pleine échelle ADC)
static const uint16_t CFG_MIN_SUPPLY_VOLTAGE_MV = 9000; // Sous-tension alimentation → FAULT
static constexpr uint32_t CFG_FAULT_RETRY_DELAY_MS = 100; // pause avant tentative locale

// Mesure courant via IPROPI (datasheet SLVSF66A eq.1/2) :
//   I = V_IPROPI / (R_IPROPI * AIPROPI)
static constexpr uint16_t CFG_RIPROPI_OHMS        = 3600; // R IPROPI→GND sur la carte
static constexpr uint16_t CFG_IPROPI_GAIN_UA_PER_A = 455; // 450 typ. datasheet, 455 calibré ancien projet

// Mesure de la sortie alimentation (pont diviseur), pas de la voie DCC.
// Vsupply = Vadc * (R1 + R2) / R2.
static constexpr uint32_t CFG_SUPPLY_DIV_R1_OHMS = 68000; // haut
static constexpr uint32_t CFG_SUPPLY_DIV_R2_OHMS = 10000; // bas

// Mesure de la tension différentielle des rails après le DRV8874.
// Le facteur 11 correspond au diviseur 100 kΩ / 10 kΩ retenu.
// Correction et validité restent à étalonner sur la carte construite.
static constexpr float CFG_RAIL_VOLTAGE_DIVIDER_RATIO = 11.0f;
static constexpr int32_t CFG_RAIL_VOLTAGE_CORRECTION_MV = 0;
static constexpr bool CFG_RAIL_VOLTAGE_CALIBRATED = false;
static constexpr uint16_t CFG_RAIL_VOLTAGE_SAMPLE_PERIOD_MS = 250;
static constexpr uint16_t CFG_RAIL_VOLTAGE_TELEMETRY_PERIOD_MS = 1000;

// Honore le cutout global (drapeau global de l'enregistrement 0x100).
static const bool CFG_ENABLE_GLOBAL_CUTOUT = true;

// --- Validation de la durée cutout reçue dans l'enregistrement 0x100 ---
static constexpr uint16_t CFG_CUTOUT_MIN_US = 300;
static constexpr uint16_t CFG_CUTOUT_MAX_US = 2000;

// --- LED d'état ---
static const uint8_t CFG_WS2812_BRIGHTNESS = 40; // Luminosité (0-255)

// --- Digipot MCP45HV51-103, identique à LaBox ---
static constexpr uint8_t DIGIPOT_ADDR = 0x3D; // A1=GND, A0=3V3, comme LaBox
static constexpr uint8_t DIGIPOT_WIPER0_ADDRESS = 0x00;
static constexpr uint8_t DIGIPOT_READ_WIPER0_COMMAND = 0x0C;
static constexpr uint16_t DIGIPOT_RAB_OHMS = 10000; // variante -103, 10 kΩ
static constexpr uint8_t DIGIPOT_HO = 23; // code de départ existant, à calibrer
static constexpr uint8_t DIGIPOT_N  = 30; // code de départ existant, à calibrer
