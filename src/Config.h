#pragma once
#include <cstdint>

// ======================================================================================
// CAN_DCC_RMT-BoosterDRV8874 — Configuration carte les Chemins de Fer du Chablais (CFC)
// Tout ce qui est matériel est ICI, nulle part ailleurs.
// ======================================================================================

// --- Identité sur le bus (trame 0x102) ---
static const uint8_t  CFG_BOOSTER_ID   = 1;     // ID_B_01 (1..32, unique sur le bus)
static const uint8_t  CFG_BOOSTER_TYPE = 1;     // TYPE B = booster (nommage ID_B_xx)

// --- Bus CAN (identique au convertisseur) ---
static const int      CFG_CAN_TX_PIN  = 5;     // TWAI natif
static const int      CFG_CAN_RX_PIN  = 4;
static const uint32_t CFG_CAN_BITRATE = 500000; // kbit/s → attention : à régler
                                                // côté CanUniversal (voir CanBooster.h)

// --- DRV8874 ---
static const int      CFG_PIN_DCC_PH     = 32;  // signal DCC (RMT) → PH (PMODE = 0)
static const int      CFG_PIN_DRV_EN     = 33;  // autorisation sortie (0 = coast/arrêt)
static const int      CFG_PIN_DRV_NSLEEP = 27;  // veille driver (1 = actif)
static const int      CFG_PIN_IPROPI     = 34; // ADC IPROPI (courant voie)
static const int      CFG_PIN_VRAIL      = 35;  // ADC pont diviseur (tension voie)

// --- RailCom HF (lecture sur les rails pendant le cutout) ---
// Broche ADC1 OBLIGATOIRE (la capture DMA I2S ne fonctionne que sur
// l'ADC1 : GPIO 32..39) ; signal RailCom conditionné (opto + RC).
static const int      CFG_PIN_RAILCOM_ADC  = 36;  // ADC1_CH0
static const uint16_t CFG_RAILCOM_ADC_ZERO = 2048; // zéro HF à calibrer (12 bits)

// --- RMT ---
static const int      CFG_RMT_DCC_CHANNEL   = 5; // TX DCC (distinct du WS2812)
static const int      CFG_RMT_WS2812_CHANNEL = 7; // défaut Ws2812Universal

// --- LED d'état ---
static const int      CFG_PIN_WS2812 = 26;      // strip 3 LED (comme convertisseur)
static const int      CFG_WS2812_COUNT = 3;
static const uint8_t  CFG_WS2812_BRIGHTNESS = 40;

// --- Sécurité ---
static const uint32_t CFG_FAILSAFE_MS   = 500;  // pas de bloc 0x100 depuis 500 ms → OFF
static const uint16_t CFG_MAX_CURRENT_MA = 2000; // seuil coupure (calibrer avec RIPROPI)
static const uint16_t CFG_MIN_VOLTAGE_MV = 9000;  // sous-tension → FAULT
static const uint32_t CFG_CUTOUT_US     = 454;  // cutout nominal régénéré (RailCom)