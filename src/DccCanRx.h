#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include <CfcDccProtocol.h>

// ============================================================
// DccCanRx — décodage des trames CAN du protocole DCC_CAN_RMT-Booster
//
//   0x100 : enregistrements versionnés v0.4.0 (DCC bits OU cutout), avec
//     compteur séquentiel 16 bits; une séquence manquante bloque la sortie.
// ============================================================

enum class DccCanEventType : uint8_t {
    BIT,
    CUTOUT
};

struct DccCanEvent {
    DccCanEventType type = DccCanEventType::BIT;
    uint8_t encoded = 0; // bit DCC + phase pour BIT
    bool packetStart = false;
    bool global = false; // portée du cutout
    uint16_t cutoutIntervalUs = 0; // intervalle d'entrée mesuré
    bool hasMeasuredCutoutInterval = false;
};

class DccCanRx {
public:
    static constexpr size_t   FIFO_SIZE    = 256; // bits — marge > 16 × retard bus

    void begin();
    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len);
    bool peekEvent(DccCanEvent &event) const;
    bool popEvent(DccCanEvent &event);
    bool dccAlive() const;
    cfc::dcc::BoosterDccCause faultCause() const;
    uint16_t faultExpectedSequence() const;
    uint16_t faultReceivedSequence() const;
    uint32_t cutoutStartsReceived() const;
    uint16_t lastCutoutReceivedUs() const;
    bool lastCutoutReceivedMeasured() const;
    uint32_t cutoutMalformedFrames() const;
    uint32_t cutoutQueueDrops() const;

    uint32_t blocksSeen = 0;

private:
    void markStreamFault(cfc::dcc::BoosterDccCause cause);
    void pushBit(uint8_t encoded, bool packetStart);
    void pushCutout(bool global, uint16_t intervalUs, bool measured);

    DccCanEvent fifo[FIFO_SIZE] = {};
    size_t   head = 0, count = 0;
    uint32_t lastBlockMs = 0;
    uint32_t receivedCutoutStarts = 0;
    uint16_t lastReceivedCutoutUs = 0;
    bool lastReceivedCutoutHasMeasurement = false;
    uint32_t malformedCutoutFrames = 0;
    uint32_t droppedCutoutEvents = 0;
    uint32_t malformedFrames = 0;
    uint32_t sequenceGaps = 0;
    uint16_t expectedSequence = 0;
    uint16_t faultExpectedSequenceValue = 0;
    uint16_t faultReceivedSequenceValue = 0;
    cfc::dcc::BoosterDccCause activeFaultCause = cfc::dcc::BoosterDccCause::NONE;
    bool haveSequence = false;
    bool streamFault = false;
};
