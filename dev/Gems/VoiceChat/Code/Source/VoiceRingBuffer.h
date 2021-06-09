#pragma once

#include "VoicePacket.h"

namespace VoiceChat
{
    class VoiceRingBuffer
    {
    public:
        VoiceRingBuffer();
        VoiceRingBuffer(size_t capacity);

        void Resize(size_t capacity);

        void Push(const AZ::u8* data, size_t size);
        size_t Pop(AZ::u8* data, size_t size);

        size_t Size() const { return m_filledSize; }

    private:
        VoiceBuffer m_buffer;

        size_t m_pushPos{ 0 };
        size_t m_fetchPos{ 0 };
        size_t m_totalSize{ 0 };
        size_t m_filledSize{ 0 };
    };
}
