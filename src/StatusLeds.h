#pragma once
#include <Ws2812Universal.h>
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
    BoosterWs2812Config()
        : cfg(true, (gpio_num_t)CFG_PIN_WS2812, (uint16_t)CFG_WS2812_COUNT,
              (rmt_channel_t)CFG_RMT_WS2812_CHANNEL, // canal 7 ≠ canal 5 du DCC
              CFG_WS2812_BRIGHTNESS)
    {}

    uint8_t busCount() const override { return 1; }
    const Ws2812BusConfig &busConfig(uint8_t) const override { return cfg; }

private:
    Ws2812BusConfig cfg;
};

class StatusLeds {
public:
    void begin() {
        Ws2812Init::begin(provider);      // tolérant aux erreurs (architecture CFC)
        bus = &Ws2812Bus::bus(0);        // jamais NULL ; isValid() à vérifier
    }

    void update(BoosterCore &core) {
        if (bus == nullptr || !bus->isValid()) return;

        const BoosterTelemetry &t = core.getTelemetry();

        // LED 1 — flux DCC
        const uint32_t now = millis();
        if (core.getCanRx().dccAlive())
            bus->setPixel(0, (now % 1500 < 750) ? GREEN : BLACK);
        else
            bus->setPixel(0, (now % 2000 < 1000) ? RED : BLACK);

        // LED 2 — voie / défaut
        switch (t.state) {
            case BoosterState::OK:    bus->setPixel(1, GREEN);  break;
            case BoosterState::FAULT: bus->setPixel(1, RED);    break;
            default:                  bus->setPixel(1, BLACK);  break; // OFF / RETRY
        }

        // LED 3 — CAN (v1 : vert fixe si une trame a été vue depuis < 3 s)
        bus->setPixel(2, (now - lastCanRxMs < 3000) ? GREEN : BLACK);

        bus->show(); // non bloquant ; false si trame précédente en cours (ignoré)
    }

    void noteCanActivity(uint32_t ms) { lastCanRxMs = ms; }

private:
    static constexpr uint32_t GREEN = 0x00FF00, RED = 0xFF0000, BLACK = 0;
    BoosterWs2812Config provider;
    Ws2812Bus *bus = nullptr;
    uint32_t lastCanRxMs = 0;
};