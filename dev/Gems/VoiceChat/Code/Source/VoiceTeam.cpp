#include "VoiceChat_precompiled.h"
#include "VoiceTeam.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <GridMate/Session/Session.h>

namespace VoiceChat
{
    void VoiceTeam::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VoiceTeam"));
    }

    void VoiceTeam::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("VoiceTeam"));
    }

    void VoiceTeam::Activate()
    {
        VoiceTeamBus::Handler::BusConnect();
        AzFramework::NetBindingSystemEventsBus::Handler::BusConnect();
    }

    void VoiceTeam::Deactivate()
    {
        VoiceTeamBus::Handler::BusDisconnect();
        AzFramework::NetBindingSystemEventsBus::Handler::BusDisconnect();
    }

    void VoiceTeam::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<VoiceTeam, AZ::Component>()->Version(0);
        }
    }

    void VoiceTeam::OnNetworkSessionCreated(GridMate::GridSession* session)
    {
        m_session = session;
    }

    void VoiceTeam::OnNetworkSessionDeactivated(GridMate::GridSession* session)
    {
        m_session = nullptr;

        m_teams.clear();
    }

    void VoiceTeam::Join(VoiceTeamId teamId, GridMate::MemberIDCompact memberId)
    {
        GridMate::GridMember* member = m_session ? m_session->GetMemberById(memberId) : nullptr;

        if (member)
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            m_teams[member->GetIdCompact()] = teamId;
        }
    }

    void VoiceTeam::Leave(GridMate::MemberIDCompact memberId)
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        m_teams.erase(memberId);
    }

    VoiceTeamId VoiceTeam::GetJoinedTeam(GridMate::MemberIDCompact memberId) const
    {
        VoiceTeamId groupId = kInvalidVoiceTeamId;
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            const auto& it = m_teams.find(memberId);
            groupId = it != m_teams.end() ? it->second : kInvalidVoiceTeamId;
        }

        return groupId;
    }

    void VoiceTeam::Mute(GridMate::MemberIDCompact memberId)
    {
        GridMate::GridMember* localMember = m_session ? m_session->GetMyMember() : nullptr;
        GridMate::GridMember* member = m_session ? m_session->GetMemberById(memberId) : nullptr;

        if (localMember && member)
        {
            localMember->Mute(member->GetIdCompact());
        }
    }

    void VoiceTeam::Unmute(GridMate::MemberIDCompact memberId)
    {
        GridMate::GridMember* localMember = m_session ? m_session->GetMyMember() : nullptr;
        GridMate::GridMember* member = m_session ? m_session->GetMemberById(memberId) : nullptr;

        if (localMember && member)
        {
            localMember->Unmute(member->GetIdCompact());
        }
    }
}
