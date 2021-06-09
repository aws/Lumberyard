#pragma once

#include <AzCore/Component/Component.h>
#include <AzFramework/Network/NetBindingSystemBus.h>

#include "VoiceCompressionBus.h"

struct OpusEncoder;
struct OpusDecoder;

namespace VoiceChat
{
    class OpusVoiceEncoder
        : public AZ::Component
        , private AzFramework::NetBindingSystemEventsBus::Handler
        , private VoiceCompressionBus::Handler
    {
    public:
        AZ_COMPONENT(OpusVoiceEncoder, "{007EB3E5-B156-4DBB-B615-735197594E7C}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    private:
        // AZ::Component interface implementation
        virtual void Activate() override;
        virtual void Deactivate() override;

        // NetBindingSystemBus
        void OnNetworkSessionDeactivated(GridMate::GridSession* session) override;

        // VoiceCompressionBus
        virtual size_t Compress(const int streamId, const AZ::s16* samples, const size_t numSamples, AZ::u8* data) override;
        virtual size_t Decompress(const int streamId, const AZ::u8* data, const size_t dataSize, AZ::s16* samples) override;

        //////////////////////////////////////////////////////////////////////////
        int GetFrameSizeEnum() const;

        OpusEncoder* GetEncoder(const int streamId);
        OpusDecoder* GetDecoder(const int streamId);

    private:
        AZStd::unordered_map<int, OpusEncoder*> m_encoders;
        AZStd::unordered_map<int, OpusDecoder*> m_decoders;
    };
}
