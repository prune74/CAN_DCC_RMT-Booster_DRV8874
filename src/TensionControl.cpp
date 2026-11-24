#include "TensionControl.h"

// Adresse I2C et registre du digipot (MCP4551 / MCP4561)
// Les valeurs DIGIPOT_HO et DIGIPOT_N sont fixes dans Config.h

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
    return writeDigipot(value);
}

// Écrit la valeur dans le digipot via I2C
bool TensionControl::writeDigipot(uint8_t value) noexcept
{
    Wire.beginTransmission(DIGIPOT_ADDR);
    Wire.write(DIGIPOT_REG);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}
