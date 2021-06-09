#pragma once

#include <AzCore/Component/Component.h>
#include <AzFramework/Network/NetBindingSystemBus.h>

#include <VoiceChat/VoiceTeamBus.h>

namespace VoiceChat
{
    class VoiceTeam
        : public AZ::Component
        , private VoiceTeamBus::Handler
        , private AzFramework::NetBindingSystemEventsBus::Handler
    {
    private:
        using VoiceTeams = AZStd::unordered_map<GridMate::MemberIDCompact, VoiceTeamId>;
    public:
        AZ_COMPONENT(VoiceTeam, "{79ED1279-F2F0-4805-B238-C6BDE0047BFD}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    private:
        // AZ::Component interface implementation
        virtual void Activate() override;
        virtual void Deactivate() override;

        // NetBindingSystemBus
        void OnNetworkSessionCreated(GridMate::GridSession* session) override;
        void OnNetworkSessionDeactivated(GridMate::GridSession* session) override;

        ////////////////////////////////////////////////////////////////////////
        // VoiceTeamBus interface implementation
        virtual void Join(VoiceTeamId teamId, GridMate::MemberIDCompact memberId) override;
        virtual void Leave(GridMate::MemberIDCompact memberId) override;

        virtual VoiceTeamId GetJoinedTeam(GridMate::MemberIDCompact memberId) const override;

        virtual void Mute(GridMate::MemberIDCompact memberId) override;
        virtual void Unmute(GridMate::MemberIDCompact memberId) override;

    private:
        VoiceTeams m_teams;

        GridMate::GridSession* m_session{ nullptr };

        mutable AZStd::mutex m_mutex;
    };
}
