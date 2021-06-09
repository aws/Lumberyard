#pragma once

#include <AzCore/EBus/EBus.h>
#include <GridMate/Session/Session.h>

namespace VoiceChat
{
    using VoiceTeamId = int;
    const constexpr VoiceTeamId kInvalidVoiceTeamId = -1;

    class VoiceTeamRequests
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual ~VoiceTeamRequests() = default;

        virtual void Join(VoiceTeamId teamId, GridMate::MemberIDCompact memberId) = 0;
        virtual void Leave(GridMate::MemberIDCompact memberId) = 0;

        virtual VoiceTeamId GetJoinedTeam(GridMate::MemberIDCompact memberId) const = 0;

        virtual void Mute(GridMate::MemberIDCompact memberId) = 0;
        virtual void Unmute(GridMate::MemberIDCompact memberId) = 0;
    };
    using VoiceTeamBus = AZ::EBus<VoiceTeamRequests>;
} // namespace VoiceChat
