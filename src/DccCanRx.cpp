#include "DccCanRx.h"

void DccCanRx::begin() { lastBlockMs = millis(); }

void DccCanRx::onCanMessage(uint16_t id, const uint8_t *data, uint8_t len) {
    if (id == DCC_PROFILE_ID) {
        if (len != 8 || data[0] != DCC_PROFILE_VERSION ||
            (data[3] & PROFILE_VALID) == 0 ||
            (data[3] & static_cast<uint8_t>(~(PROFILE_VALID | PROFILE_MAIN_ON))) != 0 ||
            data[4] == 0 || data[5] == 0 || data[4] >= data[5] ||
            data[6] != 1 || data[7] == 0) {
            invalidProfileFrames++;
            return;
        }

        profileSeq = static_cast<uint16_t>(data[1]) |
            (static_cast<uint16_t>(data[2]) << 8);
        profileHalfBit1Us = data[4];
        profileHalfBit0Us = data[5];
        dccProfileMainOn = (data[3] & PROFILE_MAIN_ON) != 0;
        dccProfileReceived = true;
        lastProfileMs = millis();
        validProfileFrames++;
    } else if (id == DCC_BLOCK_ID && len >= 1) {
        const uint8_t n = data[0];
        if (n >= 1 && n <= 16 && (size_t)len >= (size_t)(1 + (n + 1) / 2)) {
            for (uint8_t i = 0; i < n; i++) {
                const uint8_t byteVal = data[1 + i / 2];
                const uint8_t nibble  = (i & 1) ? (byteVal & 0x0F) : (byteVal >> 4);
                const uint8_t bit     = (nibble >> 1) & 0x01; // valeur du bit DCC
                const uint8_t phase   = nibble & 0x01;       // demi-bit (alignement)
                pushBit(bit | (phase << 1));
            }
            lastBlockMs = millis();
            blocksSeen++;
        }
    } else if (id == CUTOUT_ID) {
        // START DLC 4 porte l'intervalle mesuré; DLC 2 reste accepté en legacy.
        if (len < 2) {
            malformedCutoutFrames++;
            return;
        }
        const bool local = data[0] != 0;
        const bool global = data[1] != 0;
        if (local || global) {
            if (len != 2 && len != 4) {
                malformedCutoutFrames++;
                return;
            }
            const bool measured = len >= 4;
            const uint16_t intervalUs = measured
                ? static_cast<uint16_t>(data[2] | (static_cast<uint16_t>(data[3]) << 8))
                : 0;
            receivedCutoutStarts++;
            lastReceivedCutoutUs = intervalUs;
            lastReceivedCutoutHasMeasurement = measured;
            pushCutout(global, intervalUs, measured);
        } else if (len != 2) {
            malformedCutoutFrames++;
        }
    }
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

// Watchdog : le convertisseur coupe à > 500 µs de silence DCC ;
// on rejoue la même exigence côté booster (500 ms de marge bus).
bool DccCanRx::dccAlive() const { return (millis() - lastBlockMs) < CFG_FAILSAFE_MS; }
bool DccCanRx::hasDccProfile() const { return dccProfileReceived; }
bool DccCanRx::dccMainOn() const { return dccProfileMainOn; }
uint8_t DccCanRx::halfBit1Us() const { return profileHalfBit1Us; }
uint8_t DccCanRx::halfBit0Us() const { return profileHalfBit0Us; }
uint16_t DccCanRx::profileSequence() const { return profileSeq; }
uint32_t DccCanRx::profileAgeMs() const {
    return dccProfileReceived ? millis() - lastProfileMs : 0;
}
uint32_t DccCanRx::profileValidFrames() const { return validProfileFrames; }
uint32_t DccCanRx::profileInvalidFrames() const { return invalidProfileFrames; }
uint32_t DccCanRx::cutoutStartsReceived() const { return receivedCutoutStarts; }
uint16_t DccCanRx::lastCutoutReceivedUs() const { return lastReceivedCutoutUs; }
bool DccCanRx::lastCutoutReceivedMeasured() const { return lastReceivedCutoutHasMeasurement; }
uint32_t DccCanRx::cutoutMalformedFrames() const { return malformedCutoutFrames; }
uint32_t DccCanRx::cutoutQueueDrops() const { return droppedCutoutEvents; }

void DccCanRx::pushBit(uint8_t encoded) {
    if (count >= FIFO_SIZE) return;
    DccCanEvent &event = fifo[(head + count) % FIFO_SIZE];
    event.type = DccCanEventType::BIT;
    event.encoded = encoded;
    event.global = false;
    event.cutoutIntervalUs = 0;
    event.hasMeasuredCutoutInterval = false;
    count++;
}

void DccCanRx::pushCutout(bool global, uint16_t intervalUs, bool measured) {
    if (count >= FIFO_SIZE) {
        droppedCutoutEvents++;
        return;
    }
    DccCanEvent &event = fifo[(head + count) % FIFO_SIZE];
    event.type = DccCanEventType::CUTOUT;
    event.encoded = 0;
    event.global = global;
    event.cutoutIntervalUs = intervalUs;
    event.hasMeasuredCutoutInterval = measured;
    count++;
}
