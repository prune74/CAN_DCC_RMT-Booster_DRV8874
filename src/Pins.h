#pragma once

// ======================================================================================
// Pins — Définition des broches matérielles (ESP32)
// Tout ce qui est brochage est ICI, nulle part ailleurs.
// ======================================================================================

// --- Identité sur le bus (trame 0x102) ---
// (Les IDs sont dans Config.h)

// --- Bus CAN (identique au convertisseur) ---
static const int PIN_CAN_TX = 5;     // TWAI natif
static const int PIN_CAN_RX = 4;     // TWAI natif

// --- DRV8874 ---
static const int PIN_DCC_PH = 32;    // Signal DCC (RMT) → PH (PMODE = 0)
static const int PIN_DRV_EN = 33;    // Autorisation sortie (0 = coast/arrêt)
static const int PIN_DRV_NSLEEP = 27; // Veille driver (1 = actif)
static const int PIN_IPROPI = 34;    // ADC IPROPI (courant voie)
static const int PIN_VRAIL = 35;     // ADC pont diviseur (tension voie)

// --- RailCom HF (lecture sur les rails pendant le cutout) ---
// Broche ADC1 OBLIGATOIRE (la capture DMA I2S ne fonctionne que sur l'ADC1 : GPIO 32..39)
// Signal RailCom conditionné (opto + RC).
static const int PIN_RAILCOM_ADC = 36; // ADC1_CH0

// --- RMT ---
static const int RMT_DCC_CHANNEL = 5;    // TX DCC (distinct du WS2812)
static const int RMT_WS2812_CHANNEL = 7;  // Défaut Ws2812Universal

// --- LED d'état ---
static const int PIN_WS2812 = 26;   // Strip 3 LED (comme convertisseur)
static const int WS2812_COUNT = 3;

// --- Digipot (MCP4551/4561) ---
static const int PIN_I2C_SDA = 21;   // I2C SDA
static const int PIN_I2C_SCL = 22;   // I2C SCL   // Nombre de LEDs
