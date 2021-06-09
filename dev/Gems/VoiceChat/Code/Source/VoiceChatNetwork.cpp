#include "VoiceChat_precompiled.h"
#include "VoiceChatNetwork.h"
#include "VoiceChatCVars.h"
#include "VoiceFilters.h"
#include "VoiceCompressionBus.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <VoiceChat/VoiceTeamBus.h>

#include <GridMate/Session/Session.h>
#include <GridMate/Online/UserServiceTypes.h>

namespace VoiceChat
{
    static const GridMate::MemberIDCompact kHostId = 0;

    void VoiceChatNetwork::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VoiceChatNetwork"));
    }

    void VoiceChatNetwork::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("VoiceChatNetwork"));
    }

    void VoiceChatNetwork::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<VoiceChatNetwork, AZ::Component>()->Version(0);
        }
    }

    void VoiceChatNetwork::Activate()
    {
        CrySystemEventBus::Handler::BusConnect();
        VoiceChatNetworkRequestBus::Handler::BusConnect();
    }

    void VoiceChatNetwork::Deactivate()
    {
        if (m_threadPump)
        {
            m_threadPump = false;

            if (m_voiceThread.joinable())
            {
                m_voiceEvent.release();
                m_voiceThread.join();
            }
        }

        m_mixTick.Stop();

        CrySystemEventBus::Handler::BusDisconnect();
        VoiceChatNetworkRequestBus::Handler::BusDisconnect();
    }

    void VoiceChatNetwork::OnSessionJoined(GridMate::GridSession* session)
    {
        // voice chat disabled ignore this session
        if (!g_pVoiceChatCVars->voice_chat_enabled)
        {
            return;
        }

        m_state = State::Client;
        m_session = session;

        m_threadPump = true;

        m_voiceThread = AZStd::thread([this]()
        {
            while (m_threadPump)
            {
                m_voiceEvent.acquire();
                PumpVoiceChatClient();
            }
        });

        GridMate::GridMateEventsBus::Handler::BusConnect(session->GetGridMate());
    }

    void VoiceChatNetwork::OnSessionHosted(GridMate::GridSession* session)
    {
        // voice chat disabled ignore this session
        if (!g_pVoiceChatCVars->voice_chat_enabled)
        {
            return;
        }

        m_state = State::Host;
        m_session = session;

        m_threadPump = true;

        m_voiceThread = AZStd::thread([this]()
        {
            while (m_threadPump)
            {
                m_voiceEvent.acquire();
                PumpVoiceChatHost();
            }
        });

        m_mixTick.Start([this]()
        {
            MixHostPackets();
        });

        GridMate::VoiceChatServiceBus::Handler::BusConnect(session->GetGridMate());
        GridMate::GridMateEventsBus::Handler::BusConnect(session->GetGridMate());
    }

    void VoiceChatNetwork::OnSessionDelete(GridMate::GridSession* session)
    {
        m_state = State::None;

        if (m_threadPump)
        {
            m_threadPump = false;

            if (m_voiceThread.joinable())
            {
                m_voiceEvent.release();
                m_voiceThread.join();
            }
        }

        m_mixTick.Stop();

        m_session = nullptr;

        GridMate::VoiceChatServiceBus::Handler::BusDisconnect(session->GetGridMate());
        GridMate::GridMateEventsBus::Handler::BusDisconnect(session->GetGridMate());
    }

    void VoiceChatNetwork::PumpVoiceChatHost()
    {
        VoiceClients connectedVoices;
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
            connectedVoices = m_connectedVoices;
        }

        for (auto cidIn : connectedVoices)
        {
            AZ::u8 buf[voice_traits::VOICE_AUDIO_PACKET_SIZE];
            size_t bufSize = sizeof(buf);

            while (PopFromNetwork(buf, bufSize, cidIn))
            {
                AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
                m_incomingPackets[cidIn].Push(buf, bufSize);

                if (g_pVoiceChatCVars->voice_chat_network_debug)
                {
                    AZ_TracePrintf("VoiceChat", "Received voice packet %i from %u queue %i", (int)bufSize, cidIn, (int)m_incomingPackets[cidIn].Size());
                }
            }
        }
    }

    void VoiceChatNetwork::MixHostPackets()
    {
        // flush after all speaking connection get the voices
        VoiceRingsMap toMixPackets;
        VoiceClients connectedVoices;

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);

            connectedVoices = m_connectedVoices;

            for (auto& kv : m_incomingPackets)
            {
                VoiceBuffer pkt;

                if (kv.second.Pop(pkt))
                {
                    toMixPackets[kv.first].Push(pkt.data(), pkt.size());
                }
            }
        }

        if (!toMixPackets.empty())
        {
            FlushToClients(toMixPackets, connectedVoices);
        }
    }

    size_t VoiceChatNetwork::FlushToClients(VoiceRingsMap& incomingPackets, const VoiceClients& to)
    {
        size_t totalFlushedSize = 0;
        VoiceRingsMap fromHostPackets;
        VoiceMuteMap muteTable;

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
            muteTable = m_muteTable;
        }

        for (auto& kv : incomingPackets)
        {
            if (kv.second.Size() < voice_traits::VOICE_AUDIO_PACKET_SIZE)
            {
                continue;
            }

            auto cidIn = kv.first;

            VoiceTeamId teamInId = kInvalidVoiceTeamId;
            EBUS_EVENT_RESULT(teamInId, VoiceTeamBus, GetJoinedTeam, cidIn);

            AZ::u8 samples[voice_traits::VOICE_AUDIO_PACKET_SIZE];
            kv.second.Pop(samples, sizeof(samples));

            for (auto cidOut : to)
            {
                // ignore myself
                if (cidIn == cidOut)
                {
                    continue;
                }

                // ignore muted members
                if (muteTable[cidOut].find(cidIn) != muteTable[cidOut].end())
                {
                    continue;
                }

                VoiceTeamId teamOutId = kInvalidVoiceTeamId;
                EBUS_EVENT_RESULT(teamOutId, VoiceTeamBus, GetJoinedTeam, cidOut);

                // ignore members in other voice team
                if (teamInId != teamOutId)
                {
                    continue;
                }

                if (0 == fromHostPackets[cidOut].Size())
                {
                    fromHostPackets[cidOut].Push(samples, sizeof(samples));

                    if (g_pVoiceChatCVars->voice_chat_network_debug)
                    {
                        AZ_TracePrintf("VoiceChat", "Seed mix buffer for %u", cidOut);
                    }
                }
                else
                {
                    AZ::u8 mixedBuf[voice_traits::VOICE_AUDIO_PACKET_SIZE];
                    size_t mixedSize = fromHostPackets[cidOut].Pop(mixedBuf, sizeof(mixedBuf));

                    voice_filters::MixPackets(mixedBuf, samples, mixedSize, mixedBuf);

                    fromHostPackets[cidOut].Push(mixedBuf, mixedSize);

                    if (g_pVoiceChatCVars->voice_chat_network_debug)
                    {
                        AZ_TracePrintf("VoiceChat", "Mixing buffer to %u from %u", cidOut, cidIn);
                    }
                }
            }
        }

        // send mixed packets
        for (auto& kv : fromHostPackets)
        {
            AZ::u8 samples[voice_traits::VOICE_AUDIO_PACKET_SIZE];
            size_t samplesSize = kv.second.Pop(samples, sizeof(samples));

            totalFlushedSize += samplesSize;

            if (g_pVoiceChatCVars->voice_chat_network_debug)
            {
                AZ_TracePrintf("VoiceChat", "Sending mixed voice packet %i to %u total %i", (int)samplesSize, kv.first, (int)totalFlushedSize);
            }
            if (g_pVoiceChatCVars->voice_dump_disk)
            {
                voice_filters::SampleToFile(samples, samplesSize, kv.first);
            }

            PushToNetwork(samples, samplesSize, kv.first);
        }

        return totalFlushedSize;
    }

    void VoiceChatNetwork::OnCrySystemInitialized(ISystem&, const SSystemInitParams&)
    {
        GridMate::IGridMate* gridMate = gEnv->pNetwork->GetGridMate();
        AZ_Assert(gridMate, "GridMate must be initialized");

        GridMate::SessionEventBus::Handler::BusConnect(gridMate);
    }

    void VoiceChatNetwork::RegisterMember(GridMate::GridMember* member)
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
        m_connectedVoices.emplace(member->GetIdCompact());
    }

    void VoiceChatNetwork::UnregisterMember(GridMate::GridMember* member)
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
        m_connectedVoices.erase(member->GetIdCompact());
    }

    void VoiceChatNetwork::SendVoice(const AZ::u8* data, size_t dataSize)
    {
        FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

        PushToNetwork(data, dataSize, kHostId);
    }

    void VoiceChatNetwork::PushToNetwork(const AZ::u8* data, size_t size, const GridMate::MemberIDCompact toId)
    {
        AZ::s16 samples[voice_traits::VOICE_AUDIO_PACKET_SAMPLES];
        voice_filters::ArrayToSamples(data, samples, size);
        size_t numSamples = size / voice_traits::BYTES_PER_SAMPLE;

        AZ::u8 compressedBuf[voice_traits::VOICE_AUDIO_PACKET_SIZE];

        size_t compressedSize = 0;
        EBUS_EVENT_RESULT(compressedSize, VoiceCompressionBus, Compress, toId, samples, numSamples, compressedBuf);

        if (compressedSize > voice_traits::VOICE_NETWORK_PACKET_SIZE)
        {
            AZ_Warning("VoiceChat", false, "Compressed packet size %i is too big for network", (int)compressedSize);
            return;
        }

        AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
        AddToQueue(m_toNetworkPackets[toId], compressedBuf, compressedSize);
    }

    bool VoiceChatNetwork::PopFromNetwork(AZ::u8* data, size_t& size, const GridMate::MemberIDCompact fromId)
    {
        AZ::u8 networkBuf[voice_traits::VOICE_NETWORK_PACKET_SIZE];

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);

            if (m_fromNetworkPackets[fromId].empty())
            {
                return false;
            }

            const auto& pkt = m_fromNetworkPackets[fromId].front();
            memcpy(networkBuf, pkt.data(), pkt.size());

            size = pkt.size();
            m_fromNetworkPackets[fromId].pop();
        }

        AZ::s16 actualSamples[voice_traits::VOICE_AUDIO_PACKET_SAMPLES];

        size_t numSamples = 0;
        EBUS_EVENT_RESULT(numSamples, VoiceCompressionBus, Decompress, fromId, networkBuf, size, actualSamples);

        voice_filters::SamplesToArray(actualSamples, data, numSamples);
        size = numSamples * voice_traits::BYTES_PER_SAMPLE;

        return true;
    }

    void VoiceChatNetwork::PumpVoiceChatClient()
    {
        AZ::u8 voiceData[voice_traits::VOICE_AUDIO_PACKET_SIZE];
        size_t dataSize = sizeof(voiceData);

        while (PopFromNetwork(voiceData, dataSize, kHostId))
        {
            EBUS_EVENT(VoiceChatNetworkEventBus, OnVoice, voiceData, dataSize);
        }
    }

    void VoiceChatNetwork::UpdateHostNetwork()
    {
        FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

        if (!m_session)
        {
            return;
        }

        // update mute table on main thread
        VoiceClients connectedVoices;
        VoiceMuteMap muteTable;

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
            connectedVoices = m_connectedVoices;
        }

        for (const auto& cid : connectedVoices)
        {
            auto member = m_session->GetMemberById(cid);
            if (!member)
            {
                continue;
            }

            for (const auto& coud : connectedVoices)
            {
                if (member->IsMuted(coud))
                {
                    muteTable[cid].emplace(coud);
                }
            }
        }

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
            AZStd::swap(m_muteTable, muteTable);
        }

        // receive from all connected voices
        for (const auto& cid : connectedVoices)
        {
            auto member = m_session->GetMemberById(cid);
            if (!member)
            {
                continue;
            }

            auto carrier = m_session->GetCarrier();

            while (true)
            {
                auto connId = member->GetConnectionId();

                if (GridMate::InvalidConnectionID == connId)
                {
                    break;
                }

                AZ::u8 networkBuf[voice_traits::VOICE_NETWORK_PACKET_SIZE];

                auto res = carrier->Receive((char*)networkBuf, sizeof(networkBuf), connId,
                    GridMate::GridSession::CarrierChannels::CC_VOICE_DATA);

                if (res.m_state != GridMate::Carrier::ReceiveResult::RECEIVED)
                {
                    break;
                }

                {
                    AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
                    AddToQueue(m_fromNetworkPackets[cid], networkBuf, res.m_numBytes);
                }
            }
        }

        // send to all pushed
        VoiceQueuesMap toNetworkPackets;
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
            AZStd::swap(m_toNetworkPackets, toNetworkPackets);
        }

        for (auto& kv : toNetworkPackets)
        {
            auto member = m_session->GetMemberById(kv.first);
            if (!member)
            {
                continue;
            }

            auto connId = member->GetConnectionId();

            if (GridMate::InvalidConnectionID == connId)
            {
                continue;
            }

            auto carrier = m_session->GetCarrier();

            while (!kv.second.empty())
            {
                const auto& pkt = kv.second.front();

                carrier->Send((const char*)pkt.data(), pkt.size(), connId,
                    GridMate::Carrier::SEND_UNRELIABLE,
                    GridMate::Carrier::PRIORITY_NORMAL,
                    GridMate::GridSession::CarrierChannels::CC_VOICE_DATA);

                kv.second.pop();
            }
        }
    }

    void VoiceChatNetwork::UpdateClientNetwork()
    {
        FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

        if (!m_session || !m_session->GetHost())
        {
            return;
        }

        auto host = m_session->GetHost();
        if (!host->IsReady())
        {
            return;
        }

        auto carrier = m_session->GetCarrier();

        // receive from host all packets
        while (true)
        {
            auto hostId = host->GetConnectionId();

            if (GridMate::InvalidConnectionID == hostId)
            {
                break;
            }

            AZ::u8 networkBuf[voice_traits::VOICE_NETWORK_PACKET_SIZE];

            auto res = carrier->Receive((char*)networkBuf, sizeof(networkBuf), hostId,
                GridMate::GridSession::CarrierChannels::CC_VOICE_DATA);

            if (res.m_state != GridMate::Carrier::ReceiveResult::RECEIVED)
            {
                break;
            }
            {
                AZStd::lock_guard<AZStd::mutex> lock(m_voiceMutex);
                AddToQueue(m_fromNetworkPackets[kHostId], networkBuf, res.m_numBytes);
            }
        }

        // send to host all packets
        while (!m_toNetworkPackets[kHostId].empty())
        {
            const auto& pkt = m_toNetworkPackets[kHostId].front();

            auto hostId = host->GetConnectionId();

            if (GridMate::InvalidConnectionID != hostId)
            {
                carrier->Send((const char*)pkt.data(), pkt.size(), hostId,
                    GridMate::Carrier::SEND_UNRELIABLE,
                    GridMate::Carrier::PRIORITY_NORMAL,
                    GridMate::GridSession::CarrierChannels::CC_VOICE_DATA);
            }

            m_toNetworkPackets[kHostId].pop();
        }
    }

    void VoiceChatNetwork::OnGridMateUpdate(GridMate::IGridMate* gridMate)
    {
        if (m_state == State::Host)
        {
            UpdateHostNetwork();
            m_voiceEvent.release();
        }
        else if (m_state == State::Client)
        {
            UpdateClientNetwork();
            m_voiceEvent.release();
        }
    }

    void VoiceChatNetwork::AddToQueue(VoiceQueue& queue, const AZ::u8* buffer, size_t size)
    {
        int queueLimit = g_pVoiceChatCVars->voice_network_queue_limit;

        if (queue.size() > queueLimit)
        {
            AZ_Warning("VoiceChat", false, "Voice queue limit %i. Drop outdated data", queueLimit);
            VoiceQueue().swap(queue);
        }

        queue.emplace(VoiceBuffer(buffer, buffer + size));
    }
}
