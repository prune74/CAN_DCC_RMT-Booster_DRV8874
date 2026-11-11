#include "BoosterCore.h"

void BoosterCore::begin()
{
    regen.begin();
    drv.begin();
    railcom.begin();
    canRx.begin();
    tensionControl.begin(); // Initialise le digipot (profil par défaut)
    telemetry.state = BoosterState::OFF;
}

void BoosterCore::shutdown(BoosterState state)
{
    drv.disableOutput();     // coast = Hi-Z
    telemetry.state = state;
}

// Gestion du profil de tension (HO/N)
void BoosterCore::setTrackProfile(bool ho) noexcept
{
    tensionControl.setTrackProfile(ho);
}

void BoosterCore::onCanMessage(uint16_t id, const uint8_t *data, uint8_t len)
{
    canRx.onCanMessage(id, data, len);
}

void BoosterCore::update()
{
    // --- 1. Failsafe : DCC perdu → coupe immédiate ---
    if (!canRx.dccAlive())
    {
        if (drv.isEnabled())
        {
            shutdown(BoosterState::OFF);
            // TODO : trame d'événement immédiate (hors télémétrie 1 s)
        }
        return;
    }

    // --- 1b. Protection globale : un autre booster en défaut → coupe ---
    if (CFG_ENABLE_GLOBAL_PROTECTION && canRx.otherBoosterFault())
    {
        if (drv.isEnabled())
            shutdown(BoosterState::OFF);
        return;
    }

    // --- 2. Sécurité locale (surintensité / sous-tension) ---
    telemetry.current_mA = drv.readCurrent_mA();
    telemetry.voltage_mV = drv.readVoltage_mV();
    if (telemetry.current_mA > CFG_MAX_CURRENT_MA ||
        telemetry.voltage_mV < CFG_MIN_VOLTAGE_MV)
    {
        shutdown(BoosterState::FAULT);
        return;
    }

    // --- 3. Cutout RailCom (0x101) : BRAKE + capture UART ~488 µs ---
    bool global = false;
    if (canRx.popCutout(global) && telemetry.state != BoosterState::FAULT)
    {
        // Option GLOBAL_CUTOUT : honorer/ignorer le cutout global
        if (CFG_ENABLE_GLOBAL_CUTOUT || !global)
        {
            drv.cutoutStart();                                         // BRAKE : rails communs
            telemetry.railcomAddress = railcom.capture(CFG_CUTOUT_US);  // UART pendant ~488 µs
            drv.cutoutEnd();                                           // rebranchement RMT
        }
    }

    // --- 4. Flux DCC : FIFO → RMT, par paquets de 32 bits ---
    uint8_t bits[32];
    size_t n = 0;
    uint8_t encoded;
    while (n < 32 && canRx.popBit(encoded))
    {
        bits[n++] = encoded & 0x01; // bit DCC (la phase sert à l'alignement amont)
    }
    if (n > 0)
    {
        if (!drv.isEnabled())
            drv.enableOutput();

        if (regen.emit(bits, n) == 0)
        {
            // Ring buffer plein : on retentera au prochain update()
            // (les bits perdus sont dans la FIFO… les remettre = v2)
        }
        if (telemetry.state != BoosterState::FAULT)
            telemetry.state = BoosterState::OK;
    }
}

const BoosterTelemetry &BoosterCore::getTelemetry() const { return telemetry; }
DccCanRx &BoosterCore::getCanRx() { return canRx; }
Drv8874 &BoosterCore::getDrv() { return drv; }