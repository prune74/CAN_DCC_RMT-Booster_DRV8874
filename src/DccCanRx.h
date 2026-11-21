#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "Config.h"
#include "Types.h"

// ============================================================
// DccCanRx — décodage des trames CAN du protocole DCC_CAN_RMT-Booster
//
//   0x100 : bloc de bits DCC, transmis dans l'ordre chronologique
//     data[0] = N bits (1..16)
//     puis 4 bits/octet : premier bit dans le nibble haut, chaque nibble
//     vaut (bit << 1) | phase
//     DLC = 1 + ceil(N/2)
//
//   0x101 : START DLC 4 { local, global, intervalle_us_LE16 } puis END
//     DLC 2 { 0, 0 }. Le START est inséré après les bits précédents.
//     Les anciens START DLC 2 restent reconnus avec la durée de repli.
//     L'intervalle capturé est classé comme cutout; sa correspondance exacte
//     avec la fenêtre RailCom électrique reste à valider.
//
//   0x102 : télémétrie sur le CAN local ; elle n'intervient pas dans les
//     décisions de ce booster (la supervision remonte par UART à l'EXCC).
//
//   0x104 : profil DCC relayé par le convertisseur, DLC 8 :
//     {version, séquence_LE16, flags, demi-bit1_us, demi-bit0_us,
//      tick_ref_us, préambule_MAIN}. Les durées de bits servent à la RMT TX.
//
//   Le convertisseur envoie le flux au fil de l'eau. Blocs et START cutout
//   sont conservés dans une FIFO ordonnée que le Core vide vers le RMT.
// ============================================================

enum class DccCanEventType : uint8_t {
    BIT,
    CUTOUT
};

struct DccCanEvent {
    DccCanEventType type = DccCanEventType::BIT;
    uint8_t encoded = 0; // bit DCC + phase pour BIT
    bool global = false; // portée du cutout
    uint16_t cutoutIntervalUs = 0; // intervalle d'entrée mesuré
    bool hasMeasuredCutoutInterval = false;
};

class DccCanRx {
public:
    static constexpr uint16_t DCC_BLOCK_ID  = 0x100;
    static constexpr uint16_t CUTOUT_ID     = 0x101;
    static constexpr uint16_t DCC_PROFILE_ID = 0x104;
    static constexpr uint8_t DCC_PROFILE_VERSION = 1;
    static constexpr uint8_t PROFILE_VALID = 0x01;
    static constexpr uint8_t PROFILE_MAIN_ON = 0x02;
    static constexpr size_t   FIFO_SIZE    = 256; // bits — marge > 16 × retard bus

    void begin();
    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len);
    bool peekEvent(DccCanEvent &event) const;
    bool popEvent(DccCanEvent &event);
    bool dccAlive() const;
    bool hasDccProfile() const;
    bool dccMainOn() const;
    uint8_t halfBit1Us() const;
    uint8_t halfBit0Us() const;
    uint16_t profileSequence() const;
    uint32_t profileAgeMs() const;
    uint32_t profileValidFrames() const;
    uint32_t profileInvalidFrames() const;
    uint32_t cutoutStartsReceived() const;
    uint16_t lastCutoutReceivedUs() const;
    bool lastCutoutReceivedMeasured() const;
    uint32_t cutoutMalformedFrames() const;
    uint32_t cutoutQueueDrops() const;

    uint32_t blocksSeen = 0;

private:
    void pushBit(uint8_t encoded);
    void pushCutout(bool global, uint16_t intervalUs, bool measured);

    DccCanEvent fifo[FIFO_SIZE] = {};
    size_t   head = 0, count = 0;
    uint32_t lastBlockMs = 0;
    uint32_t lastProfileMs = 0;
    bool dccProfileReceived = false;
    bool dccProfileMainOn = false;
    uint8_t profileHalfBit1Us = 58; // repli compatible avec le profil LaBox actuel
    uint8_t profileHalfBit0Us = 100;
    uint16_t profileSeq = 0;
    uint32_t validProfileFrames = 0;
    uint32_t invalidProfileFrames = 0;
    uint32_t receivedCutoutStarts = 0;
    uint16_t lastReceivedCutoutUs = 0;
    bool lastReceivedCutoutHasMeasurement = false;
    uint32_t malformedCutoutFrames = 0;
    uint32_t droppedCutoutEvents = 0;
};
