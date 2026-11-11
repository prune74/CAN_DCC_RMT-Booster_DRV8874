#pragma once
#include "Config.h"
#include "Pins.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/rmt.h"

// DRV8874 en mode PWM (PMODE strapé à 3,3 V) :
//   IN1 = DCC RMT (canal 5), IN2 = même signal INVERSÉ via la matrice GPIO.
//   Cutout  : IN1=IN2=1 -> BRAKE (rails au potentiel commun, basse impédance).
//   Failsafe/défaut : IN1=IN2=0 -> COAST (sorties Hi-Z).
class Drv8874 {
public:
    void begin();
    void connectDcc(rmt_channel_t ch);   // RMT -> IN1, RMT inversé -> IN2
    void cutoutStart();                  // les deux IN à 1 = BRAKE
    void cutoutEnd();                    // rebranche le RMT (biphasé)
    void enableOutput();                 // rebranche le RMT si nécessaire
    void disableOutput();                // coast = Hi-Z (failsafe, FAULT)
    bool isEnabled() const;             // true si sortie active (non coast/brake)
    bool isBrake() const { return braking; }

    // IPROPI : I = V(ipropi) / (RIPROPI * 450 µA/A)  -- datasheet SLVSF66A eq.1
    uint16_t readCurrent_mA();
    uint16_t readVoltage_mV();           // Lecture tension voie via pont diviseur

private:
    void coast();
    bool connected = false;
    bool braking = false;
    rmt_channel_t ch = RMT_CHANNEL_MAX;
};