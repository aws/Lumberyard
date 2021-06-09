#include "VoiceChat_precompiled.h"
#include "JitterBuffer.h"

#include "VoiceChatCVars.h"

#include <thread>

namespace VoiceChat
{
    void JitterBuffer::Push(const AZ::u8* data, size_t size)
    {
        if (m_state != State::Overflow)
        {
            size_t jitterSize = g_pVoiceChatCVars->voice_jitter_max;

            if (m_buffer.size() < jitterSize)
            {
                m_buffer.emplace(VoiceBuffer(data, data + size));
                m_cooldown = std::chrono::steady_clock::now();

                if (m_state == State::None)
                {
                    m_state = State::Underflow;
                }

                if (g_pVoiceChatCVars->voice_chat_network_debug)
                {
                    AZ_TracePrintf("VoiceChat", "Push to jitter buffer to size %i", (int)m_buffer.size());
                }
            }
            else
            {
                m_state = State::Overflow;

                if (g_pVoiceChatCVars->voice_chat_network_debug)
                {
                    AZ_TracePrintf("VoiceChat", "Jitter buffer overflow with size %i", (int)m_buffer.size());
                }
            }
        }
    }

    bool JitterBuffer::Pop(VoiceBuffer& pkt)
    {
        if (!m_buffer.empty())
        {
            size_t jitterThreshold = g_pVoiceChatCVars->voice_jitter_max / 2;

            if (m_state == State::Overflow)
            {
                if (m_buffer.size() <= jitterThreshold)
                {
                    m_state = State::Normal;

                    if (g_pVoiceChatCVars->voice_chat_network_debug)
                    {
                        AZ_TracePrintf("VoiceChat", "Jitter buffer from overflow to normal with size %i", (int)m_buffer.size());
                    }
                }
            }
            else if (m_state == State::Underflow)
            {
                if (m_buffer.size() < jitterThreshold)
                {
                    auto now = std::chrono::steady_clock::now();

                    auto duration(std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(now - m_cooldown));
                    std::chrono::milliseconds cooldown(g_pVoiceChatCVars->voice_jitter_cooldown_msec);

                    if (cooldown > duration)
                    {
                        return false;
                    }

                    if (g_pVoiceChatCVars->voice_chat_network_debug)
                    {
                        AZ_TracePrintf("VoiceChat", "Jitter buffer from underflow to normal due to cooldown with size %i", (int)m_buffer.size());
                    }
                }
            }

            if (m_state != State::Normal)
            {
                m_state = State::Normal;

                if (g_pVoiceChatCVars->voice_chat_network_debug)
                {
                    AZ_TracePrintf("VoiceChat", "Jitter buffer to normal state with size %i", (int)m_buffer.size());
                }
            }

            pkt = m_buffer.front();
            m_buffer.pop();

            return true;
        }
        else
        {
            if (m_state != State::None && m_state != State::Underflow)
            {
                m_state = State::Underflow;

                if (g_pVoiceChatCVars->voice_chat_network_debug)
                {
                    AZ_TracePrintf("VoiceChat", "Jitter buffer to underflow");
                }
            }
        }
        return false;
    }

    JitterThread::JitterThread()
    {
        m_lastTick = std::chrono::steady_clock::now();
    }

    JitterThread::~JitterThread()
    {
        Stop();
    }

    void JitterThread::Start(std::function<void()>&& onTick)
    {
        if (!m_threadTick)
        {
            m_threadTick = true;

            m_tickThread = AZStd::thread([onTick, this]()
            {
                while (m_threadTick)
                {
                    onTick();
                    Sleep();
                }
            });
        }
    }

    void JitterThread::Stop()
    {
        if (m_threadTick)
        {
            m_threadTick = false;

            if (m_tickThread.joinable())
            {
                m_tickThread.join();
            }
        }
    }

    void JitterThread::Sleep()
    {
        using milli_t = std::chrono::duration<int, std::milli>;
        std::chrono::milliseconds frame(g_pVoiceChatCVars->voice_jitter_tick_msec);

        auto end = std::chrono::steady_clock::now();
        auto duration(std::chrono::duration_cast<milli_t>(end - m_lastTick));

        if (duration < frame)
        {
            auto delta = frame - duration;
            std::this_thread::sleep_for(delta);
        }
        else
        {
            // at least give a chance to other process with the same priority
            std::this_thread::yield();
        }
        m_lastTick = std::chrono::steady_clock::now();
    }
}
