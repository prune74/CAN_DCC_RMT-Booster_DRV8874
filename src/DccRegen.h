#pragma once
#include "driver/rmt.h"
#include "Pins.h"
#include "Config.h"

// ============================================================
// DccRegen — régénération du créneau DCC par RMT (TX, canal CFG)
//
//  Driver RMT legacy (driver/rmt.h), clk_div = 2 → tick 25 ns,
//  comme Ws2812Universal. Émission non bloquante dans le ring
//  buffer du driver : le CPU ne cadence RIEN, jamais d'interruption
//  coupée pour le timing.
//
//  NMRA S-9.1 (timings nominaux régénérés — les demi-bits étirés
//  par le zéro-stretching amont sont normalisés ici) :
//    bit '1' : 2 × 58 µs  → 2320 ticks
//    bit '0' : 2 × 100 µs → 4000 ticks
//  PH/EN : chaque demi-bit s'inverse → biphasé exact sur PH.
// ============================================================

class DccRegen {
public:
    static constexpr uint16_t TICKS_HALF_ONE = 2320; // 58 µs @ 25 ns
    static constexpr uint16_t TICKS_HALF_ZER = 4000; // 100 µs @ 25 ns

    bool begin();
    bool busy();
    size_t emit(const uint8_t *bits, size_t n);

private:
    static constexpr size_t MAX_EMIT = 32; // 32 bits = 64 items par appel max
};
