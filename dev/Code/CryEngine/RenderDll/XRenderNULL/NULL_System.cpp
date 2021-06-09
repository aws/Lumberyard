/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// Description : NULL device specific implementation and extensions handling.


#include "StdAfx.h"
#include "NULL_Renderer.h"


bool CNULLRenderer::SetGammaDelta(const float fGamma)
{
    m_fDeltaGamma = fGamma;
    return true;
}

int CNULLRenderer::EnumDisplayFormats(SDispFormat* Formats)
{
    return 0;
}

bool CNULLRenderer::ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen, bool bForce)
{
    return false;
}

WIN_HWND CNULLRenderer::Init(int x, int y, int width, int height, unsigned int cbpp, int zbpp, int sbits, bool fullscreen, bool isEditor, WIN_HINSTANCE hinst, WIN_HWND Glhwnd, bool bReInit, const SCustomRenderInitArgs* pCustomArgs, bool bShaderCacheGen)
{
    //=======================================
    // Add init code here
    //=======================================

    FX_SetWireframeMode(R_SOLID_MODE);

    SetWidth(width);
    SetHeight(height);
    m_backbufferWidth = width;
    m_backbufferHeight = height;
    m_nativeWidth = width;
    m_nativeHeight = height;
    m_Features |= RFT_HW_NVIDIA;

    if (!g_shaderGeneralHeap)
    {
        g_shaderGeneralHeap = CryGetIMemoryManager()->CreateGeneralExpandingMemoryHeap(4 * 1024 * 1024, 0, "Shader General");
    }

    iLog->Log("Init Shaders\n");

    gRenDev->m_cEF.mfInit();
    EF_Init();

#if NULL_SYSTEM_TRAIT_INIT_RETURNTHIS
    return (WIN_HWND)this;//it just get checked against NULL anyway
#else
    return (WIN_HWND)GetDesktopWindow();
#endif
}


bool CNULLRenderer::SetCurrentContext(WIN_HWND hWnd)
{
    return true;
}

bool CNULLRenderer::CreateContext(WIN_HWND hWnd, bool bAllowMSAA, int SSX, int SSY)
{
    return true;
}

bool CNULLRenderer::DeleteContext(WIN_HWND hWnd)
{
    return true;
}

void CNULLRenderer::MakeMainContextActive()
{
}

void CNULLRenderer::ShutDown(bool bReInit)
{
    FreeResources(FRR_ALL);
    FX_PipelineShutdown();
}

void CNULLRenderer::ShutDownFast()
{
    FX_PipelineShutdown();
}

