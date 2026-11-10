#pragma once
#include <stdint.h>
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

    void begin() { lastBlockMs = millis(); }

    // Appelée par CanBooster à CHAQUE trame CAN reçue — traitement minuscule
    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len) {
        if (id == DCC_BLOCK_ID && len >= 1) {
            const uint8_t n = data[0];
            if (n >= 1 && n <= 16 && (size_t)len >= (size_t)(1 + (n + 1) / 2)) {
                for (uint8_t i = 0; i < n; i++) {
                    const uint8_t byteVal = data[1 + i / 2];
                    const uint8_t nibble  = (i & 1) ? (byteVal & 0x0F) : (byteVal >> 4);
                    const uint8_t bit     = (nibble >> 1) & 0x01; // valeur du bit DCC
                    const uint8_t phase   = nibble & 0x01;       // demi-bit (alignement)
                    push(bit | (phase << 1));
                }
                lastBlockMs = millis();
                blocksSeen++;
            }
        } else if (id == CUTOUT_ID) {
            // { local, global } : on ne régénère que la présence du cutout.
            // TODO : vérifier le découpage exact (START/END) dans le source
            // du convertisseur et gérer les bornes si besoin.
            cutoutPending = true;
        }
    }

    // Le Core consomme un bit ; retourne false si FIFO vide
    bool popBit(uint8_t &encoded) {
        if (count == 0) return false;
        encoded = fifo[head];
        head = (head + 1) % FIFO_SIZE;
        count--;
        return true;
    }

    bool popCutout() {
        if (!cutoutPending) return false;
        cutoutPending = false;
        return true;
    }

    // Watchdog : le convertisseur coupe à > 500 µs de silence DCC ;
    // on rejoue la même exigence côté booster (500 ms de marge bus).
    bool dccAlive() const { return (millis() - lastBlockMs) < CFG_FAILSAFE_MS; }

    uint32_t blocksSeen = 0;

private:
    void push(uint8_t encoded) {
        if (count >= FIFO_SIZE) return; // FIFO pleine : bit perdu (compteur ?)
        fifo[(head + count) % FIFO_SIZE] = encoded;
        count++;
    }

    uint8_t  fifo[FIFO_SIZE] = {0};
    size_t   head = 0, count = 0;
    uint32_t lastBlockMs = 0;
    bool     cutoutPending = false;
};