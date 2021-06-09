#include "VoiceChat_precompiled.h"

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
    #include "VoiceChatCommands.h"

    #include <AzCore/Serialization/SerializeContext.h>

    #include <VoiceChat/VoiceChatBus.h>
    #include <VoiceChat/VoiceTeamBus.h>

    #include "VoiceChatNetworkBus.h"
    #include "VoicePlayerDriverBus.h"

namespace VoiceChat
{
    void VoiceChatCommands::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VoiceChatCommands"));
    }

    void VoiceChatCommands::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("VoiceChatCommands"));
    }

    void VoiceChatCommands::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<VoiceChatCommands, AZ::Component>()->Version(0);
        }
    }

    void VoiceChatCommands::Activate()
    {
        CrySystemEventBus::Handler::BusConnect();
    }

    void VoiceChatCommands::Deactivate()
    {
        CrySystemEventBus::Handler::BusDisconnect();
    }

    void VoiceChatCommands::OnCrySystemInitialized(ISystem&, const SSystemInitParams&)
    {
        REGISTER_COMMAND("voice_start_recording", VoiceChatCommands::CmdStartRecording, VF_DEV_ONLY, "Start voice recording");
        REGISTER_COMMAND("voice_stop_recording", VoiceChatCommands::CmdStopRecording, VF_DEV_ONLY, "Stop voice recording");
        REGISTER_COMMAND("voice_test_recording", VoiceChatCommands::CmdTestRecording, VF_DEV_ONLY, "Test voice recording");

        REGISTER_COMMAND("voice_mute", VoiceChatCommands::CmdMutePlayer, VF_DEV_ONLY, "Mute player by network compact id");
        REGISTER_COMMAND("voice_unmute", VoiceChatCommands::CmdUnmutePlayer, VF_DEV_ONLY, "Unmute player by network compact id");
    }

    void VoiceChatCommands::CmdStartRecording(IConsoleCmdArgs* pArgs)
    {
        EBUS_EVENT(VoiceChatRequestBus, StartRecording);
    }

    void VoiceChatCommands::CmdStopRecording(IConsoleCmdArgs* pArgs)
    {
        EBUS_EVENT(VoiceChatRequestBus, StopRecording);
    }

    void VoiceChatCommands::CmdTestRecording(IConsoleCmdArgs* pArgs)
    {
        if (pArgs->GetArgCount() != 2)
        {
            AZ_Warning("VoiceChat", false, "Need sample name");
            return;
        }

        const char* name = pArgs->GetArg(1);

        AZ_TracePrintf("VoiceChat", "Preparing sample %s", name);
        EBUS_EVENT(VoiceChatRequestBus, TestRecording, name);
    }

    void VoiceChatCommands::CmdMutePlayer(IConsoleCmdArgs* pArgs)
    {
        if (pArgs->GetArgCount() != 2)
        {
            AZ_Warning("VoiceChat", false, "Need network compact id");
            return;
        }

        GridMate::MemberIDCompact memberID;
        azsscanf(pArgs->GetArg(1), "%u", &memberID);

        EBUS_EVENT(VoiceTeamBus, Mute, memberID);
    }

    void VoiceChatCommands::CmdUnmutePlayer(IConsoleCmdArgs* pArgs)
    {
        if (pArgs->GetArgCount() != 2)
        {
            AZ_Warning("VoiceChat", false, "Need network compact id");
            return;
        }

        GridMate::MemberIDCompact memberID;
        azsscanf(pArgs->GetArg(1), "%u", &memberID);

        EBUS_EVENT(VoiceTeamBus, Unmute, memberID);
    }
}
#endif
