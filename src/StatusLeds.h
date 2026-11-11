#pragma once
#include <Ws2812Universal.h>
#include "Pins.h"
#include "Config.h"
#include "BoosterCore.h"

// ============================================================
// StatusLeds — 3 LED WS2812 (strip, GPIO 26) via Ws2812Universal
// Sémantique propre au booster (3 rôles simultanés, comme le
// convertisseur — aucune priorité exclusive) :
//   LED 1 — flux DCC  : battement vert 1,5 s si blocs reçus ;
//                        rouge lent si failsafe (OFF)
//   LED 2 — voie     : vert fixe = EN actif ; rouge = FAULT ;
//                        éteint = OFF
//   LED 3 — CAN      : vert discret = bus valide (sniffer simple)
// V2 : étendre au style complet du convertisseur (cyan, orange…)
// ============================================================

class BoosterWs2812Config : public Ws2812ConfigProvider {
public:
    BoosterWs2812Config();
    uint8_t busCount() const override;
    const Ws2812BusConfig &busConfig(uint8_t) const override;

private:
    Ws2812BusConfig cfg;
};

class StatusLeds {
public:
    void begin();
    void update(BoosterCore &core);
    void noteCanActivity(uint32_t ms);

private:
    static constexpr uint32_t GREEN = 0x00FF00, RED = 0xFF0000, BLACK = 0;
    BoosterWs2812Config provider;
    Ws2812Bus *bus = nullptr;
    uint32_t lastCanRxMs = 0;
};
