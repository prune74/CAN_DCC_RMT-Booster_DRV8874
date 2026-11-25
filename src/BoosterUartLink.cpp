#include "BoosterUartLink.h"

#include "Pins.h"

#include <Arduino.h>

namespace
{
constexpr uint32_t UART_BAUD = 460800;
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 250;
constexpr uint32_t SAMPLE_PERIOD_US = 1000;
constexpr uint32_t DIAGNOSTIC_RETRY_MS = 100;
constexpr uint32_t PROFILE_RESULT_RETRY_MS = 100;

constexpr uint8_t ACK_OK = 0;
constexpr uint8_t ACK_INVALID = 1;
constexpr uint8_t ACK_NO_HEARTBEAT = 2;
constexpr uint8_t ACK_NO_ID = 3;
}

void BoosterUartLink::begin()
{
    Serial1.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
    m_lastSampleUs = micros();
    m_lastRailVoltageTxMs = millis();
}

void BoosterUartLink::update(BoosterCore &booster)
{
    receive(booster);

    const uint32_t nowMs = millis();
    const bool heartbeatValid =
        m_heartbeatSeen && nowMs - m_lastHeartbeatMs < HEARTBEAT_TIMEOUT_MS;
    if (!heartbeatValid && !m_timeoutApplied)
    {
        booster.setPowerEnabled(false);
        booster.setDiscoveryPermit(false);
        m_timeoutApplied = true;
    }

    const uint32_t nowUs = micros();
    if (nowUs - m_lastSampleUs >= SAMPLE_PERIOD_US)
    {
        m_lastSampleUs = nowUs;
        booster.sampleSensors();
        sendSample(booster);
    }

    sendRailCom(booster);
    if (heartbeatValid && booster.hasBoosterId() &&
        nowMs - m_lastRailVoltageTxMs >= CFG_RAIL_VOLTAGE_TELEMETRY_PERIOD_MS)
        sendRailVoltage(booster);
    updateDccDiagnostic(booster, nowMs);
    sendPendingTimingResult(nowMs);
    sendPendingProfileSetpointResult(nowMs);
}

void BoosterUartLink::receive(BoosterCore &booster)
{
    BoosterUart::Frame frame;
    while (Serial1.available() > 0)
    {
        if (m_parser.feed(static_cast<uint8_t>(Serial1.read()), frame))
            handleFrame(frame, booster);
    }
}

void BoosterUartLink::handleFrame(
    const BoosterUart::Frame &frame,
    BoosterCore &booster)
{
    const uint32_t now = millis();
    switch (static_cast<BoosterUart::Type>(frame.type))
    {
    case BoosterUart::Type::HEARTBEAT:
        if (frame.length == 0)
        {
            m_lastHeartbeatMs = now;
            m_heartbeatSeen = true;
            m_timeoutApplied = false;
        }
        break;

    case BoosterUart::Type::SET_CANTON_ID:
        if (frame.length != 1)
            sendAck(frame.type, frame.sequence, ACK_INVALID);
        else if (!m_heartbeatSeen || now - m_lastHeartbeatMs >= HEARTBEAT_TIMEOUT_MS)
            sendAck(frame.type, frame.sequence, ACK_NO_HEARTBEAT);
        else
        {
            const bool hadId = booster.hasBoosterId();
            const uint8_t previousId = booster.boosterId();
            if (!booster.setBoosterId(frame.payload[0]))
                sendAck(frame.type, frame.sequence, ACK_INVALID);
            else
            {
                if (frame.payload[0] == CFG_CANTON_ID_UNASSIGNED)
                {
                    if (hadId)
                        Serial.println("[Booster] ID CC retiré; sortie OFF");
                }
                else if (!hadId || previousId != frame.payload[0])
                    Serial.printf("[Booster] ID attribué par le CC : ID_B_%02u\n",
                                  frame.payload[0]);
                sendAck(frame.type, frame.sequence, ACK_OK);
            }
        }
        break;

    case BoosterUart::Type::BOOSTER_DISCOVERY_MODE:
        if (frame.length != 1 || frame.payload[0] > 1)
            sendAck(frame.type, frame.sequence, ACK_INVALID);
        else if (!m_heartbeatSeen || now - m_lastHeartbeatMs >= HEARTBEAT_TIMEOUT_MS)
            sendAck(frame.type, frame.sequence, ACK_NO_HEARTBEAT);
        else
        {
            booster.setDiscoveryPermit(frame.payload[0] != 0);
            m_lastDiscoverySequence = frame.sequence;
            sendAck(frame.type, frame.sequence, ACK_OK);
        }
        break;

    case BoosterUart::Type::BOOSTER_ASSOCIATION_VALIDATE:
        if (frame.length != 8 || frame.payload[0] != 1 ||
            (frame.payload[1] == 0 && frame.payload[2] == 0) ||
            frame.payload[3] < 1 || frame.payload[3] > 252 ||
            frame.payload[4] != 0 || frame.payload[5] != 0 ||
            frame.payload[6] != 0 || frame.payload[7] != 0)
            sendAck(frame.type, frame.sequence, ACK_INVALID);
        else if (!m_heartbeatSeen || now - m_lastHeartbeatMs >= HEARTBEAT_TIMEOUT_MS)
            sendAck(frame.type, frame.sequence, ACK_NO_HEARTBEAT);
        else if (!booster.hasBoosterId())
            sendAck(frame.type, frame.sequence, ACK_NO_ID);
        else if (!booster.hasFreshDiscoveryPermit())
            break; // Sans autorisation Exploration fraîche, ne pas valider.
        else if (frame.payload[3] != booster.boosterId())
            sendAck(frame.type, frame.sequence, ACK_INVALID);
        else
        {
            booster.markAssociationValidated();
            m_lastAssociationSequence = frame.sequence;
            sendAck(frame.type, frame.sequence, ACK_OK);
        }
        break;

    case BoosterUart::Type::CLEAR_CANTON_ID:
        if (frame.length != 0)
            sendAck(frame.type, frame.sequence, ACK_INVALID);
        else if (!m_heartbeatSeen || now - m_lastHeartbeatMs >= HEARTBEAT_TIMEOUT_MS)
            sendAck(frame.type, frame.sequence, ACK_NO_HEARTBEAT);
        else
        {
            booster.clearBoosterId();
            sendAck(frame.type, frame.sequence, ACK_OK);
        }
        break;

    case BoosterUart::Type::SET_POWER:
        if (frame.length != 1 || frame.payload[0] > 1)
            sendAck(frame.type, frame.sequence, ACK_INVALID);
        else if (!m_heartbeatSeen || now - m_lastHeartbeatMs >= HEARTBEAT_TIMEOUT_MS)
            sendAck(frame.type, frame.sequence, ACK_NO_HEARTBEAT);
        else if (frame.payload[0] != 0 && !booster.hasBoosterId())
            sendAck(frame.type, frame.sequence, ACK_NO_ID);
        else
        {
            booster.setPowerEnabled(frame.payload[0] != 0);
            sendAck(frame.type, frame.sequence, ACK_OK);
        }
        break;

    case BoosterUart::Type::DCC_REFERENCE:
        // Le payload est le profil LaBox 0xB7 relayé sans modification par
        // l’EXCC. Les profils invalides sont conservés comme défaut, sans
        // renouveler la fraîcheur de la dernière référence valide.
        booster.setDccReference(frame.payload, frame.length);
        break;

    case BoosterUart::Type::PROFILE_SETPOINT:
    {
        if (frame.length != cfc::dcc::SERVICE_DLC ||
            frame.payload[0] != cfc::dcc::PROTOCOL_VERSION ||
            frame.payload[1] != cfc::dcc::PROFILE_SETPOINT_TARGET_ALL_CC ||
            frame.payload[7] != 0)
        {
            sendAck(frame.type, frame.sequence, ACK_INVALID);
            break;
        }

        const uint16_t transaction = static_cast<uint16_t>(frame.payload[5]) |
            (static_cast<uint16_t>(frame.payload[6]) << 8);
        if (transaction == 0)
        {
            sendAck(frame.type, frame.sequence, ACK_INVALID);
            break;
        }

        if (m_profileSetpointValid && transaction == m_profileSetpointTransaction)
        {
            bool sameRequest = true;
            for (uint8_t i = 0; i < cfc::dcc::SERVICE_DLC; ++i)
                sameRequest = sameRequest && frame.payload[i] == m_profileSetpointRequest[i];
            sendAck(frame.type, frame.sequence, ACK_OK);
            if (sameRequest && m_profileSetpointResultPending)
            {
                if (sendFrame(m_pendingProfileSetpointResultFrame))
                    m_lastProfileSetpointResultTxMs = now;
            }
            else
                queueProfileSetpointResult(transaction, sameRequest
                    ? m_profileSetpointResult
                    : static_cast<uint8_t>(
                        BoosterUart::ProfileSetpointResult::TRANSACTION_CONFLICT));
            break;
        }

        for (uint8_t i = 0; i < cfc::dcc::SERVICE_DLC; ++i)
            m_profileSetpointRequest[i] = frame.payload[i];
        m_profileSetpointTransaction = transaction;
        m_profileSetpointValid = true;

        uint8_t result = static_cast<uint8_t>(
            BoosterUart::ProfileSetpointResult::APPLICATION_FAILED);
        if (!m_heartbeatSeen || now - m_lastHeartbeatMs >= HEARTBEAT_TIMEOUT_MS)
            result = static_cast<uint8_t>(
                BoosterUart::ProfileSetpointResult::APPLICATION_FAILED);
        else if (!booster.hasFreshDiscoveryPermit())
            result = static_cast<uint8_t>(
                BoosterUart::ProfileSetpointResult::NOT_IN_EXPLORATION);
        else if (frame.payload[2] != cfc::dcc::TRACK_PROFILE_N &&
                 frame.payload[2] != cfc::dcc::TRACK_PROFILE_HO)
            result = static_cast<uint8_t>(
                BoosterUart::ProfileSetpointResult::INVALID_PROFILE);
        else
        {
            const uint16_t setpoint = static_cast<uint16_t>(frame.payload[3]) |
                (static_cast<uint16_t>(frame.payload[4]) << 8);
            const uint16_t expected = frame.payload[2] == cfc::dcc::TRACK_PROFILE_N
                ? 12000 : 15000;
            if (setpoint != expected)
                result = static_cast<uint8_t>(
                    BoosterUart::ProfileSetpointResult::INVALID_SETPOINT);
            else if (booster.isPowerEnabled())
                result = static_cast<uint8_t>(
                    BoosterUart::ProfileSetpointResult::APPLICATION_FAILED);
            else if (booster.setTrackProfile(
                         frame.payload[2] == cfc::dcc::TRACK_PROFILE_HO))
                result = static_cast<uint8_t>(
                    BoosterUart::ProfileSetpointResult::ACCEPTED);
        }

        sendAck(frame.type, frame.sequence, ACK_OK);
        m_profileSetpointResult = result;
        queueProfileSetpointResult(transaction, result);
        break;
    }

    case BoosterUart::Type::DCC_TIMING_SET:
        if (frame.length != 4 ||
            (frame.payload[0] == 0 && frame.payload[1] == 0))
        {
            sendAck(frame.type, frame.sequence, ACK_INVALID);
            break;
        }
        {
            const uint16_t transaction = static_cast<uint16_t>(frame.payload[0]) |
                (static_cast<uint16_t>(frame.payload[1]) << 8);
            if (m_timingResultPending &&
                static_cast<uint16_t>(m_pendingTimingResultFrame.payload[0] |
                    (static_cast<uint16_t>(m_pendingTimingResultFrame.payload[1]) << 8)) ==
                    transaction)
            {
                sendAck(frame.type, frame.sequence, ACK_OK);
                break;
            }
            int8_t saved1 = booster.timingCorrectionHalfBit1Us();
            int8_t saved0 = booster.timingCorrectionHalfBit0Us();
            const auto result = booster.setTimingCorrections(
                transaction, static_cast<int8_t>(frame.payload[2]),
                static_cast<int8_t>(frame.payload[3]), saved1, saved0);
            sendAck(frame.type, frame.sequence, ACK_OK);
            queueTimingResult(transaction, static_cast<uint8_t>(result), saved1, saved0);
        }
        break;

    case BoosterUart::Type::DCC_TIMING_READ:
        if (frame.length != 2 ||
            (frame.payload[0] == 0 && frame.payload[1] == 0))
        {
            sendAck(frame.type, frame.sequence, ACK_INVALID);
            break;
        }
        {
            const uint16_t transaction = static_cast<uint16_t>(frame.payload[0]) |
                (static_cast<uint16_t>(frame.payload[1]) << 8);
            if (m_timingResultPending &&
                static_cast<uint16_t>(m_pendingTimingResultFrame.payload[0] |
                    (static_cast<uint16_t>(m_pendingTimingResultFrame.payload[1]) << 8)) ==
                    transaction)
            {
                sendAck(frame.type, frame.sequence, ACK_OK);
                break;
            }
            sendAck(frame.type, frame.sequence, ACK_OK);
            queueTimingResult(transaction,
                static_cast<uint8_t>(cfc::dcc::TimingResult::APPLIED),
                booster.timingCorrectionHalfBit1Us(),
                booster.timingCorrectionHalfBit0Us());
        }
        break;

    case BoosterUart::Type::ACK:
        if (frame.length == 4 && m_timingResultPending &&
            frame.payload[0] == static_cast<uint8_t>(BoosterUart::Type::DCC_TIMING_RESULT) &&
            static_cast<uint16_t>(frame.payload[1] |
                (static_cast<uint16_t>(frame.payload[2]) << 8)) ==
                m_pendingTimingResultFrame.sequence && frame.payload[3] == ACK_OK)
            m_timingResultPending = false;
        if (frame.length == 4 && m_profileSetpointResultPending &&
            frame.payload[0] == static_cast<uint8_t>(
                BoosterUart::Type::PROFILE_SETPOINT_RESULT) &&
            static_cast<uint16_t>(frame.payload[1] |
                (static_cast<uint16_t>(frame.payload[2]) << 8)) ==
                m_pendingProfileSetpointResultFrame.sequence)
        {
            if (frame.payload[3] == ACK_OK)
                m_profileSetpointResultPending = false;
            else
                m_lastProfileSetpointResultTxMs = now - PROFILE_RESULT_RETRY_MS;
        }
        if (frame.length == 4 && m_diagnosticPending &&
            frame.payload[0] == static_cast<uint8_t>(BoosterUart::Type::DCC_DIAGNOSTIC) &&
            static_cast<uint16_t>(frame.payload[1] |
                (static_cast<uint16_t>(frame.payload[2]) << 8)) ==
                m_pendingDiagnosticFrame.sequence && frame.payload[3] == ACK_OK)
        {
            m_reportedDiagnosticCause = m_pendingDiagnosticFrame.payload[1];
            m_reportedDiagnosticActive = m_pendingDiagnosticFrame.payload[2] != 0;
            m_diagnosticPending = false;
        }
        break;

    default:
        break;
    }
}

void BoosterUartLink::sendPendingTimingResult(uint32_t nowMs)
{
    if (m_timingResultPending &&
        nowMs - m_lastTimingResultTxMs >= DIAGNOSTIC_RETRY_MS &&
        sendFrame(m_pendingTimingResultFrame))
        m_lastTimingResultTxMs = nowMs;
}

void BoosterUartLink::queueTimingResult(uint16_t transaction, uint8_t result,
                                        int8_t saved1, int8_t saved0)
{
    BoosterUart::Frame &response = m_pendingTimingResultFrame;
    response = BoosterUart::Frame{};
    response.type = static_cast<uint8_t>(BoosterUart::Type::DCC_TIMING_RESULT);
    response.sequence = m_txSequence++;
    response.length = 5;
    response.payload[0] = static_cast<uint8_t>(transaction & 0xFFU);
    response.payload[1] = static_cast<uint8_t>(transaction >> 8);
    response.payload[2] = result;
    response.payload[3] = static_cast<uint8_t>(saved1);
    response.payload[4] = static_cast<uint8_t>(saved0);
    m_timingResultPending = true;
    m_lastTimingResultTxMs = millis() - DIAGNOSTIC_RETRY_MS;
}

void BoosterUartLink::queueProfileSetpointResult(uint16_t transaction,
                                                  uint8_t result)
{
    BoosterUart::Frame &response = m_pendingProfileSetpointResultFrame;
    response = BoosterUart::Frame{};
    response.type = static_cast<uint8_t>(
        BoosterUart::Type::PROFILE_SETPOINT_RESULT);
    response.sequence = m_txSequence++;
    response.length = 3;
    response.payload[0] = static_cast<uint8_t>(transaction & 0xFFU);
    response.payload[1] = static_cast<uint8_t>(transaction >> 8);
    response.payload[2] = result;
    m_profileSetpointResultPending = true;
    m_lastProfileSetpointResultTxMs = millis() - PROFILE_RESULT_RETRY_MS;
}

void BoosterUartLink::sendPendingProfileSetpointResult(uint32_t nowMs)
{
    if (m_profileSetpointResultPending &&
        nowMs - m_lastProfileSetpointResultTxMs >= PROFILE_RESULT_RETRY_MS &&
        sendFrame(m_pendingProfileSetpointResultFrame))
        m_lastProfileSetpointResultTxMs = nowMs;
}

void BoosterUartLink::sendAck(
    uint8_t command,
    uint16_t commandSequence,
    uint8_t status)
{
    BoosterUart::Frame frame;
    frame.type = static_cast<uint8_t>(BoosterUart::Type::ACK);
    frame.sequence = m_txSequence++;
    frame.length = 4;
    frame.payload[0] = command;
    frame.payload[1] = static_cast<uint8_t>(commandSequence & 0xFFU);
    frame.payload[2] = static_cast<uint8_t>(commandSequence >> 8);
    frame.payload[3] = status;
    sendFrame(frame);
}

bool BoosterUartLink::sendFrame(const BoosterUart::Frame &frame)
{
    uint8_t buffer[BoosterUart::MAX_FRAME_SIZE];
    const size_t length = BoosterUart::encode(frame, buffer, sizeof(buffer));
    if (length == 0 || Serial1.availableForWrite() < static_cast<int>(length))
        return false;
    return Serial1.write(buffer, length) == length;
}

void BoosterUartLink::updateDccDiagnostic(const BoosterCore &booster,
                                          uint32_t nowMs)
{
    if (!booster.hasBoosterId()) return;

    if (!m_diagnosticPending) {
        const uint8_t desiredCause = static_cast<uint8_t>(booster.dccFaultCause());
        const bool desiredActive = desiredCause != 0;
        if (desiredCause != m_reportedDiagnosticCause ||
            desiredActive != m_reportedDiagnosticActive) {
            BoosterUart::Frame &frame = m_pendingDiagnosticFrame;
            frame = BoosterUart::Frame{};
            frame.type = static_cast<uint8_t>(BoosterUart::Type::DCC_DIAGNOSTIC);
            frame.sequence = m_txSequence++;
            frame.length = 8;
            frame.payload[0] = booster.boosterId();
            frame.payload[1] = desiredActive ? desiredCause : m_reportedDiagnosticCause;
            frame.payload[2] = desiredActive ? 1 : 0;
            const bool hasSequence = desiredActive &&
                desiredCause == static_cast<uint8_t>(cfc::dcc::BoosterDccCause::SEQUENCE_GAP);
            const uint16_t expected = hasSequence
                ? booster.getCanRx().faultExpectedSequence() : 0;
            const uint16_t received = hasSequence
                ? booster.getCanRx().faultReceivedSequence() : 0;
            frame.payload[3] = hasSequence ? 0x01 : 0;
            frame.payload[4] = static_cast<uint8_t>(expected & 0xFFU);
            frame.payload[5] = static_cast<uint8_t>(expected >> 8);
            frame.payload[6] = static_cast<uint8_t>(received & 0xFFU);
            frame.payload[7] = static_cast<uint8_t>(received >> 8);
            m_diagnosticPending = true;
            m_lastDiagnosticTxMs = nowMs - DIAGNOSTIC_RETRY_MS;
        }
    }

    if (m_diagnosticPending &&
        nowMs - m_lastDiagnosticTxMs >= DIAGNOSTIC_RETRY_MS &&
        sendFrame(m_pendingDiagnosticFrame))
        m_lastDiagnosticTxMs = nowMs;
}

void BoosterUartLink::sendSample(BoosterCore &booster)
{
    const BoosterTelemetry &telemetry = booster.getTelemetry();
    const bool dccAlive = booster.getCanRx().dccAlive();

    BoosterUart::Frame frame;
    frame.type = static_cast<uint8_t>(BoosterUart::Type::SAMPLE);
    frame.sequence = m_txSequence++;
    frame.length = 7;
    frame.payload[0] = static_cast<uint8_t>(telemetry.current_mA & 0xFFU);
    frame.payload[1] = static_cast<uint8_t>(telemetry.current_mA >> 8);
    frame.payload[2] = static_cast<uint8_t>(telemetry.voltage_mV & 0xFFU);
    frame.payload[3] = static_cast<uint8_t>(telemetry.voltage_mV >> 8);
    frame.payload[4] = static_cast<uint8_t>(telemetry.state);
    frame.payload[5] = static_cast<uint8_t>((booster.isPowerEnabled() ? 0x02U : 0U) |
                                            (dccAlive ? 0x04U : 0U));
    frame.payload[6] = static_cast<uint8_t>(
        dccAlive && booster.isPowerEnabled() && telemetry.state == BoosterState::OK);
    sendFrame(frame);
}

void BoosterUartLink::sendRailVoltage(const BoosterCore &booster)
{
    const BoosterTelemetry &telemetry = booster.getTelemetry();
    BoosterUart::Frame frame;
    frame.type = static_cast<uint8_t>(BoosterUart::Type::BOOSTER_RAIL_VOLTAGE);
    frame.sequence = m_txSequence++;
    frame.length = 3;
    frame.payload[0] = static_cast<uint8_t>(telemetry.railVoltage_mV & 0xFFU);
    frame.payload[1] = static_cast<uint8_t>(telemetry.railVoltage_mV >> 8);
    frame.payload[2] = telemetry.railVoltageValid
        ? cfc::dcc::BOOSTER_RAIL_VOLTAGE_VALID
        : cfc::dcc::BOOSTER_RAIL_VOLTAGE_INVALID;
    if (sendFrame(frame)) m_lastRailVoltageTxMs = millis();
}

void BoosterUartLink::sendRailCom(const BoosterCore &booster)
{
    const uint16_t address = booster.getTelemetry().railcomAddress;
    if (address == 0 || address == m_lastRailcomAddress)
        return;

    BoosterUart::Frame frame;
    frame.type = static_cast<uint8_t>(BoosterUart::Type::RAILCOM);
    frame.sequence = m_txSequence++;
    frame.length = 3;
    frame.payload[0] = 0;
    frame.payload[1] = static_cast<uint8_t>(address & 0xFFU);
    frame.payload[2] = static_cast<uint8_t>(address >> 8);
    if (sendFrame(frame))
        m_lastRailcomAddress = address;
}
