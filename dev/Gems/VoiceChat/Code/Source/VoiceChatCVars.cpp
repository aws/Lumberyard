#include "VoiceChat_precompiled.h"
#include "VoiceChatCVars.h"

#include <AzCore/Serialization/SerializeContext.h>

#include <IConsole.h>

namespace VoiceChat
{
    VoiceChatCVars* g_pVoiceChatCVars = nullptr;

    void VoiceChatCVars::Activate()
    {
        CrySystemEventBus::Handler::BusConnect();
    }

    void VoiceChatCVars::Deactivate()
    {
        CrySystemEventBus::Handler::BusDisconnect();
    }

    void VoiceChatCVars::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<VoiceChatCVars, AZ::Component>()->Version(0);
        }
    }

    void VoiceChatCVars::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VoiceChatCVars"));
    }

    void VoiceChatCVars::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("VoiceChatCVars"));
    }

    void VoiceChatCVars::OnCrySystemInitialized(ISystem&, const SSystemInitParams&)
    {
        g_pVoiceChatCVars = this;

        REGISTER_CVAR2("voice_jitter_tick_msec", &voice_jitter_tick_msec, 100, VF_NULL, "Tick period of voice jitter buffer");

        REGISTER_CVAR2("voice_jitter_tick_msec", &voice_jitter_tick_msec, 100, VF_NULL, "Tick period of voice jitter buffer");
        REGISTER_CVAR2("voice_jitter_max", &voice_jitter_max, 6, VF_NULL, "Maximum number of voice packet handled by jitter buffer");
        REGISTER_CVAR2("voice_jitter_cooldown_msec", &voice_jitter_cooldown_msec, 200, VF_NULL, "Cooldown period to reduce jitter threshold");

        REGISTER_CVAR2("voice_network_queue_limit", &voice_network_queue_limit, 100, VF_NULL, "Maximum number of voice network queue size");

        REGISTER_CVAR2("voice_client_chat_mute", &voice_client_chat_mute, 0, VF_NULL, "Mute/unmute voice chat");
        REGISTER_CVAR2("voice_client_activation_level", &voice_client_activation_level, 0.0f, VF_NULL, "Send voice only if voice level higher");
        REGISTER_CVAR2("voice_client_activation_decay_msec", &voice_client_activation_decay_msec, 200, VF_NULL, "Decay voice duration when no sound is available");

        REGISTER_CVAR2("voice_recorder_low_pass_enabled", &voice_recorder_low_pass_enabled, 0, VF_NULL, "Enable/disable low pass filter on microphone");
        REGISTER_CVAR2("voice_recorder_push2talk_decay_msec", &voice_recorder_stop_decay_msec, 400, VF_NULL, "How long to record voice after push2talk is over");

        REGISTER_CVAR2("voice_player_volume", &voice_player_volume, 1.0f, VF_NULL, "Client volume from 0 to 1.5f");
        REGISTER_CVAR2("voice_player_volume_enabled", &voice_player_volume_enabled, 1, VF_NULL, "Enable/disable volume filter on player");

        REGISTER_CVAR2("voice_player_driver_block_size_msec", &voice_player_driver_block_size_msec, 100, VF_REQUIRE_APP_RESTART, "Start playing PCM block with enough data cached");
        REGISTER_CVAR2("voice_player_driver_blocks_count", &voice_player_driver_blocks_count, 6, VF_REQUIRE_APP_RESTART, "Total count of audio driver blocks");

        REGISTER_CVAR2("voice_chat_enabled", &voice_chat_enabled, 1, VF_REQUIRE_APP_RESTART, "Enable/disable voice chat");
        REGISTER_CVAR2("voice_chat_debug", &voice_chat_debug, 1, VF_DEV_ONLY, "Enable/disable voice chat debug info");
        REGISTER_CVAR2("voice_chat_network_debug", &voice_chat_network_debug, 0, VF_DEV_ONLY, "Enable/disable printing of voice network packets");

        REGISTER_CVAR2("voice_dump_disk", &voice_dump_disk, 0, VF_DEV_ONLY, "Enable pcm dump on server");

        // disable voice chat by default in the Editor
        if (gEnv->IsEditor())
        {
            g_pVoiceChatCVars->voice_chat_enabled = 0;
        }
    }
}
