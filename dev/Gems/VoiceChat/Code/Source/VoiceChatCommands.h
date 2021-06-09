#pragma once

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
    #include <AzCore/Component/Component.h>
    #include <CrySystemBus.h>

    #include <ISystem.h>
    #include <IConsole.h>

namespace VoiceChat
{
    class VoiceChatCommands
        : public AZ::Component
        , public CrySystemEventBus::Handler
    {
    public:
        AZ_COMPONENT(VoiceChatCommands, "{C3677AFC-B056-4E79-86B8-A73B7FD8EA1B}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    private:
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;

        // CrySystemEventBus interface implementation
        void OnCrySystemInitialized(ISystem&, const SSystemInitParams&) override;

    private:
        static void CmdStartRecording(IConsoleCmdArgs* pArgs);
        static void CmdStopRecording(IConsoleCmdArgs* pArgs);
        static void CmdTestRecording(IConsoleCmdArgs* pArgs);

        static void CmdMutePlayer(IConsoleCmdArgs* pArgs);
        static void CmdUnmutePlayer(IConsoleCmdArgs* pArgs);
    };
}
#endif
