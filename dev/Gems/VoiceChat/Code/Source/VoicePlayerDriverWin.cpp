#include "VoiceChat_precompiled.h"
#include "VoicePlayerDriverWin.h"
#include "VoicePacket.h"
#include "VoiceChatCVars.h"
#include "VoiceChatCVars.h"

#include <AzCore/Serialization/SerializeContext.h>

namespace
{
    using namespace VoiceChat;

    static void CALLBACK waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
    {
        if (WOM_DONE == uMsg)
        {
            VoicePlayerDriverWin* audioDriver = (VoicePlayerDriverWin*)dwInstance;
            audioDriver->AddFreeBlock();
        }
    }
}

namespace VoiceChat
{
    void VoicePlayerDriverWin::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VoicePlayerDriverWin"));
    }

    void VoicePlayerDriverWin::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("VoicePlayerDriverWin"));
    }

    void VoicePlayerDriverWin::Reflect(AZ::ReflectContext* pContext)
    {
        if (AZ::SerializeContext* pSerializeContext = azrtti_cast<AZ::SerializeContext*>(pContext))
        {
            pSerializeContext->Class<VoicePlayerDriverWin, AZ::Component>()->Version(0);
        }
    }

    void VoicePlayerDriverWin::Activate()
    {
        VoicePlayerDriverRequestBus::Handler::BusConnect();
    }

    void VoicePlayerDriverWin::Deactivate()
    {
        VoicePlayerDriverRequestBus::Handler::BusDisconnect();
        DestroyAudioDevice();
    }

    void VoicePlayerDriverWin::CreateAudioDevice()
    {
        WAVEFORMATEX wfx;
        wfx.nSamplesPerSec = voice_traits::SAMPLE_RATE;
        wfx.wBitsPerSample = voice_traits::BITS_PER_SAMPLE;
        wfx.nChannels = voice_traits::CHANNELS;
        wfx.cbSize = 0;
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nBlockAlign = (wfx.wBitsPerSample * wfx.nChannels) >> 3;
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

        MMRESULT res = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
        if (MMSYSERR_NOERROR != res)
        {
            AZ_WarningOnce("VoiceChat", false, "Failed to initialize VOIP audio driver %i", res);
            return;
        }

        m_blockSize = static_cast<size_t>((voice_traits::BYTES_PER_SAMPLE * voice_traits::SAMPLE_RATE / 1000.f) * g_pVoiceChatCVars->voice_player_driver_block_size_msec);
        m_blockCount = g_pVoiceChatCVars->voice_player_driver_blocks_count; // one second should be enough

        m_waveBlocks = AllocateBlocks(m_blockSize, m_blockCount);
        m_waveFreeBlockCount = m_blockCount;
        m_waveCurrentBlock = 0;

        // Only critical section is allowed to call from audio callback
        InitializeCriticalSection(&m_waveCriticalSection);

        m_isInitialized = true;
    }

    void VoicePlayerDriverWin::DestroyAudioDevice()
    {
        if (m_isInitialized)
        {
            for (int i = 0; i < m_waveFreeBlockCount; ++i)
            {
                if (m_waveBlocks[i].dwFlags & WHDR_PREPARED)
                {
                    waveOutUnprepareHeader(m_hWaveOut, &m_waveBlocks[i], sizeof(WAVEHDR));
                }
            }
            DeleteCriticalSection(&m_waveCriticalSection);
            FreeBlocks(m_waveBlocks);
            waveOutClose(m_hWaveOut);
        }
    }

    void VoicePlayerDriverWin::PlayAudio(const AZ::u8* data, size_t dataSize)
    {
        if (!m_isInitialized)
        {
            CreateAudioDevice();
        }

        if (m_isInitialized)
        {
            WriteBlock(m_hWaveOut, (LPSTR)data, dataSize);
        }
    }

    WAVEHDR* VoicePlayerDriverWin::AllocateBlocks(size_t size, int count)
    {
        size_t totalBufferSize = (size + sizeof(WAVEHDR)) * count;
        AZ::u8* buffer = static_cast<AZ::u8*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalBufferSize));

        WAVEHDR* blocks = (WAVEHDR*)buffer;
        buffer += sizeof(WAVEHDR) * count;

        for (int i = 0; i < count; ++i)
        {
            blocks[i].dwBufferLength = size;
            blocks[i].lpData = (LPSTR)buffer;
            buffer += size;
        }
        return blocks;
    }

    void VoicePlayerDriverWin::FreeBlocks(WAVEHDR* blockArray)
    {
        HeapFree(GetProcessHeap(), 0, blockArray);
    }

    void VoicePlayerDriverWin::AddFreeBlock()
    {
        EnterCriticalSection(&m_waveCriticalSection);
        ++m_waveFreeBlockCount;

        if (g_pVoiceChatCVars->voice_chat_network_debug)
        {
            AZ_TracePrintf("VoiceChat", "Free audio blocks increased to %i", m_waveFreeBlockCount);
        }
        LeaveCriticalSection(&m_waveCriticalSection);

    }

    void VoicePlayerDriverWin::RemoveFreeBlock()
    {
        EnterCriticalSection(&m_waveCriticalSection);
        --m_waveFreeBlockCount;

        if (g_pVoiceChatCVars->voice_chat_network_debug)
        {
            AZ_TracePrintf("VoiceChat", "Free audio blocks decreased to %i", m_waveFreeBlockCount);
        }
        LeaveCriticalSection(&m_waveCriticalSection);

    }

    void VoicePlayerDriverWin::WriteBlock(HWAVEOUT hWaveOut, LPSTR data, size_t size)
    {
        size_t remain = 0;
        WAVEHDR* current = &m_waveBlocks[m_waveCurrentBlock];

        while (size > 0 && m_waveFreeBlockCount)
        {
            if (current->dwFlags & WHDR_PREPARED)
            {
                waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
            }

            if (size < (m_blockSize - current->dwUser))
            {
                memcpy(current->lpData + current->dwUser, data, size);
                current->dwUser += size;
                break; // don't play until full block is filled
            }

            remain = m_blockSize - current->dwUser;
            memcpy(current->lpData + current->dwUser, data, remain);
            size -= remain;
            data += remain;

            current->dwBufferLength = m_blockSize;

            waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));

            RemoveFreeBlock();

            m_waveCurrentBlock++;
            m_waveCurrentBlock %= m_blockCount;
            current = &m_waveBlocks[m_waveCurrentBlock];
            current->dwUser = 0;
        }
    }
}
