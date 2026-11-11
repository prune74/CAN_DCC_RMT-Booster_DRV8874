#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "Config.h"
#include "Types.h"

// ============================================================
// DccCanRx — décodage des trames CAN du protocole DCC_CAN_RMT-Booster
//
//   0x100 : bloc de bits DCC
//     data[0] = N bits (1..16)
//     puis 4 bits/octet, chaque nibble = (bit << 1) | phase
//     DLC = 1 + ceil(N/2)
//
//   0x101 : cutout réel { local, global } — START/END consécutifs
//     (le booster régénère un cutout nominal, jamais en failsafe)
//
//  Le convertisseur envoie le flux au fil de l'eau (bloc parti à
//  16 bits, sur cutout, ou après 500 µs de silence) : on stocke tout
//  dans une FIFO de bits, le Core la vide vers le RMT.
// ============================================================

class DccCanRx {
public:
    static constexpr uint16_t DCC_BLOCK_ID = 0x100;
    static constexpr uint16_t CUTOUT_ID    = 0x101;
    static constexpr size_t   FIFO_SIZE    = 256; // bits — marge > 16 × retard bus

    void begin();
    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len);
    bool popBit(uint8_t &encoded);
    bool popCutout();
    bool dccAlive() const;

    uint32_t blocksSeen = 0;

private:
    void push(uint8_t encoded);

    uint8_t  fifo[FIFO_SIZE] = {0};
    size_t   head = 0, count = 0;
    uint32_t lastBlockMs = 0;
    bool     cutoutPending = false;
};
