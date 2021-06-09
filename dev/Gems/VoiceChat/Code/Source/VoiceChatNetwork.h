#pragma once

#include <AzCore/Component/Component.h>

#include <GridMate/VoiceChat/VoiceChatServiceBus.h>
#include <GridMate/Session/SessionServiceBus.h>
#include <GridMate/GridMateEventsBus.h>

#include <AzCore/std/parallel/binary_semaphore.h>

#include <CrySystemBus.h>
#include <ISystem.h>

#include "VoicePacket.h"
#include "VoiceRingBuffer.h"
#include "JitterBuffer.h"
#include "VoiceChatNetworkBus.h"

namespace VoiceChat
{
    class VoiceChatNetwork
        : public AZ::Component
        , protected VoiceChatNetworkRequestBus::Handler
        , private GridMate::VoiceChatServiceBus::Handler
        , private GridMate::SessionEventBus::Handler
        , private CrySystemEventBus::Handler
        , private GridMate::GridMateEventsBus::Handler
    {
    private:
        enum class State { None, Host, Client };

        using VoiceClients = AZStd::set<GridMate::MemberIDCompact>;
        using VoiceRingsMap = AZStd::unordered_map<GridMate::MemberIDCompact, VoiceRingBuffer>;
        using VoiceQueuesMap = AZStd::unordered_map<GridMate::MemberIDCompact, VoiceQueue>;
        using VoiceMuteMap = AZStd::unordered_map<GridMate::MemberIDCompact, VoiceClients>;
        using JitterBuffersMap = AZStd::unordered_map<GridMate::MemberIDCompact, JitterBuffer>;

    public:
        AZ_COMPONENT(VoiceChatNetwork, "{4DFC4C39-498C-4725-A948-2D2C6546CA0B}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        // VoiceChatNetworkRequestBus interface implementation
        void SendVoice(const AZ::u8* data, size_t dataSize) override;

    private:
        // GridMate::VoiceChatServiceBus
        void RegisterMember(GridMate::GridMember* member) override;
        void UnregisterMember(GridMate::GridMember* member) override;

        // GridMate::SessionEventBus
        void OnSessionJoined(GridMate::GridSession* session) override;
        void OnSessionHosted(GridMate::GridSession* session) override;
        void OnSessionDelete(GridMate::GridSession* session) override;

        // CrySystemEventBus
        void OnCrySystemInitialized(ISystem&, const SSystemInitParams&) override;

        // GridMateEventBus
        void OnGridMateUpdate(GridMate::IGridMate* gridMate) override;

        //////////////////////////////////////////////////////////////////////////
        void PumpVoiceChatHost();
        void PumpVoiceChatClient();

        size_t FlushToClients(VoiceRingsMap& incomingPackets, const VoiceClients& to);

        void PushToNetwork(const AZ::u8* data, size_t size, const GridMate::MemberIDCompact toId);
        bool PopFromNetwork(AZ::u8* data, size_t& size, const GridMate::MemberIDCompact fromId);

        void UpdateClientNetwork();
        void UpdateHostNetwork();

        void MixHostPackets();

        void AddToQueue(VoiceQueue& queue, const AZ::u8* buffer, size_t size);

    private:
        State m_state{ State::None };
        GridMate::GridSession* m_session{ nullptr };
        VoiceClients m_connectedVoices;

        AZStd::thread m_voiceThread;
        AZStd::mutex m_voiceMutex;
        AZStd::binary_semaphore m_voiceEvent;
        AZStd::atomic_bool m_threadPump{ false };

        VoiceQueuesMap m_toNetworkPackets;
        VoiceQueuesMap m_fromNetworkPackets;

        JitterThread m_mixTick;

        VoiceMuteMap m_muteTable;
        JitterBuffersMap m_incomingPackets;
    };
}
