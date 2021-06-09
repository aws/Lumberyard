#include "VoiceChat_precompiled.h"
#include "OpusVoiceEncoder.h"
#include "VoicePacket.h"
#include "VoiceFilters.h"
#include "VoiceChatCVars.h"

#include <AzCore/Serialization/SerializeContext.h>

#include <opus.h>

using namespace Audio;

namespace VoiceChat
{
    void OpusVoiceEncoder::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("OpusVoiceEncoder"));
    }

    void OpusVoiceEncoder::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("OpusVoiceEncoder"));
    }

    void OpusVoiceEncoder::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<OpusVoiceEncoder, AZ::Component>()->Version(0);
        }
    }

    void OpusVoiceEncoder::Activate()
    {
        VoiceCompressionBus::Handler::BusConnect();
        AzFramework::NetBindingSystemEventsBus::Handler::BusConnect();
    }

    void OpusVoiceEncoder::Deactivate()
    {
        AzFramework::NetBindingSystemEventsBus::Handler::BusDisconnect();
        VoiceCompressionBus::Handler::BusDisconnect();
    }

    void OpusVoiceEncoder::OnNetworkSessionDeactivated(GridMate::GridSession* session)
    {
        for (auto& kv : m_encoders)
        {
            opus_encoder_destroy(kv.second);
        }

        m_encoders.clear();
        AZ_TracePrintf("VoiceChat", "Opus encoders destroyed");

        for (auto& kv : m_decoders)
        {
            opus_decoder_destroy(kv.second);
        }

        m_decoders.clear();
        AZ_TracePrintf("VoiceChat", "Opus decoders destroyed");
    }

    size_t OpusVoiceEncoder::Compress(const int streamId, const AZ::s16* samples, const size_t numSamples, AZ::u8* data)
    {
        if (OpusEncoder* encoder = GetEncoder(streamId))
        {
            size_t numBytes = opus_encode(encoder, samples, numSamples, data, voice_traits::VOICE_AUDIO_PACKET_SIZE);

            if (numBytes > 0)
            {
                return numBytes;
            }
            else
            {
                AZ_Warning("VoiceChat", false, "Compress failed with %i, packet size: %i", (int)numBytes, (int)numSamples);
            }
        }
        else
        {
            AZ_Warning("VoiceChat", false, "Encoder isn't available, keep raw data");
        }

        size_t numBytes = numSamples * voice_traits::BYTES_PER_SAMPLE;
        voice_filters::SamplesToArray(samples, data, numSamples);

        return numBytes;
    }

    size_t OpusVoiceEncoder::Decompress(const int streamId, const AZ::u8* data, const size_t dataSize, AZ::s16* samples)
    {
        if (OpusDecoder* decoder = GetDecoder(streamId))
        {
            size_t numSamples = opus_decode(decoder, data, dataSize, samples, voice_traits::VOICE_AUDIO_PACKET_SAMPLES, 0);

            if (numSamples > 0)
            {
                return numSamples;
            }
            else
            {
                AZ_Warning("VoiceChat", false, "Decompress failed with %i, packet size %i", (int)numSamples, (int)dataSize);
            }
        }
        else
        {
            AZ_Warning("VoiceChat", false, "Decoder isn't available, keep raw data");
        }

        size_t numSamples = dataSize / voice_traits::BYTES_PER_SAMPLE;
        voice_filters::ArrayToSamples(data, samples, dataSize);
        return numSamples;
    }

    int OpusVoiceEncoder::GetFrameSizeEnum() const
    {
        int frameSize = (int)std::floor((voice_traits::VOICE_AUDIO_PACKET_SIZE / (float)voice_traits::BYTES_PER_SAMPLE) / voice_traits::SAMPLE_RATE * 1000.0f);

        int frameSizeEnum = OPUS_FRAMESIZE_ARG;

        if (frameSize == 5)
        {
            frameSizeEnum = OPUS_FRAMESIZE_5_MS;
        }
        else if (frameSize == 10)
        {
            frameSizeEnum = OPUS_FRAMESIZE_10_MS;
        }
        else if (frameSize == 20)
        {
            frameSizeEnum = OPUS_FRAMESIZE_20_MS;
        }
        else if (frameSize == 40)
        {
            frameSizeEnum = OPUS_FRAMESIZE_40_MS;
        }
        else if (frameSize == 60)
        {
            frameSizeEnum = OPUS_FRAMESIZE_60_MS;
        }
        else if (frameSize == 80)
        {
            frameSizeEnum = OPUS_FRAMESIZE_80_MS;
        }
        else if (frameSize == 100)
        {
            frameSizeEnum = OPUS_FRAMESIZE_100_MS;
        }
        else if (frameSize == 120)
        {
            frameSizeEnum = OPUS_FRAMESIZE_120_MS;
        }
        else
        {
            AZ_Warning("VoiceChat", false, "Can't get frame size for %i", frameSize);
        }

        return frameSizeEnum;
    }

    OpusEncoder* OpusVoiceEncoder::GetEncoder(const int streamId)
    {
        if (m_encoders.find(streamId) == m_encoders.end())
        {
            int err = OPUS_OK;
            m_encoders[streamId] = opus_encoder_create(voice_traits::SAMPLE_RATE, voice_traits::CHANNELS, OPUS_APPLICATION_VOIP, &err);
            if (err != OPUS_OK || m_encoders[streamId] == nullptr)
            {
                AZ_Warning("VoiceChat", false, "Cannot create opus encoder, error: %i", err);
                m_encoders.erase(streamId);
                return nullptr;
            }

            AZ_TracePrintf("VoiceChat", "Opus encoder created for stream %i", streamId);
            opus_encoder_ctl(m_encoders[streamId], OPUS_SET_FORCE_CHANNELS(voice_traits::CHANNELS));
            opus_encoder_ctl(m_encoders[streamId], OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
            opus_encoder_ctl(m_encoders[streamId], OPUS_SET_EXPERT_FRAME_DURATION(GetFrameSizeEnum()));
        }

        return m_encoders[streamId];
    }

    OpusDecoder* OpusVoiceEncoder::GetDecoder(const int streamId)
    {
        if (m_decoders.find(streamId) == m_decoders.end())
        {
            int err = OPUS_OK;

            m_decoders[streamId] = opus_decoder_create(voice_traits::SAMPLE_RATE, voice_traits::CHANNELS, &err);
            if (err != OPUS_OK || m_decoders[streamId] == nullptr)
            {
                AZ_Warning("VoiceChat", false, "Cannot create opus decoder, error: %i", err);
                m_decoders.erase(streamId);
                return nullptr;
            }

            AZ_TracePrintf("VoiceChat", "Opus decoder created for stream %i", streamId);
            opus_decoder_ctl(m_decoders[streamId], OPUS_SET_FORCE_CHANNELS(voice_traits::CHANNELS));
            opus_decoder_ctl(m_decoders[streamId], OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
            opus_decoder_ctl(m_decoders[streamId], OPUS_SET_EXPERT_FRAME_DURATION(GetFrameSizeEnum()));
        }

        return m_decoders[streamId];
    }
}
