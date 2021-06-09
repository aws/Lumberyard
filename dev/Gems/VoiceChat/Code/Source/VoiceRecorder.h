#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <MicrophoneBus.h>

#include <VoiceChat/VoiceChatBus.h>

#include "VoiceRingBuffer.h"
#include "JitterBuffer.h"

#include <chrono>

namespace VoiceChat
{
    class VoiceRecorder
        : public AZ::Component
        , private VoiceChatRequestBus::Handler
        , private AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(VoiceRecorder, "{4B289831-31C8-4A7B-9448-21336079C587}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    private:
        // AZ::Component interface implementation
        virtual void Init() override;
        virtual void Activate() override;
        virtual void Deactivate() override;

        ////////////////////////////////////////////////////////////////////////
        // VoiceChatRequestBus interface implementation
        virtual void StartRecording() override;
        virtual void StopRecording() override;
        virtual void ToggleRecording() override;
        virtual void TestRecording(const char* sampleName) override;

        // AZ::TickBus
        virtual void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        ////////////////////////////////////////////////////////////////////////

        void PauseRecording(bool force);
        void ReadBuffer();
        void ReadTestBuffer(float deltaTime);

        size_t SendVoiceBuffer(const AZ::u8* data, size_t dataSize);

        void InitRecorder();

    private:
        enum class State { Stopped, Started, Stopping };

    private:
        AZ::u8* m_voiceBuffer{ nullptr };
        AZ::u8* m_voiceBufferPos{ nullptr };
        Audio::SAudioInputConfig m_bufferConfig;

        JitterThread m_recorderTick;
        JitterBuffer m_recordedBuffer;
        AZStd::mutex m_sendMutex;

        State m_isRecordingState{ State::Stopped };
        std::chrono::time_point<std::chrono::steady_clock> m_lastActiveVoice;
        std::chrono::time_point<std::chrono::steady_clock> m_stopRecording;

        bool m_isTestMode{ false };
        float m_testDelta{ 0 };
        VoiceRingBuffer m_testData;
    };
}
