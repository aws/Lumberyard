#pragma once

#include <AzCore/EBus/EBus.h>

namespace VoiceChat
{
    class VoiceChatNetworkRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~VoiceChatNetworkRequests() = default;

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual void SendVoice(const AZ::u8* data, size_t dataSize) = 0;
    };
    using VoiceChatNetworkRequestBus = AZ::EBus<VoiceChatNetworkRequests>;

    class VoiceChatNetworkEvents
        : public AZ::EBusTraits
    {
    public:
        virtual ~VoiceChatNetworkEvents() = default;

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual void OnVoice(const AZ::u8* data, size_t dataSize) = 0;
    };
    using VoiceChatNetworkEventBus = AZ::EBus<VoiceChatNetworkEvents>;
}
