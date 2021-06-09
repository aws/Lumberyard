#pragma once

#include <AzCore/EBus/EBus.h>

namespace VoiceChat
{
    class VoiceChatRequests
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual ~VoiceChatRequests() = default;

        virtual void StartRecording() = 0;
        virtual void StopRecording() = 0;

        virtual void TestRecording(const char* sampleName) = 0;

        virtual void ToggleRecording() = 0;
    };
    using VoiceChatRequestBus = AZ::EBus<VoiceChatRequests>;
} // namespace VoiceChat
