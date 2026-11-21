#include "DccCanRx.h"
#include <CfcDccProtocol.h>

using namespace cfc::dcc;

void DccCanRx::begin() {
    lastBlockMs = millis();
    // Au démarrage, attendre un début de paquet complet pour éviter de
    // régénérer un flux qui aurait commencé avant l'initialisation du Booster.
    streamFault = true;
    haveSequence = false;
}

void DccCanRx::onCanMessage(uint16_t id, const uint8_t *data, uint8_t len) {
    if (id != CAN_BOOSTER_STREAM_ID) return;
    if (millis() - lastBlockMs >= BOOSTER_NO_BLOCK_FAILSAFE_MS)
        markStreamFault();
    if (data == nullptr || len < STREAM_DLC_DCC_1_TO_8_BITS ||
        data[0] != STREAM_MAGIC) {
        markStreamFault();
        malformedFrames++;
        return;
    }

    const uint16_t sequence = static_cast<uint16_t>(data[1]) |
                              (static_cast<uint16_t>(data[2]) << 8);
    const uint8_t control = data[3];
    const uint8_t type = control & STREAM_CONTROL_TYPE_MASK;
    const bool packetStart = (control & STREAM_CONTROL_PACKET_START) != 0;

    bool sequenceOk = !haveSequence || sequence == expectedSequence;
    if (!sequenceOk) {
        markStreamFault();
        sequenceGaps++;
        expectedSequence = static_cast<uint16_t>(sequence + 1);
        haveSequence = true;
        return;
    }
    expectedSequence = static_cast<uint16_t>(sequence + 1);
    haveSequence = true;

    if (type == static_cast<uint8_t>(StreamRecordType::DCC_BITS) << 6) {
        const uint8_t n = control & STREAM_CONTROL_COUNT_MASK;
        const uint8_t requiredDlc = n >= 1 && n <= 8
            ? STREAM_DLC_DCC_1_TO_8_BITS
            : (n >= 9 && n <= MAX_DCC_BITS_PER_BLOCK
                ? STREAM_DLC_DCC_9_TO_16_BITS : 0);
        if (requiredDlc == 0 || len != requiredDlc) {
            markStreamFault();
            malformedFrames++;
            return;
        }
        const bool unusedBitsNonZero = n <= 8
            ? (n < 8 && (data[4] & static_cast<uint8_t>((1u << (8 - n)) - 1u)) != 0)
            : (n < 16 && (data[5] & static_cast<uint8_t>((1u << (16 - n)) - 1u)) != 0);
        const bool packetStartNotZero = packetStart && (data[4] & 0x80) != 0;
        if (unusedBitsNonZero || packetStartNotZero) {
            markStreamFault();
            malformedFrames++;
            return;
        }
        if (streamFault && !packetStart) return;
        if (packetStart) streamFault = false;
        for (uint8_t i = 0; i < n; i++) {
            const uint8_t packed = data[4 + (i / 8)];
            const uint8_t bit = (packed >> (7 - (i % 8))) & 1;
            pushBit(bit, packetStart && i == 0);
            if (streamFault) return;
        }
        lastBlockMs = millis();
        blocksSeen++;
        return;
    }

    if (type == (static_cast<uint8_t>(StreamRecordType::CUTOUT) << 6)) {
        if (len != STREAM_DLC_CUTOUT) {
            markStreamFault();
            malformedFrames++;
            malformedCutoutFrames++;
            return;
        }
        const uint8_t n = control & STREAM_CONTROL_COUNT_MASK;
        const uint8_t flags = data[6];
        const uint8_t allowedFlags = STREAM_CUTOUT_FLAG_LOCAL |
                                     STREAM_CUTOUT_FLAG_GLOBAL;
        if (n != 0 || packetStart ||
            (flags & static_cast<uint8_t>(~allowedFlags)) != 0 ||
            (flags & allowedFlags) == 0) {
            markStreamFault();
            malformedFrames++;
            malformedCutoutFrames++;
            return;
        }
        if (streamFault) return;
        const uint16_t intervalUs = static_cast<uint16_t>(data[4]) |
                                    (static_cast<uint16_t>(data[5]) << 8);
        receivedCutoutStarts++;
        lastReceivedCutoutUs = intervalUs;
        lastReceivedCutoutHasMeasurement = true;
        pushCutout((flags & STREAM_CUTOUT_FLAG_GLOBAL) != 0, intervalUs, true);
        lastBlockMs = millis();
        return;
    }

    markStreamFault();
    malformedFrames++;
}

bool DccCanRx::peekEvent(DccCanEvent &event) const {
    if (count == 0) return false;
    event = fifo[head];
    return true;
}

bool DccCanRx::popEvent(DccCanEvent &event) {
    if (count == 0) return false;
    event = fifo[head];
    head = (head + 1) % FIFO_SIZE;
    count--;
    return true;
}

bool DccCanRx::dccAlive() const {
    return !streamFault && (millis() - lastBlockMs) < BOOSTER_NO_BLOCK_FAILSAFE_MS;
}

void DccCanRx::markStreamFault() {
    streamFault = true;
    head = 0;
    count = 0;
}
uint32_t DccCanRx::cutoutStartsReceived() const { return receivedCutoutStarts; }
uint16_t DccCanRx::lastCutoutReceivedUs() const { return lastReceivedCutoutUs; }
bool DccCanRx::lastCutoutReceivedMeasured() const { return lastReceivedCutoutHasMeasurement; }
uint32_t DccCanRx::cutoutMalformedFrames() const { return malformedCutoutFrames; }
uint32_t DccCanRx::cutoutQueueDrops() const { return droppedCutoutEvents; }

void DccCanRx::pushBit(uint8_t bit, bool packetStart) {
    if (count >= FIFO_SIZE) {
        markStreamFault();
        return;
    }
    DccCanEvent &event = fifo[(head + count) % FIFO_SIZE];
    event.type = DccCanEventType::BIT;
    event.encoded = bit;
    event.packetStart = packetStart;
    event.global = false;
    event.cutoutIntervalUs = 0;
    event.hasMeasuredCutoutInterval = false;
    count++;
}

void DccCanRx::pushCutout(bool global, uint16_t intervalUs, bool measured) {
    if (count >= FIFO_SIZE) {
        droppedCutoutEvents++;
        markStreamFault();
        return;
    }
    DccCanEvent &event = fifo[(head + count) % FIFO_SIZE];
    event.type = DccCanEventType::CUTOUT;
    event.encoded = 0;
    event.packetStart = false;
    event.global = global;
    event.cutoutIntervalUs = intervalUs;
    event.hasMeasuredCutoutInterval = measured;
    count++;
}
