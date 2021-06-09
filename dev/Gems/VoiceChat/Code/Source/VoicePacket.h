#pragma once

#include <GridMate/Session/Session.h>
#include "VoiceChatCVars.h"

namespace VoiceChat
{
    namespace voice_traits
    {
        static const AZ::u32 SAMPLE_RATE = 16000; // preferred data rate for voice
        static const AZ::u16 BITS_PER_SAMPLE = 16;
        static const AZ::u32 BYTES_PER_SAMPLE = (AZ::u32)(BITS_PER_SAMPLE / 8);
        static const AZ::u16 CHANNELS = 1;
        static const AZ::u16 VOICE_NETWORK_PACKET_SIZE = 960; // to go thru Internet without defragmentation
        static const AZ::u16 VOICE_ENCODER_PACKET_MSEC = 100; // fixed chunk used by Opus voice encoder
        static const AZ::u16 VOICE_AUDIO_PACKET_SIZE = ((SAMPLE_RATE * BYTES_PER_SAMPLE) / 1000) * VOICE_ENCODER_PACKET_MSEC; // voice buffer transferred via network (configured for OPUS compression size)
        static const AZ::u16 VOICE_AUDIO_PACKET_SAMPLES = VOICE_AUDIO_PACKET_SIZE / BYTES_PER_SAMPLE;

        static const AZ::u32 RECORDER_BUFFER_ALLOC_SIZE = SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE;
        static const AZ::u32 RECORDER_FRAME_SIZE = RECORDER_BUFFER_ALLOC_SIZE / 4;

        static const AZ::u32 VOICE_RING_BUFFER_SIZE = SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE;
    }

    using VoiceBuffer = AZStd::vector<AZ::u8, AZ::OSStdAllocator>;
    using VoiceQueue = AZStd::queue<VoiceBuffer>;
}
