#include "Telemetry.h"

Telemetry::Telemetry(BoosterCore &core, SendFrame send) : core(core), send(send) {}

void Telemetry::update() {
    const uint32_t now = millis();
    if (!core.hasBoosterId()) {
        hasSentState = false;
        lastBoosterId = CFG_CANTON_ID_UNASSIGNED;
        return;
    }

    const BoosterTelemetry &t = core.getTelemetry();
    const uint8_t id = core.boosterId();
    const uint8_t state = static_cast<uint8_t>(t.state);
    const bool idChanged = !hasSentState || id != lastBoosterId;
    const bool stateChanged = !hasSentState || state != lastState;
    const bool heartbeatDue = !hasSentState ||
                              now - lastSend >= CFG_TELEMETRY_HEARTBEAT_MS;

    if (idChanged || stateChanged || heartbeatDue) {
        uint8_t data[5];
        data[0] = id;
        data[1] = CFG_BOOSTER_TYPE;
        data[2] = (uint8_t)(t.current_mA / 10);
        data[3] = (uint8_t)(t.voltage_mV / 100);
        data[4] = state;
        if (send(0x102, data, 5)) {
            lastSend = now;
            lastState = state;
            lastBoosterId = id;
            hasSentState = true;
        }
    }

}
