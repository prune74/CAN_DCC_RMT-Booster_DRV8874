#include "StatusLeds.h"

// ============================================================
// Implémentation BoosterWs2812Config
// ============================================================

BoosterWs2812Config::BoosterWs2812Config()
    : cfg(true, (gpio_num_t)PIN_WS2812, (uint16_t)WS2812_COUNT,
          (rmt_channel_t)RMT_WS2812_CHANNEL, // canal 7 ≠ canal 5 du DCC
          CFG_WS2812_BRIGHTNESS)
{}

uint8_t BoosterWs2812Config::busCount() const { return 1; }
const Ws2812BusConfig &BoosterWs2812Config::busConfig(uint8_t) const { return cfg; }

// ============================================================
// Implémentation StatusLeds
// ============================================================

void StatusLeds::begin() {
    Ws2812Init::begin(provider);      // tolérant aux erreurs (architecture CFC)
    bus = &Ws2812Bus::bus(0);        // jamais NULL ; isValid() à vérifier
}

void StatusLeds::update(BoosterCore &core) {
    if (bus == nullptr || !bus->isValid()) return;

    const BoosterTelemetry &t = core.getTelemetry();
    const uint32_t now = millis();

    // LED 1 — flux DCC
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

void StatusLeds::noteCanActivity(uint32_t ms) { lastCanRxMs = ms; }
