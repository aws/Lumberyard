#pragma once

#include <AzCore/Component/Component.h>
#include <CrySystemBus.h>

#include <ISystem.h>

namespace VoiceChat
{
    class VoiceChatCVars;
    extern VoiceChatCVars* g_pVoiceChatCVars;

    class VoiceChatCVars
        : public AZ::Component
        , public CrySystemEventBus::Handler
    {
    public:
        AZ_COMPONENT(VoiceChatCVars, "{16A6AFAB-0DCB-4CBA-8DBC-9988C8760E5D}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    private:
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;

        // CrySystemEventBus interface implementation
        void OnCrySystemInitialized(ISystem&, const SSystemInitParams&) override;

    public:
        int voice_jitter_tick_msec;
        int voice_jitter_max;
        int voice_jitter_cooldown_msec;

        int voice_network_queue_limit;

        int voice_client_chat_mute;
        float voice_client_activation_level;
        int voice_client_activation_decay_msec;

        int voice_recorder_low_pass_enabled;
        int voice_recorder_stop_decay_msec;

        float voice_player_volume;
        float voice_player_volume_enabled;

        int voice_player_driver_block_size_msec;
        int voice_player_driver_blocks_count;

        int voice_chat_enabled;
        int voice_chat_debug;
        int voice_chat_network_debug;

        int voice_dump_disk;
    };
}
