#pragma once
#include "DccCanRx.h"
#include "DccRegen.h"
#include "Config.h"
#include "Drv8874.h"
#include "RailCom.h"
#include "TensionControl.h"
#include "Types.h"
#include <CfcDccProtocol.h>

// ============================================================
// BoosterCore — orchestration
//
//  update() — appelé en boucle, jamais bloquant :
//    1. failsafe (watchdog blocs 0x100)
//    2. sécurité locale (courant, tension)
//    3. cutout RailCom régénéré (enregistrement 0x100, BRAKE + capture UART)
//    4. flux bits FIFO → RMT
// ============================================================

class BoosterCore {
public:
    void begin();
    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len);
    void update();
    void sampleSensors();
    void setPowerEnabled(bool enabled);
    bool setBoosterId(uint8_t id);
    bool clearBoosterId();
    void setDiscoveryPermit(bool enabled);
    bool hasFreshDiscoveryPermit() const;
    bool shouldAnnounceDiscoveryId() const;
    void markAssociationValidated();
    bool setDccReference(const uint8_t *data, uint8_t len);
    cfc::dcc::TimingResult setTimingCorrections(uint16_t transaction,
                                                int8_t halfBit1Us,
                                                int8_t halfBit0Us,
                                                int8_t &savedHalfBit1Us,
                                                int8_t &savedHalfBit0Us);

    // Gestion du profil de tension (HO/N)
    bool setTrackProfile(bool ho) noexcept;
    bool isPowerEnabled() const noexcept;
    bool hasBoosterId() const noexcept;
    uint8_t boosterId() const noexcept;
    bool hasAppliedDccProfile() const noexcept;
    uint16_t appliedProfileSequence() const noexcept;
    bool hasDccReference() const noexcept;
    bool dccReferenceValid() const noexcept;
    bool dccReferenceFresh() const noexcept;
    bool dccReferenceMainOn() const noexcept;
    uint16_t dccReferenceSequence() const noexcept;
    uint32_t dccReferenceAgeMs() const noexcept;
    uint8_t dccReferenceHalfBit1Us() const noexcept;
    uint8_t dccReferenceHalfBit0Us() const noexcept;
    uint32_t dccReferenceValidFrames() const noexcept;
    uint32_t dccReferenceInvalidFrames() const noexcept;
    uint8_t appliedHalfBit1Us() const noexcept;
    uint8_t appliedHalfBit0Us() const noexcept;
    int8_t timingCorrectionHalfBit1Us() const noexcept;
    int8_t timingCorrectionHalfBit0Us() const noexcept;
    uint32_t profileApplications() const noexcept;
    uint32_t cutoutsApplied() const noexcept;
    uint32_t cutoutsRejectedRange() const noexcept;
    uint32_t cutoutsSkipped() const noexcept;
    uint16_t lastCutoutAppliedUs() const noexcept;
    bool lastCutoutWasMeasured() const noexcept;
    cfc::dcc::BoosterDccCause dccFaultCause() const noexcept;

    const BoosterTelemetry &getTelemetry() const;
    DccCanRx &getCanRx();
    const DccCanRx &getCanRx() const;
    Drv8874 &getDrv();

private:
    void shutdown(BoosterState state);   // coupe + note l'état

    DccCanRx canRx;
    DccRegen regen;
    Drv8874 drv;
    RailCom railcom;
    TensionControl tensionControl;
    BoosterTelemetry telemetry;
    bool powerEnabled = false;
    bool trackProfileHo = false;
    uint8_t assignedBoosterId = CFG_CANTON_ID_UNASSIGNED;
    uint32_t discoveryPermitMs = 0;
    uint32_t lastRailVoltageSampleMs = 0;
    bool discoveryPermit = false;
    bool associationValidated = false;
    uint32_t localFaultSinceMs = 0;
    bool profileApplied = false;
    uint16_t appliedProfileSeq = 0;
    bool dccRefReceived = false;
    bool dccRefValid = false;
    bool dccRefMainOn = false;
    uint16_t dccRefSeq = 0;
    uint8_t dccRefHalfBit1Us = 0;
    uint8_t dccRefHalfBit0Us = 0;
    uint8_t dccRefTickUs = 0;
    uint8_t dccRefPreambleBits = 0;
    uint32_t lastValidDccReferenceMs = 0;
    uint32_t validDccReferenceFrames = 0;
    uint32_t invalidDccReferenceFrames = 0;
    uint8_t appliedBit1Us = 58;
    uint8_t appliedBit0Us = 100;
    int8_t timingCorrection1Us = 0;
    int8_t timingCorrection0Us = 0;
    uint16_t lastTimingTransaction = 0;
    int8_t lastTimingRequest1Us = 0;
    int8_t lastTimingRequest0Us = 0;
    uint32_t appliedProfileCount = 0;
    uint32_t appliedCutoutCount = 0;
    uint32_t rejectedCutoutRangeCount = 0;
    uint32_t skippedCutoutCount = 0;
    uint16_t lastAppliedCutoutUs = 0;
    bool lastCutoutMeasured = false;
    bool generatorFault = false;
};
