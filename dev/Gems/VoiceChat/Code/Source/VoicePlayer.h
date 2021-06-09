#pragma once

#include <AzCore/Component/Component.h>

#include "VoiceChatNetworkBus.h"
#include "VoicePacket.h"
#include "JitterBuffer.h"

namespace VoiceChat
{
    class VoicePlayer
        : public AZ::Component
        , private VoiceChatNetworkEventBus::Handler
    {
    public:
        AZ_COMPONENT(VoicePlayer, "{5FED6B22-6E35-4B2F-9F3F-8FADDDA7E3F6}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        // VoiceChatNetworkEventBus interface implementation
        void OnVoice(const AZ::u8* data, size_t dataSize) override;

        void StartPlayer();
        void StopPlayer();

    private:
        JitterBuffer m_voiceBuffer;
        JitterThread m_playerTick;
        AZStd::mutex m_playMutex;
    };
}
