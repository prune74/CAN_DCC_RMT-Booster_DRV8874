#include "Drv8874.h"

void Drv8874::begin() {
    pinMode(PIN_DRV_EN, OUTPUT);
    pinMode(PIN_DRV_NSLEEP, OUTPUT);
    digitalWrite(PIN_DRV_EN, LOW);      // sortie coupée au boot
    digitalWrite(PIN_DRV_NSLEEP, HIGH);
    analogReadResolution(12);
    // Atténuation 11 dB → pleine échelle ~3.3 V (IPROPI, pont voie)
    analogSetPinAttenuation(PIN_IPROPI, ADC_11db);
    analogSetPinAttenuation(PIN_VRAIL, ADC_11db);
    enabled = false;
}

void Drv8874::enableOutput()  { digitalWrite(PIN_DRV_EN, HIGH); enabled = true; }
void Drv8874::disableOutput() { digitalWrite(PIN_DRV_EN, LOW);  enabled = false; }
bool Drv8874::isEnabled() const { return enabled; }

// TODO calibration : IPROPI du DRV8874 : I_charge = V_IPROPI / RIPROPI / AIPR.
// Régler RIPROPI pour que le seuil de coupure (ILIM) corresponde au réseau CFC.
uint16_t Drv8874::readCurrent_mA() {
    const uint32_t adc = analogRead(PIN_IPROPI);
    return (uint16_t)((adc * 3300UL) / 4095UL); // mV IPROPI → échelle mA à calibrer
}

// TODO calibration : pont diviseur voie (ex. 10k/1k → ×11)
uint16_t Drv8874::readVoltage_mV() {
    const uint32_t adc = analogRead(PIN_VRAIL);
    return (uint16_t)((adc * 3300UL * 11UL) / 4095UL);
}
