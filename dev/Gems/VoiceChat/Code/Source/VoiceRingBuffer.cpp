#include "VoiceChat_precompiled.h"
#include "VoiceRingBuffer.h"

namespace VoiceChat
{
    VoiceRingBuffer::VoiceRingBuffer() : VoiceRingBuffer(voice_traits::VOICE_RING_BUFFER_SIZE) {}

    VoiceRingBuffer::VoiceRingBuffer(size_t capacity)
    {
        Resize(capacity);
    }

    void VoiceRingBuffer::Resize(size_t capacity)
    {
        m_totalSize = capacity;

        m_buffer.resize(m_totalSize, 0);
        m_pushPos = 0;
        m_fetchPos = 0;
        m_filledSize = 0;
    }

    void VoiceRingBuffer::Push(const AZ::u8* data, size_t size)
    {
        while (size > 0)
        {
            size_t toCopy = AZStd::min<size_t>(size, m_totalSize - m_pushPos);
            memcpy(m_buffer.data() + m_pushPos, data, toCopy);

            size -= toCopy;
            data += toCopy;

            m_pushPos += toCopy;
            m_pushPos %= m_totalSize;
            m_filledSize += toCopy;

            if (m_filledSize > m_totalSize)
            {
                if (m_fetchPos < m_pushPos)
                {
                    AZ_Warning("VoiceChat", false, "Override non read data with %i", (int)m_filledSize);
                    m_fetchPos = m_pushPos;
                }
                m_filledSize = m_totalSize;
            }
        }
    }

    size_t VoiceRingBuffer::Pop(AZ::u8* data, size_t size)
    {
        size_t offset = 0;
        size_t actualFetched = AZStd::min<size_t>(m_filledSize, size);

        size = actualFetched;

        while (size > 0)
        {
            size_t toFetch = AZStd::min<size_t>(m_totalSize - m_fetchPos, size);
            memcpy(data + offset, m_buffer.data() + m_fetchPos, toFetch);

            size -= toFetch;
            offset += toFetch;

            m_fetchPos += toFetch;
            m_fetchPos %= m_totalSize;
            m_filledSize -= toFetch;
        }
        return actualFetched;
    }
}
