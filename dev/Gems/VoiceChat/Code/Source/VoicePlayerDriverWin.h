#pragma once

#include <AzCore/Component/Component.h>

#include "VoicePlayerDriverBus.h"

#include <Windows.h>
#include <mmsystem.h>

namespace VoiceChat
{
    class VoicePlayerDriverWin
        : public AZ::Component
        , private VoicePlayerDriverRequestBus::Handler
    {
    public:
        AZ_COMPONENT(VoicePlayerDriverWin, "{A54054C8-8855-4A67-9824-5F21FCEDC203}");

        static void Reflect(AZ::ReflectContext* pContext);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void AddFreeBlock();
        void RemoveFreeBlock();

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        // VoicePlayerDriverRequestBus interface implementation
        void PlayAudio(const AZ::u8* data, size_t dataSize) override;

    private:
        void CreateAudioDevice();
        void DestroyAudioDevice();

        WAVEHDR* AllocateBlocks(size_t size, int count);
        void FreeBlocks(WAVEHDR* blockArray);
        void WriteBlock(HWAVEOUT hWaveOut, LPSTR data, size_t size);

    private:
        HWAVEOUT m_hWaveOut{ 0 };
        CRITICAL_SECTION m_waveCriticalSection;
        WAVEHDR* m_waveBlocks{ nullptr };
        volatile int m_waveFreeBlockCount{ 0 };
        int m_waveCurrentBlock{ 0 };
        size_t m_blockSize{ 0 };
        int m_blockCount{ 0 };
        bool m_isInitialized{ false };
    };
}
