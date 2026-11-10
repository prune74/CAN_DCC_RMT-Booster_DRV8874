#pragma once
#include "DccCanRx.h"
#include "DccRegen.h"
#include "Config.h"
#include "Drv8874.h"
#include "RailCom.h"

// ============================================================
// BoosterCore — orchestration (adapté du Core de CAN_DCC-Booster
// au protocole blocs de DCC_CAN_RMT-Booster)
//
//  update() — appelé en boucle, jamais bloquant :
//    1. failsafe (watchdog blocs 0x100)
//    2. sécurité (courant, tension)
//    3. cutout RailCom régénéré
//    4. flux bits FIFO → RMT
// ============================================================

class BoosterCore {
public:
    void begin() {
        regen.begin();
        drv.begin();
        railcom.begin();
        canRx.begin();
        telemetry.state = BoosterState::OFF;
    }

    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len) {
        canRx.onCanMessage(id, data, len);
    }

    void update() {
        const uint32_t now = millis();

        // --- 1. Failsafe : DCC perdu → coupe immédiate ---
        if (!canRx.dccAlive()) {
            if (drv.isEnabled()) {
                drv.disableOutput();
                telemetry.state = BoosterState::OFF;
                // TODO : trame d'événement immédiate (hors télémétrie 1 s)
            }
            return;
        }

        // --- 2. Sécurité (surintensité / sous-tension) ---
        telemetry.current_mA = drv.readCurrent_mA();
        telemetry.voltage_mV = drv.readVoltage_mV();
        if (telemetry.current_mA > CFG_MAX_CURRENT_MA ||
            telemetry.voltage_mV < CFG_MIN_VOLTAGE_MV) {
            drv.disableOutput();
            telemetry.state = BoosterState::FAULT;
            return;
        }

        // --- 3. Cutout RailCom (0x101) : EN=0 + capture HF 454 µs ---
        if (canRx.popCutout() && telemetry.state != BoosterState::FAULT) {
            const bool wasOn = drv.isEnabled();
            drv.disableOutput(); // rails silencieux, PH inchangé (le RMT continue)
            telemetry.railcomAddress = railcom.capture(CFG_CUTOUT_US);
            if (wasOn) drv.enableOutput(); // rétablissement EN
        }

        // --- 4. Flux DCC : FIFO → RMT, par paquets de 32 bits ---
        uint8_t bits[32];
        size_t  n = 0;
        uint8_t encoded;
        while (n < 32 && canRx.popBit(encoded)) {
            bits[n++] = encoded & 0x01; // bit DCC (la phase sert à l'alignement amont)
        }
        if (n > 0) {
            if (!drv.isEnabled()) drv.enableOutput();
            if (regen.emit(bits, n) == 0) {
                // Ring buffer plein : on retentera au prochain update()
                // (les bits perdus sont dans la FIFO… les remettre = v2)
            }
            if (telemetry.state != BoosterState::FAULT)
                telemetry.state = BoosterState::OK;
            lastDccMs = now;
        }
    }

    const BoosterTelemetry &getTelemetry() const { return telemetry; }
    DccCanRx &getCanRx() { return canRx; }
    Drv8874  &getDrv()   { return drv; }

private:
    DccCanRx        canRx;
    DccRegen        regen;
    Drv8874         drv;
    RailCom         railcom;
    BoosterTelemetry telemetry;
    uint32_t        lastDccMs = 0;
};