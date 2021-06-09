#pragma once

#include <cmath>
#include <climits>

#include <GridMate/Session/Session.h>
#include <INetwork.h>

namespace VoiceChat
{
    namespace voice_filters
    {
        namespace conv
        {
            static void S2BLE(const AZ::s16 sample, AZ::u8* arr) // assume little endian
            {
                *arr++ = (AZ::u8)(sample & 0xFF);
                *arr = (AZ::u8)(sample >> 8);
            }

            static AZ::s16 B2SLE(const AZ::u8* arr)
            {
                return ((AZ::s16)*(arr + 1) << 8) | *arr;
            }
        }

        static void ApplyVolume(AZ::u8* buf, size_t size, float volume)
        {
            float mult = std::tan(volume);

            for (size_t pos = 0; pos < size; pos += sizeof(AZ::s16))
            {
                AZ::s16 sample = conv::B2SLE(&buf[pos]);

                float newSample = std::floor(mult * sample);

                newSample = AZStd::min<float>(SHRT_MAX, newSample);
                newSample = AZStd::max<float>(SHRT_MIN, newSample);

                AZ::s16 outSample = (AZ::s16)newSample;

                conv::S2BLE(outSample, &buf[pos]);
            }
        }

        static void LowPass(AZ::u8* buf, size_t size)
        {
            AZ::s16 outSample = 0;

            for (size_t pos = 0; pos < size; pos += sizeof(AZ::s16))
            {
                AZ::s16 sample = conv::B2SLE(&buf[pos]);
                float newSample = std::floor((outSample * 90 + sample * 10) / 100.0f);

                newSample = AZStd::min<float>(SHRT_MAX, newSample);
                newSample = AZStd::max<float>(SHRT_MIN, newSample);

                outSample = (AZ::s16)newSample;
                conv::S2BLE(outSample, &buf[pos]);
            }
        }

        static float VoiceActiveLevel(const AZ::u8* buf, size_t size)
        {
            float samplesSum = 0;
            size_t numSamples = 0;

            for (size_t pos = 0; pos < size; pos += sizeof(AZ::s16))
            {
                AZ::s16 sample = conv::B2SLE(&buf[pos]);
                samplesSum += abs(sample);
                ++numSamples;
            }
            return std::floor(samplesSum / numSamples);
        }

        static void MixPackets(const AZ::u8* firstPacket, const AZ::u8* secondPacket, size_t size, AZ::u8* mixedPacket)
        {
            // http://www.vttoth.com/CMS/index.php/technical-notes/68
            for (size_t pos = 0; pos < size; pos += sizeof(AZ::s16))
            {
                AZ::s32 mixedSample = 0;
                AZ::s32 firstSample = conv::B2SLE(&firstPacket[pos]);
                AZ::s32 secondSample = conv::B2SLE(&secondPacket[pos]);

                firstSample += (SHRT_MAX + 1);
                secondSample += (SHRT_MAX + 1);

                if (firstSample < (SHRT_MAX + 1) || secondSample < (SHRT_MAX + 1))
                {
                    mixedSample = (firstSample * secondSample) / (SHRT_MAX + 1);
                }
                else
                {
                    mixedSample = 2 * (firstSample + secondSample) - (firstSample * secondSample) / (SHRT_MAX + 1) - (USHRT_MAX + 1);
                }

                // clipping
                if (mixedSample >= (USHRT_MAX + 1))
                {
                    mixedSample = USHRT_MAX;
                }
                AZ::s16 newSample = mixedSample - (SHRT_MAX + 1);

                conv::S2BLE(newSample, &mixedPacket[pos]);
            }
        }

        static void ArrayToSamples(const AZ::u8* buf, AZ::s16* samples, size_t bufLen)
        {
            for (size_t pos = 0; pos < bufLen; pos += sizeof(AZ::s16))
            {
                *samples++ = conv::B2SLE(&buf[pos]);
            }
        }

        static void SamplesToArray(const AZ::s16* samples, AZ::u8* buf, size_t numSamples)
        {
            for (size_t pos = 0; pos < numSamples; ++pos)
            {
                conv::S2BLE(samples[pos], buf);
                buf += sizeof(AZ::s16);
            }
        }

        static void SampleToFile(const AZ::u8* data, size_t size, const GridMate::MemberIDCompact toId)
        {
            auto sampleName = GridMate::string::format("%u.pcm", toId);
            auto name = PathUtil::Make("sounds/voip_samples", sampleName.c_str());

            CCryFile file;
            if (file.Open(name.c_str(), "ab+"))
            {
                file.Write(data, size);
                file.Flush();
            }
            else
            {
                AZ_Warning("VoiceChat", false, "Failed to write sample %s", sampleName.c_str());
            }
        }
    }
}
