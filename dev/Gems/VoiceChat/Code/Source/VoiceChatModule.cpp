#include "VoiceChat_precompiled.h"
#include <platform_impl.h>

#include <AzCore/Memory/SystemAllocator.h>

#include "VoiceChatCVars.h"
#include "VoiceTeam.h"
#include "VoiceChatCommands.h"

#if defined(AZ_PLATFORM_WINDOWS) || defined (DEDICATED_SERVER)
    #include "VoiceChatNetwork.h"
    #include "OpusVoiceEncoder.h"
#endif

#if defined(AZ_PLATFORM_WINDOWS)
    #include "VoicePlayer.h"
    #include "VoiceRecorder.h"
    #include "VoicePlayerDriverWin.h"
#endif

#include <IGem.h>

namespace VoiceChat
{
    class VoiceChatModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(VoiceChatModule, "{D039E256-7DE7-409C-A98F-07DCC8548975}", CryHooksModule);
        AZ_CLASS_ALLOCATOR(VoiceChatModule, AZ::SystemAllocator, 0);

        VoiceChatModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                VoiceChatCVars::CreateDescriptor(),
                VoiceTeam::CreateDescriptor(),
#if defined(AZ_PLATFORM_WINDOWS) || defined (DEDICATED_SERVER)
                    VoiceChatNetwork::CreateDescriptor(),
                    OpusVoiceEncoder::CreateDescriptor(),
#endif
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
                    VoiceChatCommands::CreateDescriptor(),
#endif
#if defined(AZ_PLATFORM_WINDOWS)
                    VoicePlayer::CreateDescriptor(),
                    VoiceRecorder::CreateDescriptor(),
                    VoicePlayerDriverWin::CreateDescriptor()
#endif
                });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                       azrtti_typeid<VoiceChatCVars>(),
                       azrtti_typeid<VoiceTeam>(),
#if defined(AZ_PLATFORM_WINDOWS) || defined (DEDICATED_SERVER)
                           azrtti_typeid<VoiceChatNetwork>(),
                           azrtti_typeid<OpusVoiceEncoder>(),
#endif
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
                           azrtti_typeid<VoiceChatCommands>(),
#endif
#if defined(AZ_PLATFORM_WINDOWS)
                           azrtti_typeid<VoicePlayer>(),
                           azrtti_typeid<VoiceRecorder>(),
                           azrtti_typeid<VoicePlayerDriverWin>()
#endif
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(VoiceChat_f03d0d253f794cf0adc89cf7b7927aa1, VoiceChat::VoiceChatModule)
