#include "VoiceChat_precompiled.h"
#include "VoiceRecorder.h"
#include "VoicePacket.h"
#include "VoiceChatNetworkBus.h"
#include "VoiceFilters.h"
#include "VoiceChatCVars.h"

#include <AzCore/Serialization/SerializeContext.h>

#include <ICryPak.h>
#include <I3DEngine.h>

using namespace Audio;

namespace VoiceChat
{
    static const float kTextPosXOffset = 80.0f;
    static const float kTextPosYOffset = 20.0f;
    static const float kTextScale = 1.4f;

    void VoiceRecorder::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VoiceRecorder"));
    }

    void VoiceRecorder::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("VoiceRecorder"));
    }

    void VoiceRecorder::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<VoiceRecorder, AZ::Component>()->Version(0);
        }
    }

    void VoiceRecorder::Init()
    {
        m_bufferConfig = SAudioInputConfig(AudioInputSourceType::Microphone,
            voice_traits::SAMPLE_RATE, voice_traits::CHANNELS,
            voice_traits::BITS_PER_SAMPLE, AudioInputSampleType::Int);
    }

    void VoiceRecorder::Activate()
    {
        VoiceChatRequestBus::Handler::BusConnect();
    }

    void VoiceRecorder::Deactivate()
    {
        m_recorderTick.Stop();

        VoiceChatRequestBus::Handler::BusDisconnect();

        StopRecording();

        if (m_voiceBuffer)
        {
            delete[] m_voiceBuffer;
            m_voiceBuffer = nullptr;
            m_voiceBufferPos = nullptr;
        }
    }

    void VoiceRecorder::InitRecorder()
    {
        if (!m_voiceBuffer)
        {
            m_voiceBuffer = new AZ::u8[voice_traits::RECORDER_BUFFER_ALLOC_SIZE];
            m_voiceBufferPos = m_voiceBuffer;

            m_recorderTick.Start([this]()
            {
                VoiceBuffer pkt;
                {
                    AZStd::lock_guard<AZStd::mutex> lock(m_sendMutex);
                    m_recordedBuffer.Pop(pkt);
                }

                if (!pkt.empty())
                {
                    EBUS_EVENT(VoiceChatNetworkRequestBus, SendVoice, pkt.data(), pkt.size());
                }
            });
        }
    }

    void VoiceRecorder::StartRecording()
    {
        if (!g_pVoiceChatCVars->voice_chat_enabled)
        {
            return;
        }

        InitRecorder();

        if (m_isRecordingState == State::Stopped || m_isRecordingState == State::Stopping)
        {
            bool isStarted = false;
            MicrophoneRequestBus::BroadcastResult(isStarted, &MicrophoneRequestBus::Events::IsCapturing);

            if (!isStarted)
            {
                MicrophoneRequestBus::BroadcastResult(isStarted, &MicrophoneRequestBus::Events::StartSession);
            }

            if (isStarted)
            {
                AZ::TickBus::Handler::BusConnect();
                m_voiceBufferPos = m_voiceBuffer;
                m_isRecordingState = State::Started;
            }
            else
            {
                AZ_Warning("VoiceChat", false, "Unable to start recording session");
            }
        }
    }

    void VoiceRecorder::StopRecording()
    {
        PauseRecording(true);

        MicrophoneRequestBus::Broadcast(&MicrophoneRequestBus::Events::EndSession);
    }

    void VoiceRecorder::ToggleRecording()
    {
        if (m_isRecordingState == State::Started)
        {
            PauseRecording(false);
        }
        else
        {
            StartRecording();
        }
    }

    void VoiceRecorder::OnTick(float deltaTime, AZ::ScriptTimePoint time)
    {
        FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

        if (m_isRecordingState == State::Started || m_isRecordingState == State::Stopping)
        {
            bool isCapturing = false;

            MicrophoneRequestBus::BroadcastResult(isCapturing, &MicrophoneRequestBus::Events::IsCapturing);
            if (isCapturing)
            {
                if (m_isTestMode)
                {
                    ReadTestBuffer(deltaTime);
                }
                else
                {
                    ReadBuffer();
                }

                if (g_pVoiceChatCVars->voice_chat_debug)
                {
                    I3DEngine* p3DEngine = gEnv ? gEnv->p3DEngine : nullptr;
                    IRenderer* pRenderer = gEnv ? gEnv->pRenderer : nullptr;

                    if (pRenderer && p3DEngine)
                    {
                        p3DEngine->DrawTextRightAligned(kTextPosXOffset, kTextPosYOffset, kTextScale, Col_Green, "Mic on");
                    }
                }
            }

            if (m_isRecordingState == State::Stopping)
            {
                auto current = std::chrono::steady_clock::now();

                auto duration(std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(current - m_stopRecording));
                std::chrono::milliseconds decayDuration(g_pVoiceChatCVars->voice_recorder_stop_decay_msec);

                if (duration > decayDuration)
                {
                    PauseRecording(true);
                }
            }
        }
    }

    void VoiceRecorder::PauseRecording(bool force)
    {
        if (m_isRecordingState == State::Started || m_isRecordingState == State::Stopping)
        {
            if (force)
            {
                AZ::TickBus::Handler::BusDisconnect();
                m_isRecordingState = State::Stopped;

                ReadBuffer();
                m_voiceBufferPos = m_voiceBuffer;
            }
            else
            {
                m_stopRecording = std::chrono::steady_clock::now();
                m_isRecordingState = State::Stopping;
            }
        }
    }

    void VoiceRecorder::ReadBuffer()
    {
        AZ::u32 actualSampleFrames = 0;
        MicrophoneRequestBus::BroadcastResult(actualSampleFrames,
            &MicrophoneRequestBus::Events::GetData,
            reinterpret_cast<void**>(&m_voiceBufferPos),
            voice_traits::RECORDER_FRAME_SIZE,
            m_bufferConfig,
            false);

        AZ::u32 bytesRead = actualSampleFrames * voice_traits::CHANNELS * voice_traits::BYTES_PER_SAMPLE;
        m_voiceBufferPos += bytesRead;

        size_t capturedSize = static_cast<size_t>(m_voiceBufferPos - m_voiceBuffer);
        if (capturedSize > (voice_traits::RECORDER_BUFFER_ALLOC_SIZE - voice_traits::RECORDER_FRAME_SIZE)) // no room for next GetData call
        {
            AZ_Warning("VoiceChat", false, "No room in voice recorder buffer, drop captured data");
            m_voiceBufferPos = m_voiceBuffer;
            return;
        }

        if (g_pVoiceChatCVars->voice_recorder_low_pass_enabled)
        {
            voice_filters::LowPass(m_voiceBuffer, capturedSize);
        }
        size_t sentBytes = SendVoiceBuffer(m_voiceBuffer, capturedSize);

        if (sentBytes > 0)
        {
            if (g_pVoiceChatCVars->voice_chat_network_debug)
            {
                AZ_TracePrintf("VoiceChat", "Recorded %i consumed %i", (int)capturedSize, (int)sentBytes);
            }

            // delay rest of buffer to next record
            if (capturedSize > sentBytes)
            {
                size_t remains = capturedSize - sentBytes;
                memmove(m_voiceBuffer, m_voiceBuffer + sentBytes, remains);
                m_voiceBufferPos = m_voiceBuffer + remains;
            }
            else
            {
                m_voiceBufferPos = m_voiceBuffer;
            }
        }
    }

    size_t VoiceRecorder::SendVoiceBuffer(const AZ::u8* data, size_t dataSize)
    {
        size_t actualSentByte = 0;

        if (dataSize < voice_traits::VOICE_AUDIO_PACKET_SIZE)
        {
            return actualSentByte; // wait for more data
        }

        size_t offset = 0;

        while (true)
        {
            if (offset + voice_traits::VOICE_AUDIO_PACKET_SIZE > dataSize)
            {
                break;
            }

            float voiceLevel = voice_filters::VoiceActiveLevel(data + offset, voice_traits::VOICE_AUDIO_PACKET_SIZE);
            actualSentByte += voice_traits::VOICE_AUDIO_PACKET_SIZE;

            if (voiceLevel >= g_pVoiceChatCVars->voice_client_activation_level)
            {
                if (g_pVoiceChatCVars->voice_chat_network_debug)
                {
                    AZ_TracePrintf("VoiceChat", "Sending voice packet %i with level %f", (int)dataSize, voiceLevel);
                }
                m_lastActiveVoice = std::chrono::steady_clock::now();
            }
            else
            {
                // consider decay timeout to send some period after voice is over
                auto current = std::chrono::steady_clock::now();

                auto duration(std::chrono::duration_cast<std::chrono::duration<int, std::milli>>(current - m_lastActiveVoice));
                std::chrono::milliseconds decayDuration(g_pVoiceChatCVars->voice_client_activation_decay_msec);

                if (duration < decayDuration)
                {
                    if (g_pVoiceChatCVars->voice_chat_network_debug)
                    {
                        AZ_TracePrintf("VoiceChat", "Sending decay voice packet %i with level %f", (int)dataSize, voiceLevel);
                    }
                }
                else
                {
                    continue; // ignore silent blocks after decay period is over
                }
            }

            AZStd::lock_guard<AZStd::mutex> lock(m_sendMutex);
            m_recordedBuffer.Push(data + offset, voice_traits::VOICE_AUDIO_PACKET_SIZE);

            offset += voice_traits::VOICE_AUDIO_PACKET_SIZE;
        }

        // report consumed bytes even if not sent to proceed in buffers anyway
        return actualSentByte;
    }

    void VoiceRecorder::ReadTestBuffer(float deltaTime)
    {
        m_testDelta += deltaTime;
        float cacheLimitSec = voice_traits::VOICE_ENCODER_PACKET_MSEC / 1000.0f;

        if (m_testDelta >= cacheLimitSec)
        {
            AZ::u8 buf[voice_traits::VOICE_AUDIO_PACKET_SIZE];
            size_t capturedSize = sizeof(buf);

            m_testData.Pop(buf, capturedSize);
            m_testData.Push(buf, capturedSize);

            if (g_pVoiceChatCVars->voice_recorder_low_pass_enabled)
            {
                voice_filters::LowPass(buf, capturedSize);
            }

            SendVoiceBuffer(buf, capturedSize);
            m_testDelta -= cacheLimitSec;
        }
    }

    void VoiceRecorder::TestRecording(const char* sampleName)
    {
        m_testDelta = 0;
        m_isTestMode = true;

        auto name = PathUtil::Make("sounds/voip_samples", sampleName);

        CCryFile file;
        if (file.Open(name.c_str(), "rb"))
        {
            const size_t length = file.GetLength();
            if (length > 0)
            {
                std::vector<AZ::u8> samples;

                samples.resize(length, 0);
                m_testData.Resize(length);

                file.ReadRaw(&samples[0], length);
                m_testData.Push(samples.data(), length);
            }
        }
        else
        {
            AZ_Warning("VoiceChat", false, "Failed to read sample %s", sampleName);
        }
    }
}
