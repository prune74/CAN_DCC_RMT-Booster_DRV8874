#include "BoosterCore.h"
#include <CfcDccProtocol.h>

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

bool BoosterCore::setDccReference(const uint8_t *data, uint8_t len)
{
    dccRefReceived = true;
    dccRefValid = false;
    if (data == nullptr || len != cfc::dcc::SERVICE_DLC)
    {
        invalidDccReferenceFrames++;
        return false;
    }

    dccRefSeq = static_cast<uint16_t>(data[1]) |
                (static_cast<uint16_t>(data[2]) << 8);
    const uint8_t flags = data[3];
    const uint8_t allowedFlags = cfc::dcc::PROFILE_FLAG_VALID |
                                 cfc::dcc::PROFILE_FLAG_MAIN_ON;
    dccRefMainOn = (flags & cfc::dcc::PROFILE_FLAG_MAIN_ON) != 0;

    const bool valid = data[0] == cfc::dcc::PROTOCOL_VERSION &&
        (flags & static_cast<uint8_t>(~allowedFlags)) == 0 &&
        (flags & cfc::dcc::PROFILE_FLAG_VALID) != 0 &&
        data[4] != 0 && data[5] != 0 && data[4] < data[5] &&
        data[6] == 1 && data[7] != 0;
    if (!valid)
    {
        invalidDccReferenceFrames++;
        return false;
    }

    dccRefHalfBit1Us = data[4];
    dccRefHalfBit0Us = data[5];
    dccRefTickUs = data[6];
    dccRefPreambleBits = data[7];
    dccRefValid = true;
    lastValidDccReferenceMs = millis();
    validDccReferenceFrames++;
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

    // La référence et MAIN proviennent du 0xB7 relayé par UART, pas du CAN
    // Booster. Une référence absente, invalide ou périmée interdit la sortie.
    if (!dccReferenceFresh())
    {
        shutdown(BoosterState::FAULT);
        return;
    }
    if (!dccRefMainOn)
    {
        shutdown(BoosterState::OFF);
        return;
    }

    bool profileReadyForEmission = true;
    if (!profileApplied || appliedBit1Us != dccRefHalfBit1Us ||
        appliedBit0Us != dccRefHalfBit0Us)
    {
        // Les éléments déjà préparés dans le RMT gardent leur ancien timing.
        // Attendre leur fin avant de charger une nouvelle durée de demi-bit.
        if (drv.isEnabled())
            shutdown(BoosterState::OFF);
        if (regen.busy())
            profileReadyForEmission = false;
        else if (!regen.setHalfBitUs(dccRefHalfBit1Us, dccRefHalfBit0Us))
        {
            profileReadyForEmission = false;
            dccRefValid = false;
            invalidDccReferenceFrames++;
            shutdown(BoosterState::FAULT);
            return;
        }
        else
        {
            profileApplied = true;
            appliedBit1Us = dccRefHalfBit1Us;
            appliedBit0Us = dccRefHalfBit0Us;
            appliedProfileCount++;
        }
    }
    appliedProfileSeq = dccRefSeq;

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
        telemetry.voltage_mV < CFG_MIN_SUPPLY_VOLTAGE_MV)
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

    if (!profileReadyForEmission)
        return;

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
            // Reproduire l'intervalle reçu; le DLC 2 historique utilise le repli.
            const uint32_t intervalUs = event.hasMeasuredCutoutInterval
                ? event.cutoutIntervalUs : CFG_CUTOUT_US;
            if (intervalUs < CFG_CUTOUT_MIN_US || intervalUs > CFG_CUTOUT_MAX_US) {
                rejectedCutoutRangeCount++;
                return; // ne pas lancer BRAKE avec une durée absente ou aberrante
            }
            drv.cutoutStart();
            telemetry.railcomAddress = railcom.capture(intervalUs);
            drv.cutoutEnd();
            appliedCutoutCount++;
            lastAppliedCutoutUs = static_cast<uint16_t>(intervalUs);
            lastCutoutMeasured = event.hasMeasuredCutoutInterval;
        } else {
            skippedCutoutCount++;
        }
    }
}

void BoosterCore::sampleSensors()
{
    telemetry.current_mA = drv.readCurrent_mA();
    telemetry.voltage_mV = drv.readSupplyVoltage_mV();
}

const BoosterTelemetry &BoosterCore::getTelemetry() const { return telemetry; }
DccCanRx &BoosterCore::getCanRx() { return canRx; }
Drv8874 &BoosterCore::getDrv() { return drv; }
bool BoosterCore::hasAppliedDccProfile() const noexcept { return profileApplied; }
uint16_t BoosterCore::appliedProfileSequence() const noexcept { return appliedProfileSeq; }
bool BoosterCore::hasDccReference() const noexcept { return dccRefReceived; }
bool BoosterCore::dccReferenceValid() const noexcept { return dccRefValid; }
bool BoosterCore::dccReferenceFresh() const noexcept
{
    return dccRefReceived && dccRefValid &&
           millis() - lastValidDccReferenceMs < cfc::dcc::REFERENCE_STALE_MS;
}
bool BoosterCore::dccReferenceMainOn() const noexcept { return dccRefMainOn; }
uint16_t BoosterCore::dccReferenceSequence() const noexcept { return dccRefSeq; }
uint32_t BoosterCore::dccReferenceAgeMs() const noexcept
{
    return validDccReferenceFrames > 0
        ? millis() - lastValidDccReferenceMs
        : 0;
}
uint8_t BoosterCore::dccReferenceHalfBit1Us() const noexcept { return dccRefHalfBit1Us; }
uint8_t BoosterCore::dccReferenceHalfBit0Us() const noexcept { return dccRefHalfBit0Us; }
uint32_t BoosterCore::dccReferenceValidFrames() const noexcept { return validDccReferenceFrames; }
uint32_t BoosterCore::dccReferenceInvalidFrames() const noexcept { return invalidDccReferenceFrames; }
uint8_t BoosterCore::appliedHalfBit1Us() const noexcept { return appliedBit1Us; }
uint8_t BoosterCore::appliedHalfBit0Us() const noexcept { return appliedBit0Us; }
uint32_t BoosterCore::profileApplications() const noexcept { return appliedProfileCount; }
uint32_t BoosterCore::cutoutsApplied() const noexcept { return appliedCutoutCount; }
uint32_t BoosterCore::cutoutsRejectedRange() const noexcept { return rejectedCutoutRangeCount; }
uint32_t BoosterCore::cutoutsSkipped() const noexcept { return skippedCutoutCount; }
uint16_t BoosterCore::lastCutoutAppliedUs() const noexcept { return lastAppliedCutoutUs; }
bool BoosterCore::lastCutoutWasMeasured() const noexcept { return lastCutoutMeasured; }
