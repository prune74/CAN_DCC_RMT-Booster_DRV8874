#include "TensionControl.h"

// MCP45HV51 : l'adresse et les commandes sont communes à LaBox.

void TensionControl::begin() noexcept
{
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
}

// Définit le profil de voie
//   ho = true  → profil HO (15V, valeur DIGIPOT_HO)
//   ho = false → profil N  (12V, valeur DIGIPOT_N)
bool TensionControl::setTrackProfile(bool ho) noexcept
{
    uint8_t value = ho ? DIGIPOT_HO : DIGIPOT_N;
    if (!writeDigipot(value)) return false;
    uint8_t readBack = 0;
    return readDigipot(readBack) && readBack == value;
}

// Écrit la valeur dans le digipot via I2C
bool TensionControl::writeDigipot(uint8_t value) noexcept
{
    Wire.beginTransmission(DIGIPOT_ADDR);
    Wire.write(DIGIPOT_WIPER0_ADDRESS); // Write Data, registre volatile 00h
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool TensionControl::readDigipot(uint8_t &value) noexcept
{
    Wire.beginTransmission(DIGIPOT_ADDR);
    Wire.write(DIGIPOT_READ_WIPER0_COMMAND); // Read Data, registre 00h
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<uint8_t>(DIGIPOT_ADDR),
                         static_cast<uint8_t>(2)) != 2)
        return false;
    (void)Wire.read(); // octet haut réservé pour le curseur 8 bits
    value = static_cast<uint8_t>(Wire.read());
    return true;
}
