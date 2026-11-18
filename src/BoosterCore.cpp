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

void BoosterCore::setPowerEnabled(bool enabled)
{
    powerEnabled = enabled && hasBoosterId();
    if (!powerEnabled)
    {
        if (telemetry.state == BoosterState::FAULT)
            drv.disableOutput();
        else
            shutdown(BoosterState::OFF);
    }
}

bool BoosterCore::setBoosterId(uint8_t id)
{
    if (id == CFG_CANTON_ID_UNASSIGNED)
    {
        if (hasBoosterId() || powerEnabled)
            setPowerEnabled(false);
        assignedBoosterId = CFG_CANTON_ID_UNASSIGNED;
        return true;
    }

    if (id > CFG_MAX_CANTON_ID)
        return false;

    if (!hasBoosterId() || assignedBoosterId != id)
    {
        setPowerEnabled(false);
        assignedBoosterId = id;
    }
    return true;
}

bool BoosterCore::isPowerEnabled() const noexcept
{
    return powerEnabled;
}

bool BoosterCore::hasBoosterId() const noexcept
{
    return assignedBoosterId != CFG_CANTON_ID_UNASSIGNED;
}

uint8_t BoosterCore::boosterId() const noexcept
{
    return assignedBoosterId;
}

void BoosterCore::onCanMessage(uint16_t id, const uint8_t *data, uint8_t len)
{
    canRx.onCanMessage(id, data, len);
}

void BoosterCore::update()
{
    if (!hasBoosterId())
    {
        if (drv.isEnabled())
            shutdown(BoosterState::OFF);
        return;
    }

    if (!powerEnabled)
    {
        if (drv.isEnabled())
            shutdown(BoosterState::OFF);
        return;
    }

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

    // --- 2. Sécurité locale (surintensité / sous-tension) ---
    if (telemetry.current_mA > CFG_MAX_CURRENT_MA ||
        telemetry.voltage_mV < CFG_MIN_VOLTAGE_MV)
    {
        if (telemetry.state != BoosterState::FAULT)
            localFaultSinceMs = millis();
        shutdown(BoosterState::FAULT);
        return;
    }

    // Laisser le booster en sécurité pendant 100 ms avant de réessayer.
    // Si les mesures restent dans les seuils, effacer FAULT et reconnecter
    // la sortie au prochain bloc DCC.
    if (telemetry.state == BoosterState::FAULT)
    {
        if (millis() - localFaultSinceMs < CFG_FAULT_RETRY_DELAY_MS)
            return;
        telemetry.state = BoosterState::OK;
    }

    // --- 3. Flux DCC et cutout, traités dans l'ordre CAN ---
    uint8_t bits[32];
    size_t n = 0;
    DccCanEvent event;
    while (n < 32 && canRx.peekEvent(event) &&
           event.type == DccCanEventType::BIT)
    {
        canRx.popEvent(event);
        bits[n++] = event.encoded & 0x01; // la phase n'affecte pas la valeur du bit
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
        return; // traiter l'événement suivant au prochain tour
    }

    if (canRx.peekEvent(event) && event.type == DccCanEventType::CUTOUT)
    {
        // Ne commence le cutout qu'après émission effective de tous les bits
        // qui le précèdent dans le flux CAN.
        if (regen.busy())
            return;

        canRx.popEvent(event);
        if (telemetry.state != BoosterState::FAULT &&
            (CFG_ENABLE_GLOBAL_CUTOUT || !event.global))
        {
            drv.cutoutStart();
            telemetry.railcomAddress = railcom.capture(CFG_CUTOUT_US);
            drv.cutoutEnd();
        }
    }
}

void BoosterCore::sampleSensors()
{
    telemetry.current_mA = drv.readCurrent_mA();
    telemetry.voltage_mV = drv.readVoltage_mV();
}

const BoosterTelemetry &BoosterCore::getTelemetry() const { return telemetry; }
DccCanRx &BoosterCore::getCanRx() { return canRx; }
Drv8874 &BoosterCore::getDrv() { return drv; }
