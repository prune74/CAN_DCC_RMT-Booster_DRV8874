#include "DccRegen.h"

bool DccRegen::begin() {
    rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX((gpio_num_t)PIN_DCC_PH,
                                             (rmt_channel_t)RMT_DCC_CHANNEL);
    cfg.clk_div = 2;              // tick 25 ns
    cfg.mem_block_num = 2;        // 128 items de RAM RMT dédiés au DCC
    if (rmt_config(&cfg) != ESP_OK) return false;
    // Ring buffer 4096 octets = 1024 items = ~512 bits DCC d'avance
    if (rmt_driver_install(cfg.channel, 4096, 0) != ESP_OK) return false;
    return true;
}

bool DccRegen::busy() {
    return rmt_wait_tx_done((rmt_channel_t)RMT_DCC_CHANNEL, 0) == ESP_ERR_TIMEOUT;
}

// Ajoute des bits DCC au flux (non bloquant).
// bits[i] : 0/1 = valeur du bit complet.
// Retourne le nombre de bits acceptés (0 si le ring buffer est plein).
size_t DccRegen::emit(const uint8_t *bits, size_t n) {
    rmt_item32_t items[MAX_EMIT];
    size_t k = 0;
    if (n > MAX_EMIT) n = MAX_EMIT;   // 32 bits max par appel (cf. ring buffer)
    for (size_t i = 0; i < n; i++) {
        const uint16_t t = bits[i] ? TICKS_HALF_ONE : TICKS_HALF_ZER;
        items[k].level0 = 0; items[k].duration0 = t;
        items[k].level1 = 1; items[k].duration1 = t;
        k++;
    }
    return (rmt_write_items((rmt_channel_t)RMT_DCC_CHANNEL, items, k, false) == ESP_OK)
           ? n : 0;
}
