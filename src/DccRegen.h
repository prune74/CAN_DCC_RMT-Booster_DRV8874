#pragma once
#include "driver/rmt.h"
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

    bool begin() {
        rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX((gpio_num_t)CFG_PIN_DCC_PH,
                                                 (rmt_channel_t)CFG_RMT_DCC_CHANNEL);
        cfg.clk_div = 2;              // tick 25 ns
        cfg.mem_block_num = 2;        // 128 items de RAM RMT dédiés au DCC
        if (rmt_config(&cfg) != ESP_OK) return false;
        // Ring buffer 4096 octets = 1024 items = ~512 bits DCC d'avance
        if (rmt_driver_install(cfg.channel, 4096, 0) != ESP_OK) return false;
        return true;
    }

    bool busy() {
        return rmt_wait_tx_done((rmt_channel_t)CFG_RMT_DCC_CHANNEL, 0) == ESP_ERR_TIMEOUT;
    }

    // Ajoute des bits DCC au flux (non bloquant).
    // bits[i] : 0/1 = valeur du bit complet.
    // Retourne le nombre de bits acceptés (0 si le ring buffer est plein).
    size_t emit(const uint8_t *bits, size_t n) {
        rmt_item32_t items[MAX_EMIT];
        size_t k = 0;
        if (n > MAX_EMIT) n = MAX_EMIT;   // 32 bits max par appel (cf. ring buffer)
        for (size_t i = 0; i < n; i++) {
            const uint16_t t = bits[i] ? TICKS_HALF_ONE : TICKS_HALF_ZER;
            items[k].level0 = 0; items[k].duration0 = t;
            items[k].level1 = 1; items[k].duration1 = t;
            k++;
        }
        return (rmt_write_items((rmt_channel_t)CFG_RMT_DCC_CHANNEL, items, k, false) == ESP_OK)
               ? n : 0;
    }

private:
    static constexpr size_t MAX_EMIT = 32; // 32 bits = 64 items par appel max
};