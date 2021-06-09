#pragma once

#include <AzCore/EBus/EBus.h>

    namespace VoiceChat
    {
        class VoicePlayerDriverRequests
            : public AZ::EBusTraits
        {
        public:
            virtual ~VoicePlayerDriverRequests() = default;

            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

            virtual void PlayAudio(const AZ::u8* data, size_t dataSize) = 0;
        };
        using VoicePlayerDriverRequestBus = AZ::EBus<VoicePlayerDriverRequests>;
    }
