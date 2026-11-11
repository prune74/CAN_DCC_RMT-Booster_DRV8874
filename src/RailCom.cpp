#include "RailCom.h"

bool RailCom::begin() {
    pinMode(PIN_RAILCOM_ADC, INPUT_PULLUP); // Signal actif bas
    return true;
}

uint16_t RailCom::capture(uint32_t window_us) {
    const uint32_t endTime = millis() + window_us / 1000;
    onCutoutStart();

    while (millis() < endTime) {
        feedSample(digitalRead(PIN_RAILCOM_ADC) == LOW); // Actif bas
    }

    return onCutoutEnd();
}

void RailCom::feedSample(bool bit) {
    if (!startBitDetected) {
        if (!bit) { // Front descendant (start bit)
            startBitDetected = true;
            bitIndex = 0;
            channel = 1;
        }
        sampleIndex++;
        return;
    }

    // Échantillonnage au centre du bit (8 échantillons/bit @ 250 kHz)
    if (sampleIndex % 8 == 4) { // Centre = 4/8
        if (channel == 1) ch1 |= (uint8_t)(bit << bitIndex);
        else if (channel == 2) ch2 |= (uint8_t)(bit << bitIndex);
        bitIndex++;
        if (bitIndex >= 8) {
            bitIndex = 0;
            channel++;
            if (channel > 2) valid = true;
        }
    }
    sampleIndex++;
}

uint16_t RailCom::onCutoutEnd() {
    if (!startBitDetected || !valid) return NO_ADDRESS;
    if (ch1 == 0 || ch1 == 0xFF) return NO_ADDRESS;
    if (ch2 == 0) return ch1;
    const uint16_t addr = ((uint16_t)ch2 << 7) | ch1;
    if (addr < 128 || addr > 10239) return NO_ADDRESS;
    return addr;
}

void RailCom::onCutoutStart() {
    sampleIndex = 0;
    startBitDetected = false;
    valid = false;
    ch1 = 0; ch2 = 0; bitIndex = 0; channel = 1;
}