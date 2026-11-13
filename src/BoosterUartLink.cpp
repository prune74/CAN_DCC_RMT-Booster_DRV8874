#include "BoosterUartLink.h"

#include "Pins.h"

#include <Arduino.h>

namespace
{
constexpr uint32_t UART_BAUD = 460800;
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 250;
constexpr uint32_t SAMPLE_PERIOD_US = 1000;

constexpr uint8_t ACK_OK = 0;
constexpr uint8_t ACK_INVALID = 1;
constexpr uint8_t ACK_NO_HEARTBEAT = 2;
constexpr uint8_t ACK_NO_ID = 3;
}

void BoosterUartLink::begin()
{
    Serial1.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
    m_lastSampleUs = micros();
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
        booster.setBoosterId(CFG_CANTON_ID_UNASSIGNED);
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

    case BoosterUart::Type::SET_PROFILE:
        if (frame.length != 1 || frame.payload[0] > 1)
            sendAck(frame.type, frame.sequence, ACK_INVALID);
        else if (!m_heartbeatSeen || now - m_lastHeartbeatMs >= HEARTBEAT_TIMEOUT_MS)
            sendAck(frame.type, frame.sequence, ACK_NO_HEARTBEAT);
        else
        {
            booster.setTrackProfile(frame.payload[0] == 1);
            sendAck(frame.type, frame.sequence, ACK_OK);
        }
        break;

    default:
        break;
    }
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
