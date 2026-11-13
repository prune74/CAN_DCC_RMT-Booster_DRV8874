#pragma once

#include "BoosterCore.h"
#include "UartProtocol.h"

class BoosterUartLink
{
public:
    void begin();
    void update(BoosterCore &booster);

private:
    void receive(BoosterCore &booster);
    void handleFrame(const BoosterUart::Frame &frame, BoosterCore &booster);
    void sendAck(uint8_t command, uint16_t commandSequence, uint8_t status);
    bool sendFrame(const BoosterUart::Frame &frame);
    void sendSample(BoosterCore &booster);
    void sendRailCom(const BoosterCore &booster);

    BoosterUart::Parser m_parser;
    uint16_t m_txSequence = 0;
    uint32_t m_lastHeartbeatMs = 0;
    uint32_t m_lastSampleUs = 0;
    uint16_t m_lastRailcomAddress = 0;
    bool m_heartbeatSeen = false;
    bool m_timeoutApplied = false;
};
