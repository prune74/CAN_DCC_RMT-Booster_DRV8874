#pragma once

#include "BoosterCore.h"
#include <UartProtocol.h>

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
    void updateDccDiagnostic(const BoosterCore &booster, uint32_t nowMs);
    void sendPendingDccDiagnostic(uint32_t nowMs);
    void sendPendingTimingResult(uint32_t nowMs);
    void queueTimingResult(uint16_t transaction, uint8_t result,
                           int8_t saved1, int8_t saved0);

    BoosterUart::Parser m_parser;
    uint16_t m_txSequence = 0;
    uint32_t m_lastHeartbeatMs = 0;
    uint32_t m_lastSampleUs = 0;
    uint16_t m_lastRailcomAddress = 0;
    BoosterUart::Frame m_pendingDiagnosticFrame{};
    BoosterUart::Frame m_pendingTimingResultFrame{};
    uint32_t m_lastDiagnosticTxMs = 0;
    uint32_t m_lastTimingResultTxMs = 0;
    uint8_t m_reportedDiagnosticCause = 0;
    bool m_reportedDiagnosticActive = false;
    bool m_diagnosticPending = false;
    bool m_timingResultPending = false;
    bool m_heartbeatSeen = false;
    bool m_timeoutApplied = false;
    uint16_t m_lastDiscoverySequence = 0xFFFF;
    uint16_t m_lastAssociationSequence = 0xFFFF;
};
