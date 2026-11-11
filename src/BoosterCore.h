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

    // Gestion du profil de tension (HO/N)
    void setTrackProfile(bool ho) noexcept;

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
};