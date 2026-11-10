#pragma once
#include <Arduino.h>
#include "driver/i2s.h"
#include "esp32-hal-adc.h"
#include "Config.h"
#include "Types.h"

// ============================================================
// RailCom — lecture HF sur les rails pendant le cutout
// (obligatoire Discovery 2026 — portage du décodeur de
//  prune74/CAN_DCC-Booster, adapté à la capture matérielle)
//
//  Chaîne : pendant les 454 µs de cutout (EN=0, rails silencieux),
//  l'ADC1 est capté en DMA I2S à 2 MHz (8 échantillons/bit à
//  250 kbaud), puis le décodeur rejoue le buffer :
//    - détection du start bit (front descendant sous le seuil)
//    - CH1 (8 bits) : adresse courte (1..127)
//    - CH2 (8 bits) : extension — adresse longue = (CH2 << 7) | CH1
//    - validation + reconstruction de l'adresse
//
//  Le signal RailCom doit être conditionné (opto/condo + RC) vers
//  CFG_PIN_RAILCOM_ADC — broche ADC1 OBLIGATOIRE (la capture DMA
//  I2S n'accepte que l'ADC1 : GPIO 32..39 ; les GPIO 34/35 sont
//  déjà pris par IPROPI et la tension voie).
//
//  Pourquoi le DMA : 250 kbaud → 4 µs/bit. Une boucle analogRead
//  (~10 µs) ne peut pas suivre ; seul l'ADC en DMA I2S tient le
//  rythme. La capture est bloquante (~454 µs, une fois par trame
//  DCC ≈ 30 ms → ~1,5 % CPU) : sans conséquence, le créneau DCC
//  continue d'être émis par le RMT en matériel pendant ce temps.
// ============================================================

class RailCom {
public:
    static constexpr uint32_t SAMPLE_RATE_HZ = 2000000; // 8 échantillons/bit
    static constexpr uint16_t RAILCOM_BAUD   = 250000;
    static constexpr uint16_t NO_ADDRESS     = 0;

    bool begin() {
        i2s_config_t cfg = {};
        cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX |
                                                I2S_MODE_ADC_BUILT_IN);
        cfg.sample_rate          = SAMPLE_RATE_HZ;
        cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
        cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
        cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        cfg.dma_buf_count        = 2;
        cfg.dma_buf_len          = 512;
        cfg.use_apll             = false;
        cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
        if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;

        // Broche ADC1 (attention : digitalPinToADC1Channel — cf. esp32-hal-adc.h)
        const adc1_channel_t chan =
            (adc1_channel_t)(CFG_PIN_RAILCOM_ADC - 32);
        i2s_set_adc_mode(ADC_UNIT_1, chan);
        // i2s_adc_enable/disable : API « built-in » legacy d'arduino-esp32 2.0.x,
        // assumée comme le driver RMT legacy (même choix que Ws2812Universal)
        return true;
    }

    // Bloquant ~window_us : EN est déjà à 0 (BoosterCore), on capture
    // puis on décode. Retourne l'adresse RailCom (0 = rien détecté).
    uint16_t capture(uint32_t window_us) {
        const size_t nSamples = (window_us * SAMPLE_RATE_HZ) / 1000000UL; // 454 µs → 908
        static uint16_t buf[1024];
        size_t bytesRead = 0;

        onCutoutStart();
        i2s_adc_enable(I2S_NUM_0);
        i2s_read(I2S_NUM_0, buf, nSamples * sizeof(uint16_t),
                 &bytesRead, portMAX_DELAY);
        i2s_adc_disable(I2S_NUM_0);

        const size_t got = bytesRead / sizeof(uint16_t);
        for (size_t i = 0; i < got; i++) {
            // Échantillon centré : 12 bits bruts − zéro calibré
            const int16_t s = (int16_t)(buf[i] & 0x0FFF) - (int16_t)CFG_RAILCOM_ADC_ZERO;
            feedSample(s);
        }
        return onCutoutEnd();
    }

private:
    // --- Décodeur (portage de CanDccBoosterRailCom, CAN_DCC-Booster) ---
    static constexpr int16_t  THRESHOLD_START  = 50;   // front descendant du start bit
    static constexpr uint16_t SAMPLES_PER_BIT = 8;    // 2 MHz / 250 kbaud
    static constexpr uint16_t SAMPLE_CENTER   = 4;   // échantillon au centre du bit

    void onCutoutStart() {
        sampleIndex = 0; startBitDetected = false; valid = false;
        ch1 = 0; ch2 = 0; bitIndex = 0; channel = 1;
    }

    void feedSample(int16_t s) {
        if (!startBitDetected) {
            if (s < -THRESHOLD_START) {
                startBitDetected = true;
                bitIndex = 0; channel = 1;
            }
            sampleIndex++;
            return;
        }
        if (sampleIndex % SAMPLES_PER_BIT == SAMPLE_CENTER) {
            const bool bit = (s > 0);
            if (channel == 1)      ch1 |= (uint8_t)(bit << bitIndex);
            else if (channel == 2) ch2 |= (uint8_t)(bit << bitIndex);
            bitIndex++;
            if (bitIndex >= 8) {
                bitIndex = 0; channel++;
                if (channel > 2) valid = true;
            }
        }
        sampleIndex++;
    }

    uint16_t onCutoutEnd() {
        if (!startBitDetected || !valid) return NO_ADDRESS;
        if (ch1 == 0 || ch1 == 0xFF)    return NO_ADDRESS; // trame invalide
        if (ch2 == 0)                   return ch1;        // adresse courte
        const uint16_t addr = ((uint16_t)ch2 << 7) | ch1;  // longue Locoduino
        if (addr < 128 || addr > 10239) return NO_ADDRESS;
        return addr;
    }

    bool     startBitDetected = false;
    bool     valid = false;
    uint16_t sampleIndex = 0;
    uint8_t  ch1 = 0, ch2 = 0, bitIndex = 0, channel = 1;
};