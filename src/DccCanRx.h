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
//   0x101 : START { local, global } puis END { 0, 0 }.
//     Le START est inséré dans la même FIFO que les bits, après les bits
//     qui le précèdent. L'END ne produit pas un second cutout.
//
//   0x102 : télémétrie sur le CAN local ; elle n'intervient pas dans les
//     décisions de ce booster (la supervision remonte par UART à l'EXCC).
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
};

class DccCanRx {
public:
    static constexpr uint16_t DCC_BLOCK_ID  = 0x100;
    static constexpr uint16_t CUTOUT_ID     = 0x101;
    static constexpr size_t   FIFO_SIZE    = 256; // bits — marge > 16 × retard bus

    void begin();
    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len);
    bool peekEvent(DccCanEvent &event) const;
    bool popEvent(DccCanEvent &event);
    bool dccAlive() const;

    uint32_t blocksSeen = 0;

private:
    void pushBit(uint8_t encoded);
    void pushCutout(bool global);

    DccCanEvent fifo[FIFO_SIZE] = {};
    size_t   head = 0, count = 0;
    uint32_t lastBlockMs = 0;
};
