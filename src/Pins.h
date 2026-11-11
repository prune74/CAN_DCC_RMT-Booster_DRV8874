#pragma once

// ======================================================================================
// Pins — Définition des broches matérielles (ESP32)
// Tout ce qui est brochage est ICI, nulle part ailleurs.
// ======================================================================================

// --- Bus CAN (identique au convertisseur) ---
static const int PIN_CAN_TX = 5;     // TWAI natif
static const int PIN_CAN_RX = 4;     // TWAI natif

// --- DRV8874 (mode PWM, PMODE strapé à 3,3 V) ---
static const int PIN_DRV_IN1     = 32;    // DCC RMT canal 5 → IN1
static const int PIN_DRV_IN2     = 33;    // même signal, inversé via matrice GPIO → IN2
static const int PIN_DRV_NSLEEP = 27;     // Veille driver (1 = actif)
static const int PIN_DRV_IPROPI = 34;     // ADC IPROPI (courant voie) via R 3,6 kΩ → GND
static const int PIN_VRAIL      = 35;     // ADC pont diviseur 68 kΩ / 10 kΩ (tension voie)

// --- RailCom (sortie logique isolée du front-end carte) ---
static const int PIN_RAILCOM_ISO = 36;    // UART2 RX via matrice (input-only OK)

// --- RMT ---
static const int RMT_DCC_CHANNEL   = 5;   // TX DCC (distinct du WS2812)
static const int RMT_WS2812_CHANNEL = 7;  // Défaut Ws2812Universal

// --- LED d'état ---
static const int PIN_WS2812   = 26;   // Strip 3 LED (comme convertisseur)
static const int WS2812_COUNT = 3;

// --- Digipot (MCP4551/4561) ---
static const int PIN_I2C_SDA = 21;   // I2C SDA
static const int PIN_I2C_SCL = 22;   // I2C SCL