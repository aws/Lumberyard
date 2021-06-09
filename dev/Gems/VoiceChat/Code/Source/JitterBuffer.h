#pragma once

#include "VoicePacket.h"

#include <chrono>

namespace VoiceChat
{
    class JitterBuffer
    {
    public:
        void Push(const AZ::u8* data, size_t size);
        bool Pop(VoiceBuffer& pkt);

        size_t Size() const { return m_buffer.size(); }

    private:
        VoiceQueue m_buffer;
        std::chrono::time_point<std::chrono::steady_clock> m_cooldown;

        enum class State { None, Normal, Overflow, Underflow };
        State m_state{ State::None };
    };

    class JitterThread
    {
    public:
        JitterThread();
        virtual ~JitterThread();

        void Start(std::function<void()>&& onTick);
        void Stop();

    private:
        void Sleep();

    private:
        std::chrono::time_point<std::chrono::steady_clock> m_lastTick;
        AZStd::thread m_tickThread;
        AZStd::atomic_bool m_threadTick{ false };
        std::function<void()> m_onTickFunc;
    };
}
