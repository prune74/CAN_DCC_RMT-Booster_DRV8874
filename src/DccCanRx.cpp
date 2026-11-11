#include "DccCanRx.h"

void DccCanRx::begin() { lastBlockMs = millis(); }

// Appelée par CanBooster à CHAQUE trame CAN reçue — traitement minuscule
void DccCanRx::onCanMessage(uint16_t id, const uint8_t *data, uint8_t len) {
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
bool DccCanRx::popBit(uint8_t &encoded) {
    if (count == 0) return false;
    encoded = fifo[head];
    head = (head + 1) % FIFO_SIZE;
    count--;
    return true;
}

bool DccCanRx::popCutout() {
    if (!cutoutPending) return false;
    cutoutPending = false;
    return true;
}

// Watchdog : le convertisseur coupe à > 500 µs de silence DCC ;
// on rejoue la même exigence côté booster (500 ms de marge bus).
bool DccCanRx::dccAlive() const { return (millis() - lastBlockMs) < CFG_FAILSAFE_MS; }

void DccCanRx::push(uint8_t encoded) {
    if (count >= FIFO_SIZE) return; // FIFO pleine : bit perdu (compteur ?)
    fifo[(head + count) % FIFO_SIZE] = encoded;
    count++;
}
