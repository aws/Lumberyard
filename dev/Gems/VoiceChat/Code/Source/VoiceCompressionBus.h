#pragma once

#include <AzCore/EBus/EBus.h>

namespace VoiceChat
{
    class VoiceCompressionRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~VoiceCompressionRequests() = default;

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        using MutexType = AZStd::recursive_mutex;

        virtual size_t Compress(const int streamId, const AZ::s16* samples, const size_t numSamples, AZ::u8* data) = 0;
        virtual size_t Decompress(const int streamId, const AZ::u8* data, const size_t dataSize, AZ::s16* samples) = 0;
    };
    using VoiceCompressionBus = AZ::EBus<VoiceCompressionRequests>;
}
