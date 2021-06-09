#include "VoiceChat_precompiled.h"
#include "VoicePlayer.h"
#include "VoiceChatCVars.h"
#include "VoicePacket.h"
#include "VoicePlayerDriverBus.h"
#include "VoiceFilters.h"

#include <AzCore/Serialization/SerializeContext.h>

namespace VoiceChat
{
    void VoicePlayer::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VoicePlayer"));
    }

    void VoicePlayer::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("VoicePlayer"));
    }

    void VoicePlayer::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<VoicePlayer, AZ::Component>()->Version(0);
        }
    }

    void VoicePlayer::Activate()
    {
        VoiceChatNetworkEventBus::Handler::BusConnect();
    }

    void VoicePlayer::Deactivate()
    {
        VoiceChatNetworkEventBus::Handler::BusDisconnect();
        StopPlayer();
    }

    void VoicePlayer::StartPlayer()
    {
        m_playerTick.Start([this]()
        {
            VoiceBuffer pkt;
            {
                AZStd::lock_guard<AZStd::mutex> lock(m_playMutex);
                m_voiceBuffer.Pop(pkt);
            }

            if (!pkt.empty())
            {
                if (g_pVoiceChatCVars->voice_chat_network_debug)
                {
                    AZ_TracePrintf("VoiceChat", "Play audio buffer with size %i", (int)pkt.size());
                }

                EBUS_EVENT(VoicePlayerDriverRequestBus, PlayAudio, pkt.data(), pkt.size());
            }
        });
    }

    void VoicePlayer::StopPlayer()
    {
        m_playerTick.Stop();
    }

    void VoicePlayer::OnVoice(const AZ::u8* data, size_t dataSize)
    {
        if (g_pVoiceChatCVars->voice_client_chat_mute)
        {
            return;
        }

        if (dataSize > 0)
        {
            StartPlayer();

            AZStd::lock_guard<AZStd::mutex> lock(m_playMutex);

            if (g_pVoiceChatCVars->voice_player_volume_enabled)
            {
                VoiceBuffer voiceBuf(data, data + dataSize);
                voice_filters::ApplyVolume(voiceBuf.data(), voiceBuf.size(), g_pVoiceChatCVars->voice_player_volume);

                m_voiceBuffer.Push(voiceBuf.data(), voiceBuf.size());
            }
            else
            {
                m_voiceBuffer.Push(data, dataSize);
            }

            if (g_pVoiceChatCVars->voice_chat_network_debug)
            {
                AZ_TracePrintf("VoiceChat", "Voice buffer increased to %i", (int)m_voiceBuffer.Size());
            }
        }
    }
}
