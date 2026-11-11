#pragma once
#include <Arduino.h>
#include "Config.h"
#include "Types.h"

class RailCom {
public:
    static constexpr uint32_t SAMPLE_RATE_HZ = 2000000;
    static constexpr uint16_t NO_ADDRESS = 0;

    bool begin();
    uint16_t capture(uint32_t window_us);

private:
    void onCutoutStart();
    void feedSample(bool bit);
    uint16_t onCutoutEnd();
    bool readBit(); // Lecture d'un bit RailCom

    uint16_t sampleIndex = 0;
    uint8_t  ch1 = 0, ch2 = 0, bitIndex = 0, channel = 1;
    bool     startBitDetected = false;
    bool     valid = false;
};