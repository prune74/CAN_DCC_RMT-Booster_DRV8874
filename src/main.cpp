#include <Arduino.h>
#include "Config.h"
#include "BoosterCore.h"
#include "Telemetry.h"
#include "StatusLeds.h"
#include "CanBooster.h"
#include "BoosterUartLink.h"

BoosterCore  booster;
CanBooster   canLink;
StatusLeds   leds;
Telemetry    telemetry(booster, &CanBooster::sendFrame);
BoosterUartLink uartLink;

void setup() {
    Serial.begin(115200);
    Serial.println("CAN_DCC_RMT-BoosterDRV8874 — en attente de l'ID du CC (CFC)");

    booster.begin();
    canLink.begin(booster, leds);
    leds.begin();
    uartLink.begin();
}

void loop() {
    canLink.poll();       // pompe le CAN → décodage 0x100/0x101
    booster.update();     // failsafe + sécurité + cutout + flux RMT
    uartLink.update(booster); // commandes, heartbeat et mesures à 1 kHz
    telemetry.update();   // trame 0x102 toutes les 1 s
    leds.update(booster); // WS2812 non bloquant
}