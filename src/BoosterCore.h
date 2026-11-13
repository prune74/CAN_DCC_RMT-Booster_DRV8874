#pragma once
#include "DccCanRx.h"
#include "DccRegen.h"
#include "Config.h"
#include "Drv8874.h"
#include "RailCom.h"
#include "TensionControl.h"
#include "Types.h"

// ============================================================
// BoosterCore — orchestration
//
//  update() — appelé en boucle, jamais bloquant :
//    1. failsafe (watchdog blocs 0x100) + protection globale (0x102)
//    2. sécurité locale (courant, tension)
//    3. cutout RailCom régénéré (0x101, BRAKE + capture UART)
//    4. flux bits FIFO → RMT
// ============================================================

class BoosterCore {
public:
    void begin();
    void onCanMessage(uint16_t id, const uint8_t *data, uint8_t len);
    void update();
    void sampleSensors();
    void setPowerEnabled(bool enabled);
    bool setBoosterId(uint8_t id);

    // Gestion du profil de tension (HO/N)
    void setTrackProfile(bool ho) noexcept;
    bool isPowerEnabled() const noexcept;
    bool hasBoosterId() const noexcept;
    uint8_t boosterId() const noexcept;

    const BoosterTelemetry &getTelemetry() const;
    DccCanRx &getCanRx();
    Drv8874 &getDrv();

private:
    void shutdown(BoosterState state);   // coupe + note l'état

    DccCanRx canRx;
    DccRegen regen;
    Drv8874 drv;
    RailCom railcom;
    TensionControl tensionControl;
    BoosterTelemetry telemetry;
    bool powerEnabled = false;
    uint8_t assignedBoosterId = CFG_CANTON_ID_UNASSIGNED;
};