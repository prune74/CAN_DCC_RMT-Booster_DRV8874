#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BoosterUart
{
constexpr uint8_t VERSION = 2;
constexpr uint8_t SYNC_0 = 0xA5;
constexpr uint8_t SYNC_1 = 0x5A;
constexpr uint8_t MAX_PAYLOAD = 32;
constexpr uint8_t MAX_CANTON_ID = 252;
constexpr uint8_t CANTON_ID_UNASSIGNED = 0xFF;
constexpr size_t HEADER_SIZE = 7;
constexpr size_t CRC_SIZE = 2;
constexpr size_t MAX_FRAME_SIZE = HEADER_SIZE + MAX_PAYLOAD + CRC_SIZE;

enum class Type : uint8_t
{
    HEARTBEAT = 0x01,
    SET_POWER = 0x02,
    SET_PROFILE = 0x03,
    ACK = 0x04,
    SET_CANTON_ID = 0x05,
    SAMPLE = 0x10,
    RAILCOM = 0x11
};

struct Frame
{
    uint8_t type = 0;
    uint16_t sequence = 0;
    uint8_t length = 0;
    uint8_t payload[MAX_PAYLOAD] = {};
};

inline uint16_t crc16CcittFalse(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000U) ? static_cast<uint16_t>((crc << 1) ^ 0x1021U)
                                  : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

inline size_t encode(const Frame &frame, uint8_t *out, size_t capacity)
{
    if (out == nullptr || frame.length > MAX_PAYLOAD)
        return 0;

    const size_t total = HEADER_SIZE + frame.length + CRC_SIZE;
    if (capacity < total)
        return 0;

    out[0] = SYNC_0;
    out[1] = SYNC_1;
    out[2] = VERSION;
    out[3] = frame.type;
    out[4] = static_cast<uint8_t>(frame.sequence & 0xFFU);
    out[5] = static_cast<uint8_t>(frame.sequence >> 8);
    out[6] = frame.length;
    for (uint8_t i = 0; i < frame.length; ++i)
        out[HEADER_SIZE + i] = frame.payload[i];

    const uint16_t crc = crc16CcittFalse(out + 2, 5 + frame.length);
    out[HEADER_SIZE + frame.length] = static_cast<uint8_t>(crc & 0xFFU);
    out[HEADER_SIZE + frame.length + 1] = static_cast<uint8_t>(crc >> 8);
    return total;
}

class Parser
{
public:
    bool feed(uint8_t byte, Frame &frame)
    {
        if (m_count == 0)
        {
            if (byte == SYNC_0)
                m_buffer[m_count++] = byte;
            return false;
        }

        if (m_count == 1)
        {
            if (byte == SYNC_1)
                m_buffer[m_count++] = byte;
            else if (byte == SYNC_0)
                m_buffer[0] = byte;
            else
                reset();
            return false;
        }

        if (m_count >= sizeof(m_buffer))
        {
            reset();
            return false;
        }
        m_buffer[m_count++] = byte;

        if (m_count == HEADER_SIZE)
        {
            if (m_buffer[6] > MAX_PAYLOAD)
            {
                reset();
                return false;
            }
            m_expected = HEADER_SIZE + m_buffer[6] + CRC_SIZE;
        }

        if (m_expected == 0 || m_count < m_expected)
            return false;

        const uint8_t payloadLength = m_buffer[6];
        const uint16_t receivedCrc =
            static_cast<uint16_t>(m_buffer[HEADER_SIZE + payloadLength]) |
            (static_cast<uint16_t>(m_buffer[HEADER_SIZE + payloadLength + 1]) << 8);
        const uint16_t calculatedCrc =
            crc16CcittFalse(m_buffer + 2, 5 + payloadLength);
        if (m_buffer[2] != VERSION || receivedCrc != calculatedCrc)
        {
            reset();
            return false;
        }

        frame.type = m_buffer[3];
        frame.sequence = static_cast<uint16_t>(m_buffer[4]) |
                         (static_cast<uint16_t>(m_buffer[5]) << 8);
        frame.length = payloadLength;
        for (uint8_t i = 0; i < payloadLength; ++i)
            frame.payload[i] = m_buffer[HEADER_SIZE + i];
        reset();
        return true;
    }

private:
    void reset()
    {
        m_count = 0;
        m_expected = 0;
    }

    uint8_t m_buffer[MAX_FRAME_SIZE] = {};
    size_t m_count = 0;
    size_t m_expected = 0;
};
} // namespace BoosterUart
