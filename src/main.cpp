#include <Arduino.h>
#include "Config.h"
#include "BoosterCore.h"
#include "Telemetry.h"
#include "StatusLeds.h"
#include "CanBooster.h"

BoosterCore  booster;
CanBooster   canLink;
StatusLeds   leds;
Telemetry    telemetry(booster, &CanBooster::sendFrame);

void setup() {
    Serial.begin(115200);
    Serial.printf("CAN_DCC_RMT-BoosterDRV8874 — ID_B_%02d (CFC)\n", CFG_BOOSTER_ID);

    booster.begin();
    canLink.begin(booster, leds);
    leds.begin();
}

void loop() {
    canLink.poll();       // pompe le CAN → décodage 0x100/0x101
    booster.update();     // failsafe + sécurité + cutout + flux RMT
    telemetry.update();   // trame 0x102 toutes les 1 s
    leds.update(booster); // WS2812 non bloquant
}