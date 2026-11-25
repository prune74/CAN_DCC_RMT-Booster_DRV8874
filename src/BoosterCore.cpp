#include "BoosterCore.h"
#include <CfcDccProtocol.h>
#include <Preferences.h>

void BoosterCore::begin()
{
    Preferences preferences;
    if (preferences.begin("cfc-booster", true))
    {
        const uint8_t savedId = preferences.getUChar("canton", CFG_CANTON_ID_UNASSIGNED);
        if (savedId >= 1 && savedId <= CFG_MAX_CANTON_ID)
            assignedBoosterId = savedId;
        const uint8_t savedProfile = preferences.getUChar("track_profile", 0);
        if (savedProfile <= 1)
            trackProfileHo = savedProfile == 1;
        uint8_t timing[4] = {};
        if (preferences.getBytesLength("timing") == sizeof(timing) &&
            preferences.getBytes("timing", timing, sizeof(timing)) == sizeof(timing))
        {
            const uint16_t savedTransaction = static_cast<uint16_t>(timing[0]) |
                (static_cast<uint16_t>(timing[1]) << 8);
            const int8_t correction1 = static_cast<int8_t>(timing[2]);
            const int8_t correction0 = static_cast<int8_t>(timing[3]);
            if (correction1 >= -5 && correction1 <= 5 &&
                correction0 >= -5 && correction0 <= 5)
            {
                timingCorrection1Us = correction1;
                timingCorrection0Us = correction0;
                lastTimingTransaction = savedTransaction;
                lastTimingRequest1Us = correction1;
                lastTimingRequest0Us = correction0;
            }
        }
        preferences.end();
    }
    regen.begin();
    drv.begin();
    analogSetPinAttenuation(PIN_RAIL_VOLTAGE, ADC_11db);
    railcom.begin();
    canRx.begin();
    tensionControl.begin();
    tensionControl.setTrackProfile(trackProfileHo);
    telemetry.state = BoosterState::OFF;
}

void BoosterCore::shutdown(BoosterState state)
{
    drv.disableOutput();     // coast = Hi-Z
    telemetry.state = state;
}

// Gestion du profil de tension (HO/N)
bool BoosterCore::setTrackProfile(bool ho) noexcept
{
    if (powerEnabled || drv.isEnabled())
        return false;
    if (!tensionControl.setTrackProfile(ho))
        return false;

    Preferences preferences;
    if (!preferences.begin("cfc-booster", false))
    {
        tensionControl.setTrackProfile(trackProfileHo);
        return false;
    }
    const size_t written = preferences.putUChar("track_profile", ho ? 1 : 0);
    preferences.end();
    if (written != sizeof(uint8_t))
    {
        tensionControl.setTrackProfile(trackProfileHo);
        return false;
    }
    trackProfileHo = ho;
    return true;
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
        associationValidated = false;
        Preferences preferences;
        if (preferences.begin("cfc-booster", false))
        {
            preferences.putUChar("canton", id);
            preferences.end();
        }
    }
    return true;
}

void BoosterCore::clearBoosterId()
{
    setPowerEnabled(false);
    assignedBoosterId = CFG_CANTON_ID_UNASSIGNED;
    associationValidated = false;
    discoveryPermit = false;
    Preferences preferences;
    if (preferences.begin("cfc-booster", false))
    {
        preferences.remove("canton");
        preferences.end();
    }
}

void BoosterCore::setDiscoveryPermit(bool enabled)
{
    if (!enabled)
    {
        discoveryPermit = false;
        associationValidated = false;
        return;
    }
    discoveryPermit = true;
    discoveryPermitMs = millis();
}

bool BoosterCore::shouldAnnounceDiscoveryId() const
{
    return hasFreshDiscoveryPermit() && !associationValidated && hasBoosterId();
}

bool BoosterCore::hasFreshDiscoveryPermit() const
{
    return discoveryPermit &&
           millis() - discoveryPermitMs < cfc::dcc::BOOSTER_DISCOVERY_MODE_TTL_MS;
}

void BoosterCore::markAssociationValidated()
{
    associationValidated = true;
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

cfc::dcc::TimingResult BoosterCore::setTimingCorrections(
    uint16_t transaction, int8_t halfBit1Us, int8_t halfBit0Us,
    int8_t &savedHalfBit1Us, int8_t &savedHalfBit0Us)
{
    using cfc::dcc::TimingResult;
    savedHalfBit1Us = timingCorrection1Us;
    savedHalfBit0Us = timingCorrection0Us;
    if (transaction == 0)
        return TimingResult::TRANSACTION_CONFLICT;
    if (halfBit1Us < -5 || halfBit1Us > 5 || halfBit0Us < -5 || halfBit0Us > 5)
        return TimingResult::CORRECTION_OUT_OF_RANGE;
    if (transaction == lastTimingTransaction && lastTimingTransaction != 0)
        return halfBit1Us == lastTimingRequest1Us && halfBit0Us == lastTimingRequest0Us
            ? TimingResult::APPLIED : TimingResult::TRANSACTION_CONFLICT;
    if (drv.isEnabled() || regen.busy())
        return TimingResult::OUTPUT_ACTIVE;

    const int16_t base1 = dccRefValid ? dccRefHalfBit1Us : 58;
    const int16_t base0 = dccRefValid ? dccRefHalfBit0Us : 100;
    const int16_t effective1 = base1 + halfBit1Us;
    const int16_t effective0 = base0 + halfBit0Us;
    if (effective1 <= 0 || effective0 <= 0 || effective1 >= effective0 ||
        effective1 > 255 || effective0 > 255)
        return TimingResult::RESULTING_TIMING_INVALID;

    const int16_t oldEffective1 = base1 + timingCorrection1Us;
    const int16_t oldEffective0 = base0 + timingCorrection0Us;
    if (!regen.setHalfBitUs(static_cast<uint8_t>(effective1),
                            static_cast<uint8_t>(effective0)))
        return TimingResult::RESULTING_TIMING_INVALID;

    const uint8_t timing[4] = {
        static_cast<uint8_t>(transaction & 0xFFU),
        static_cast<uint8_t>(transaction >> 8),
        static_cast<uint8_t>(halfBit1Us),
        static_cast<uint8_t>(halfBit0Us)};
    Preferences preferences;
    if (!preferences.begin("cfc-booster", false)) {
        if (oldEffective1 > 0 && oldEffective0 > oldEffective1 && oldEffective0 <= 255)
            regen.setHalfBitUs(static_cast<uint8_t>(oldEffective1),
                               static_cast<uint8_t>(oldEffective0));
        return TimingResult::STORAGE_FAILED;
    }
    const size_t written = preferences.putBytes("timing", timing, sizeof(timing));
    preferences.end();
    if (written != sizeof(timing)) {
        if (oldEffective1 > 0 && oldEffective0 > oldEffective1 && oldEffective0 <= 255)
            regen.setHalfBitUs(static_cast<uint8_t>(oldEffective1),
                               static_cast<uint8_t>(oldEffective0));
        return TimingResult::STORAGE_FAILED;
    }

    timingCorrection1Us = halfBit1Us;
    timingCorrection0Us = halfBit0Us;
    lastTimingTransaction = transaction;
    lastTimingRequest1Us = halfBit1Us;
    lastTimingRequest0Us = halfBit0Us;
    profileApplied = true;
    appliedBit1Us = static_cast<uint8_t>(effective1);
    appliedBit0Us = static_cast<uint8_t>(effective0);
    savedHalfBit1Us = timingCorrection1Us;
    savedHalfBit0Us = timingCorrection0Us;
    return TimingResult::APPLIED;
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
    const int16_t effectiveBit1Us = static_cast<int16_t>(dccRefHalfBit1Us) +
                                    timingCorrection1Us;
    const int16_t effectiveBit0Us = static_cast<int16_t>(dccRefHalfBit0Us) +
                                    timingCorrection0Us;
    if (effectiveBit1Us <= 0 || effectiveBit0Us <= 0 ||
        effectiveBit1Us >= effectiveBit0Us || effectiveBit1Us > 255 ||
        effectiveBit0Us > 255)
    {
        shutdown(BoosterState::FAULT);
        return;
    }
    if (!profileApplied || appliedBit1Us != effectiveBit1Us ||
        appliedBit0Us != effectiveBit0Us)
    {
        // Les éléments déjà préparés dans le RMT gardent leur ancien timing.
        // Attendre leur fin avant de charger une nouvelle durée de demi-bit.
        if (drv.isEnabled())
            shutdown(BoosterState::OFF);
        if (regen.busy())
            profileReadyForEmission = false;
        else if (!regen.setHalfBitUs(static_cast<uint8_t>(effectiveBit1Us),
                                     static_cast<uint8_t>(effectiveBit0Us)))
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
            appliedBit1Us = static_cast<uint8_t>(effectiveBit1Us);
            appliedBit0Us = static_cast<uint8_t>(effectiveBit0Us);
            appliedProfileCount++;
        }
    }
    appliedProfileSeq = dccRefSeq;

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
        if (event.packetStart)
            generatorFault = false;
        bits[n++] = event.encoded & 0x01; // la phase n'affecte pas la valeur du bit
    }
    if (n > 0)
    {
        if (!drv.isEnabled())
            drv.enableOutput();

        if (regen.emit(bits, n) == 0)
        {
            generatorFault = true;
            localFaultSinceMs = millis();
            shutdown(BoosterState::FAULT);
            return;
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
            // Le flux v0.4.0 exige une durée mesurée dans l'enregistrement.
            if (!event.hasMeasuredCutoutInterval)
            {
                skippedCutoutCount++;
                return;
            }
            const uint32_t intervalUs = event.cutoutIntervalUs;
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

    const uint32_t now = millis();
    if (now - lastRailVoltageSampleMs < CFG_RAIL_VOLTAGE_SAMPLE_PERIOD_MS)
        return;
    lastRailVoltageSampleMs = now;

    if (!powerEnabled || !drv.isEnabled()) {
        telemetry.railVoltage_mV = 0;
        telemetry.railVoltageValid = false;
        return;
    }

    const uint32_t adcMilliVolts = analogReadMilliVolts(PIN_RAIL_VOLTAGE);
    const int32_t measuredMilliVolts = static_cast<int32_t>(
        adcMilliVolts * CFG_RAIL_VOLTAGE_DIVIDER_RATIO + 0.5f) +
        CFG_RAIL_VOLTAGE_CORRECTION_MV;
    if (measuredMilliVolts < 0 || measuredMilliVolts >= 0xFFFF) {
        telemetry.railVoltage_mV = 0;
        telemetry.railVoltageValid = false;
        return;
    }

    telemetry.railVoltage_mV = static_cast<uint16_t>(measuredMilliVolts);
    telemetry.railVoltageValid = CFG_RAIL_VOLTAGE_CALIBRATED;
}

const BoosterTelemetry &BoosterCore::getTelemetry() const { return telemetry; }
DccCanRx &BoosterCore::getCanRx() { return canRx; }
const DccCanRx &BoosterCore::getCanRx() const { return canRx; }
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
int8_t BoosterCore::timingCorrectionHalfBit1Us() const noexcept { return timingCorrection1Us; }
int8_t BoosterCore::timingCorrectionHalfBit0Us() const noexcept { return timingCorrection0Us; }
uint32_t BoosterCore::profileApplications() const noexcept { return appliedProfileCount; }
uint32_t BoosterCore::cutoutsApplied() const noexcept { return appliedCutoutCount; }
uint32_t BoosterCore::cutoutsRejectedRange() const noexcept { return rejectedCutoutRangeCount; }
uint32_t BoosterCore::cutoutsSkipped() const noexcept { return skippedCutoutCount; }
uint16_t BoosterCore::lastCutoutAppliedUs() const noexcept { return lastAppliedCutoutUs; }
bool BoosterCore::lastCutoutWasMeasured() const noexcept { return lastCutoutMeasured; }
cfc::dcc::BoosterDccCause BoosterCore::dccFaultCause() const noexcept
{
    using cfc::dcc::BoosterDccCause;
    if (!hasBoosterId() || !powerEnabled ||
        (dccReferenceFresh() && !dccRefMainOn))
        return BoosterDccCause::NONE;
    if (generatorFault) return BoosterDccCause::GENERATOR_ERROR;
    if (!dccReferenceFresh()) {
        if (!dccRefReceived) return BoosterDccCause::PROFILE_MISSING;
        if (!dccRefValid) return BoosterDccCause::PROFILE_INVALID;
        return BoosterDccCause::PROFILE_STALE;
    }
    if (!dccRefMainOn) return BoosterDccCause::NONE;
    return canRx.faultCause();
}
