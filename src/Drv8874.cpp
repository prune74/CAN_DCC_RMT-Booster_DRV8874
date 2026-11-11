#include "Drv8874.h"
#include "soc/gpio_sig_map.h"   // RMT_SIG_OUT0_IDX

// 0x100 = SIG_GPIO_OUT (constante locale, déconnecte la pin du périphérique)
#define GPIO_SIG_DETACH 0x100

void Drv8874::begin() {
    pinMode(PIN_DRV_NSLEEP, OUTPUT);
    digitalWrite(PIN_DRV_NSLEEP, HIGH);   // lève nSLEEP : PMODE (PWM) est latché ici
    pinMode(PIN_DRV_IN1, OUTPUT);         // GPIO 32
    pinMode(PIN_DRV_IN2, OUTPUT);         // GPIO 33
    coast();                              // IN1=IN2=0 -> Hi-Z jusqu'à connectDcc
}

void Drv8874::connectDcc(rmt_channel_t channel) {
    ch = channel;
    // IN1 : signal RMT direct. IN2 : MÊME signal, inversé par la matrice
    // (drapeau inv=true, comme railcom_invpins de la Labox).
    gpio_matrix_out((gpio_num_t)PIN_DRV_IN1, RMT_SIG_OUT0_IDX + ch, false, false);
    gpio_matrix_out((gpio_num_t)PIN_DRV_IN2, RMT_SIG_OUT0_IDX + ch, true, false);
    connected = true;
    braking = false;
}

void Drv8874::cutoutStart() {
    // Déconnecte les deux IN du RMT, force 1/1 = BRAKE (séquence timer_isr_CutOut
    // de la Labox, ici en task car déclenchée par l'événement CAN 0x101).
    gpio_matrix_out((gpio_num_t)PIN_DRV_IN1, GPIO_SIG_DETACH, false, false);
    gpio_matrix_out((gpio_num_t)PIN_DRV_IN2, GPIO_SIG_DETACH, false, false);
    gpio_set_level((gpio_num_t)PIN_DRV_IN1, 1);
    gpio_set_level((gpio_num_t)PIN_DRV_IN2, 1);
    braking = true;
}

void Drv8874::cutoutEnd() {
    if (ch != RMT_CHANNEL_MAX) connectDcc(ch);   // rebranche le biphasé
}

void Drv8874::enableOutput() {
    // Le BoosterCore appelle connectDcc() après DccRegen::begin().
    if (!connected && ch != RMT_CHANNEL_MAX) connectDcc(ch);
}

void Drv8874::disableOutput() { coast(); }

void Drv8874::coast() {
    gpio_matrix_out((gpio_num_t)PIN_DRV_IN1, GPIO_SIG_DETACH, false, false);
    gpio_matrix_out((gpio_num_t)PIN_DRV_IN2, GPIO_SIG_DETACH, false, false);
    gpio_set_level((gpio_num_t)PIN_DRV_IN1, 0);
    gpio_set_level((gpio_num_t)PIN_DRV_IN2, 0);
    connected = false;
    braking = false;
}

bool Drv8874::isEnabled() const {
    return connected && !braking;
}

uint16_t Drv8874::readCurrent_mA() {
    // IPROPI (µA) = (ILS1 + ILS2)(A) * 450 ; V = IPROPI * RIPROPI
    // => I(mA) = V_mesuré(mV) * 1000 / (RIPROPI_ohms * 0.45)
    uint32_t mv = analogReadMilliVolts(PIN_DRV_IPROPI);
    uint32_t ma = (mv * 1000UL) / ((uint32_t)CFG_RIPROPI_OHMS * 450UL / 1000UL);
    return (uint16_t)(ma > 65000 ? 65000 : ma);
}

uint16_t Drv8874::readVoltage_mV() {
    return analogReadMilliVolts(PIN_VRAIL);
}