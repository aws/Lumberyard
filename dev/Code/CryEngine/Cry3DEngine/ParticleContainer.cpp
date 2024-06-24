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

#include "StdAfx.h"
#include "ParticleContainer.h"
#include "ParticleContainerGPU.h"
#include "ParticleEmitter.h"
#include "Particle.h"
#include <AzFramework/Physics/SystemBus.h>

#define fMAX_STATIC_BB_RADIUS                   4096.f  // Static bounding box above this size forces dynamic BB
#define fMAX_RELATIVE_TAIL_DEVIATION    0.1f

#define nMAX_ITERATIONS                             8
#define fMAX_FRAME_LIMIT_TIME                   0.25f       // Max frame time at which to enforce iteration limit
#define fMAX_LINEAR_STEP_TIME                   4.f         // Max time to test ahead

template<class T>
inline void move_raw_array_elem(T* pDest, T* pSrc)
{
    T val = *pSrc;
    if (pSrc < pDest)
    {
        memmove(pSrc, pSrc + 1, (char*)pDest - (char*)(pSrc + 1));
    }
    else
    {
        memmove(pDest + 1, pDest, (char*)pSrc - (char*)pDest);
    }
    *pDest = val;
}

//////////////////////////////////////////////////////////////////////////
CParticleContainer::CParticleContainer(CParticleContainer* pParent, CParticleEmitter* pMain, CParticleEffect const* pEffect)
    : m_pEffect(&non_const(*pEffect))
    , m_nEmitterSequence(0)
    , m_pMainEmitter(pMain)
    , m_fAgeLastUpdate(0.f)
    , m_fAgeStaticBoundsStable(0.f)
    , m_fContainerLife(0.f)
    , m_pGPUData(0)
    , m_LodBlend(0.f)
    , m_LodOverlap(0.f)
{
    assert(pEffect);

    // Assert effect is active if it's not in editing or not in preview mode
    assert(pEffect->IsActive() || gEnv->IsEditing() || m_pMainEmitter->GetPreviewMode());

    m_pParams = &m_pEffect->GetParams();
    assert(m_pParams->nEnvFlags & EFF_LOADED);

    if ((m_pParams->eSpawnIndirection && (m_pParams->eEmitterType != ParticleParams::EEmitterType::GPU)) || (m_pParams->eGPUSpawnIndirection && (m_pParams->eEmitterType == ParticleParams::EEmitterType::GPU)))
    {
        m_pParentContainer = pParent;
    }
    else if (pParent && pParent->IsIndirect())
    {
        m_pParentContainer = pParent->GetParent();
    }
    else
    {
        m_pParentContainer = 0;
    }

    // To do: Invalidate static bounds on updating areas.
    // To do: Use dynamic bounds if in non-uniform areas ??
    m_bbWorld.Reset();
    m_bbWorldStat.Reset();
    m_bbWorldDyn.Reset();

    OnEffectChange();

    m_nChildFlags = 0;
    m_bUsed = true;
    m_fMaxParticleFullLife = m_pEffect->CalcMaxParticleFullLife(m_pMainEmitter->GetPreviewMode());

    m_nNeedJobUpdate = 0;

    if (!m_pParentContainer)
    {
        // Not an indirect container, so it will need only a single SubEmitter - allocate it here
        AddEmitter(pMain);
    }

    // Do not use coverage buffer culling for 'draw near' or 'on top' particles
    if (pMain && (GetParams().bDrawNear || GetParams().bDrawOnTop))
    {
        pMain->SetRndFlags(ERF_RENDER_ALWAYS, true);
    }
}

CParticleContainer::CParticleContainer(CParticleContainer* pParent, CParticleEmitter* pMain, CParticleEffect const* pEffect, CLodInfo* startLod, CLodInfo* endLod)
    : CParticleContainer(pParent, pMain, pEffect)
{
    m_StartLodInfoPtr = startLod;
    m_EndLodInfoPtr = endLod;

    //Calculate start blend by comparing to the cam distance.
    auto MainCamPos = gEnv->p3DEngine->GetRenderingCamera().GetPosition();
    const AABB& bb = NeedsDynamicBounds() ? m_bbWorldDyn : m_bbWorldStat;
    float CenterCamDist = (MainCamPos - bb.GetCenter()).GetLength() - m_pParams->fSizeY.GetMaxValue();

    bool isIn = true;

    if (m_StartLodInfoPtr && CenterCamDist < m_StartLodInfoPtr->GetDistance())
    {
        isIn = false;
    }
    if (m_EndLodInfoPtr && CenterCamDist > m_EndLodInfoPtr->GetDistance())
    {
        isIn = false;
    }

    if (isIn)
    {
        m_LodBlend = 1.0f;
    }
}


CParticleContainer::~CParticleContainer()
{
    delete m_pGPUData;
    m_pGPUData = nullptr;
}

uint32 CParticleContainer::GetEnvironmentFlags() const
{
    return m_nEnvFlags & CParticleManager::Instance()->GetAllowedEnvironmentFlags();
}

void CParticleContainer::OnEffectChange()
{
    m_nEnvFlags = m_pParams->nEnvFlags;

    // Update existing particle history arrays if needed.
    int nPrevSteps = m_nHistorySteps;
    m_nHistorySteps = m_pParams->GetTailSteps();

    // Do not use coverage buffer culling for 'draw near' or 'on top' particles
    if (m_pParams->bDrawNear || m_pParams->bDrawOnTop)
    {
        GetMain().SetRndFlags(ERF_RENDER_ALWAYS, true);
    }

#if !defined(CONSOLE)
    if (gEnv->IsEditor() && m_Particles.size() > 0)
    {
        if (m_nHistorySteps > nPrevSteps || NeedsCollisionInfo())
        {
            for (ParticleList<CParticle>::iterator pPart(m_Particles); pPart; ++pPart)
            {
                // Alloc and connect emitter structures.
                pPart->UpdateAllocations(nPrevSteps);
            }
        }
    }
#endif

    if (GetGPUData())
    {
        GetGPUData()->OnEffectChange();
    }

    if (m_pParentContainer)
    {
        // Inherit dynamic bounds requirement.
        m_nEnvFlags |= m_pParentContainer->GetEnvironmentFlags() & EFF_DYNAMIC_BOUNDS;
        for (CParticleContainer* pParent = m_pParentContainer; pParent; pParent = pParent->GetParent())
        {
            pParent->m_nChildFlags |= m_nEnvFlags;
        }
        if (GetParams().eAttachType == GeomType_Render)
        {
            if (m_pParentContainer->GetParams().pStatObj)
            {
                m_pParentContainer->GetParams().pStatObj->GetExtent(GetParams().eAttachForm);
            }
        }
    }

    // Prevent invalid particle references to old geometry when editing Geometry param.
    if (gEnv->IsEditing() && GetParams().pStatObj)
    {
        m_ExternalStatObjs.push_back(GetParams().pStatObj);
    }

#if !PARTICLES_USE_CRY_PHYSICS
    if (Physics::System* system = AZ::Interface<Physics::System>::Get())
    {
        const Physics::MaterialId thisMaterialId = Physics::MaterialId::FromUUID(GetParams().sPhysicsMaterial.material);
        if (!thisMaterialId.IsNull())
        {
            if (Physics::MaterialLibraryAsset* defMaterialLib = system->GetDefaultMaterialLibraryAssetPtr()->Get())
            {
                Physics::MaterialFromAssetConfiguration materialFromAsset;
                if (defMaterialLib->GetDataForMaterialId(thisMaterialId, materialFromAsset))
                {
                    m_materialConfiguration = materialFromAsset.m_configuration;
                }
            }
        }
    }
#endif // PARTICLES_USE_CRY_PHYSICS
}

CParticleSubEmitter* CParticleContainer::AddEmitter(CParticleSource* pSource)
{
    if (void* pMem = m_Emitters.push_back_new())
    {
        return new(pMem)CParticleSubEmitter(pSource, this);
    }
    return NULL;
}

CParticle* CParticleContainer::AddParticle(SParticleUpdateContext& context, const CParticle& part)
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    const ResourceParticleParams& params = GetParams();

    if (params.IsConnectedParticles())
    {
        // Segregate by emitter, and always in front of previous particles.
        // To do: faster search for multi-emitter containers.
        int nSequence = part.GetEmitter()->GetSequence();
        for (ParticleList<CParticle>::const_iterator pPart(m_Particles); pPart; ++pPart)
        {
            if (pPart->GetEmitterSequence() == nSequence)
            {
                auto newParticle = m_Particles.insert(pPart, part);

                //Confetti - Chris Hekman
                //If the distance between particles is high enough we hide the particle
                if (params.bMinVisibleSegmentLength)
                {
                    float dist = (pPart->GetLocation().t - part.GetLocation().t).len();
                    if (dist < params.fMinVisibleSegmentLengthFloat)
                    {
                        newParticle->Hide();
                    }
                }

                return newParticle;
            }
        }
    }
    else if (context.nEnvFlags & REN_SORT)
    {
        float fNewDist = part.GetMinDist(context.vMainCamPos);

        Array<SParticleUpdateContext::SSortElem>* particles = &context.aParticleSort;

        if (!particles->empty())
        {
            // More accurate sort, using sort data in SParticleUpdateContext.
            size_t nNewPos = min((size_t)particles->size() - 1, m_Particles.size());
            SParticleUpdateContext::SSortElem* pNew = &(*particles)[nNewPos];
            pNew->pPart = NULL;
            pNew->fDist = -fNewDist;

            SParticleUpdateContext::SSortElem* pFound;
            if (context.nSortQuality == 1)
            {
                // Find nearest (progressive distance) particle. Order: log(2) n
                pFound = std::lower_bound(particles->begin(), pNew, *pNew);
            }
            else
            {
                // Find min distance error. Order: n
                float fMinError = 0.f;
                float fCumError = 0.f;
                pFound = particles->begin();
                for (size_t i = 0; i < nNewPos; i++)
                {
                    fCumError += (*particles)[i].fDist + fNewDist;
                    if (fCumError < fMinError)
                    {
                        fMinError = fCumError;
                        pFound = &(*particles)[i + 1];
                    }
                }
            }

#ifdef SORT_DEBUG
            // Compute sort error. Slow!
            {
                static float fAvgError = 0.f;
                static float fCount = 0.f;
                float fError = 0.f;
                float fDir = 1.f;
                for (ParticleList<CParticle>::const_iterator pPart(m_Particles); pPart; ++pPart)
                {
                    if (pPart == pFound->pPart)
                    {
                        fDir = -1.f;
                    }
                    float fDist = pPart->GetMinDist(context.vMainCamPos);
                    fError += max(fDir * (fNewDist - fDist), 0.f);
                }

                fAvgError = (fAvgError * fCount + fError) / (fCount + 1.f);
                fCount += 1.f;
            }
#endif

            // Move next particle in particle list, and corresponding data in local arrays.
            CParticle* pPart = m_Particles.insert(pFound->pPart, part);
            pNew->pPart = pPart;
            move_raw_array_elem(pFound, pNew);
            return pPart;
        }
        else
        {
            // Approximate sort; push to front or back of list, based on position relative to bb center.
            if (fNewDist < context.fCenterCamDist)
            {
                return m_Particles.push_back(part);
            }
        }
    }

    // Push newest to front.
    return m_Particles.push_front(part);
}

void CParticleContainer::EmitParticle(const EmitParticleData* pData)
{
    // Queue particle emission.
    EmitParticleData* pNew = m_DeferredEmitParticles.push_back();
    if (pNew && pData)
    {
        *pNew = *pData;

        if (pData->pPhysEnt || pData->bHasLocation || pData->bHasVel)
        {
            // Static bounds no longer reliable.
            SetDynamicBounds();
        }
        if (pData->pStatObj)
        {
            // Add reference to StatObj for lifetime of container.
            assert(GetEnvironmentFlags() & REN_GEOMETRY);
            m_ExternalStatObjs.push_back(pData->pStatObj);
        }
    }
}

void CParticleContainer::ComputeStaticBounds(AABB& bb, bool bWithSize, float fMaxLife)
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    const ResourceParticleParams& params = GetParams();

    QuatTS loc = GetMain().GetLocation();
    AABB bbSpawn(0.f);

    FStaticBounds opts;

    if (m_pParentContainer)
    {
        // Expand by parent spawn volume.
        bool bParentSize = params.eAttachType != GeomType_None;
        float fMaxParentLife = params.GetMaxSpawnDelay();
        if (params.bContinuous || params.eMoveRelEmitter)
        {
            if (params.fEmitterLifeTime)
            {
                fMaxParentLife += params.fEmitterLifeTime.GetMaxValue();
            }
            else
            {
                fMaxParentLife = fHUGE;
            }
        }
        m_pParentContainer->ComputeStaticBounds(bbSpawn, bParentSize, fMaxParentLife);
        loc.t = bbSpawn.GetCenter();
        opts.fAngMax = m_pParentContainer->GetParams().GetMaxRotationAngle();
    }
    else if (GetMain().GetEmitGeom())
    {
        // Expand by attached geom.
        GetMain().GetEmitGeom().GetAABB(bbSpawn, loc);
        loc.t = bbSpawn.GetCenter();
    }

    loc.s = GetMain().GetParticleScale();
    opts.vSpawnSize = bbSpawn.GetSize() * 0.5f;
    opts.fSpeedScale = GetMain().GetSpawnParams().fSpeedScale;
    opts.bWithSize = bWithSize;
    opts.fMaxLife = fMaxLife;

    SPhysForces forces;
    GetMain().GetPhysEnviron().GetForces(forces, loc.t, m_nEnvFlags);
    params.GetStaticBounds(bb, loc, forces, opts);

    // Adjust for internal/external target.
    ParticleTarget target;
    if (GetTarget(target, GetDirectEmitter()))
    {
        AABB bbTarg(target.vTarget, target.fRadius);
        bbTarg.Expand(bb.GetSize() * 0.5f);
        bb.Add(bbTarg);
    }
}

void CParticleContainer::UpdateState()
{
    FUNCTION_PROFILER_SYS(PARTICLE);

    UpdateContainerLife();

    if (!NeedsDynamicBounds())
    {
        if (m_bbWorldStat.IsReset())
        {
            ComputeStaticBounds(m_bbWorldStat);
            if (m_bbWorldStat.GetRadiusSqr() >= sqr(fMAX_STATIC_BB_RADIUS) && m_pParams->eEmitterType != (m_pParams->eEmitterType).GPU)
            {
                SetDynamicBounds();
            }
        }
    }

    if (NeedsDynamicBounds() || !m_DeferredEmitParticles.empty())
    {
        UpdateParticles();
        m_DeferredEmitParticles.clear();
    }

    if (NeedsDynamicBounds())
    {
        SyncUpdateParticles();
        m_bbWorld = m_bbWorldDyn;
    }
    else
    {
        m_bbWorld = m_bbWorldStat;
        if (!StaticBoundsStable())
        {
            m_bbWorld.Add(m_bbWorldDyn);
        }
    }
}

// To do: Add flags for movement/gravity/wind.
float CParticleContainer::InvalidateStaticBounds()
{
    m_bbWorldDyn.Reset();
    if (!NeedsDynamicBounds())
    {
        m_bbWorldStat.Reset();
        if (NeedsExtendedBounds() && (this->IsGPUParticle() || !m_Particles.empty()))
        {
            return m_fAgeStaticBoundsStable = GetAge() + m_pParams->GetMaxParticleLife();
        }
    }
    return 0.f;
}

bool CParticleContainer::GetTarget(ParticleTarget& target, const CParticleSubEmitter* pSubEmitter) const
{
    ParticleParams::STargetAttraction::ETargeting eTargeting = GetParams().TargetAttraction.eTarget;
    if (eTargeting == eTargeting.Ignore)
    {
        return false;
    }

    target = GetMain().GetTarget();
    if (!target.bPriority && eTargeting == eTargeting.OwnEmitter && pSubEmitter)
    {
        // Local target override.
        target.bTarget = true;
        target.vTarget = pSubEmitter->GetSource().GetLocation().t;
        target.vVelocity = pSubEmitter->GetSource().GetVelocity().vLin;
        target.fRadius = 0.f;
    }
    return target.bTarget;
}

float CParticleContainer::ComputeLodBlend(float distanceToCamera, float updateTime)
{
    //Preview mode should not care about lods
    if (m_pMainEmitter->GetPreviewMode())
    {
        m_LodBlend = 1.f;
        return m_LodBlend;
    }

    bool isIn = true;
    float blendIn = 1.f;
    float blendOut = 1.f;

    //Calculate blend values
    if (m_pMainEmitter->GetEffect()->GetParticleParams().fBlendIn > 0.f)
    {
        blendIn = (1 / m_pMainEmitter->GetEffect()->GetParticleParams().fBlendIn) * updateTime;
    }

    if (m_pMainEmitter->GetEffect()->GetParticleParams().fBlendOut > 0.f)
    {
        blendOut = (1 / m_pMainEmitter->GetEffect()->GetParticleParams().fBlendOut) * updateTime;
    }

    //We check if the particle is within the lod distances
    if (m_StartLodInfoPtr)
    {
        if (distanceToCamera < m_StartLodInfoPtr->GetDistance())
        {
            isIn = false;
        }
    }
    if (m_EndLodInfoPtr)
    {
        if (distanceToCamera > m_EndLodInfoPtr->GetDistance())
        {
            isIn = false;
        }
    }

    //When it is "in" we want to blend to 1
    if (isIn)
    {
        m_LodOverlap = 0;

        m_LodBlend += blendIn;
        if (m_LodBlend > 1)
        {
            m_LodBlend = 1.0f;
        }
    }
    //When it is "out" we want to blend to 0
    else
    {
        if (m_LodOverlap >= m_pMainEmitter->GetEffect()->GetParticleParams().fOverlap)
        {
            m_LodBlend -= blendOut;
            if (m_LodBlend < 0.f)
            {
                m_LodBlend = 0.f;
            }
        }
        else
        {
            m_LodOverlap += updateTime;
        }
    }

    return m_LodBlend;
}

void CParticleContainer::GetContainerBounds(Vec3& totalBounds)
{
    for (ParticleList<CParticleSubEmitter>::iterator emitter(m_Emitters); emitter != m_Emitters.end(); ++emitter)
    {
        Vec3 currentBounds(fHUGE); //subemitter checks for the min, so ensure default can't be the min
        emitter->GetEmitterBounds(currentBounds);
        totalBounds.CheckMax(currentBounds);
    }
}

void CParticleContainer::ComputeUpdateContext(SParticleUpdateContext& context, float fUpdateTime)
{
    const ResourceParticleParams& params = GetParams();

    context.startLodInfo = this->m_StartLodInfoPtr;
    context.endLodInfo = this->m_EndLodInfoPtr;
    context.fUpdateTime = fUpdateTime;
    context.fMaxLinearStepTime = fMAX_LINEAR_STEP_TIME;

    context.nEnvFlags = GetEnvironmentFlags();

    // Sorting support.
    context.nSortQuality = max((int)params.nSortQuality, GetCVars()->e_ParticlesSortQuality);
    context.vMainCamPos = gEnv->p3DEngine->GetRenderingCamera().GetPosition();
    const AABB& bb = NeedsDynamicBounds() ? m_bbWorldDyn : m_bbWorldStat;    
    context.fCenterCamDist = (context.vMainCamPos - bb.GetCenter()).GetLength() - params.fSizeY.GetMaxValue();

    //use direct emitter's position for lod blend
    float emitterCamDist = context.fCenterCamDist;
    if (GetDirectEmitter())
    {
        emitterCamDist = (context.vMainCamPos - GetDirectEmitter()->GetEmitPos()).GetLength() - params.fSizeY.GetMaxValue();
    }
    ComputeLodBlend(emitterCamDist, fUpdateTime);
    context.lodBlend = m_LodBlend;

    // Compute rotations in 3D for geometry particles, or those set to 3D orientation.
    context.b3DRotation = params.eFacing == params.eFacing.Free || params.eFacing == params.eFacing.Decal
        || params.pStatObj;

    // Emission bounds parameters.
    context.vEmitBox = params.vSpawnPosRandomOffset;

    context.vEmitScale.zero();
    if (params.fOffsetRoundness)
    {
        float fRound = params.fOffsetRoundness * max(max(context.vEmitBox.x, context.vEmitBox.y), context.vEmitBox.z);
        Vec3 vRound(min(context.vEmitBox.x, fRound), min(context.vEmitBox.y, fRound), min(context.vEmitBox.z, fRound));
        context.vEmitBox -= vRound;
        context.vEmitBox.CheckMin(params.vSpawnPosRandomOffset);
        context.vEmitScale(vRound.x ? 1 / vRound.x : 0, vRound.y ? 1 / vRound.y : 0, vRound.z ? 1 / vRound.z : 0);
    }

    if (params.bSpaceLoop)
    {
        // Use emission bounding volume.
        AABB bbSpaceLocal = params.GetEmitOffsetBounds();
        if (GetMain().GetEnvFlags() & REN_BIND_CAMERA)
        {
            // Use CameraMaxDistance as alternate space loop size.
            bbSpaceLocal.Add(Vec3(0.f), params.fCameraMaxDistance);

            // Limit bounds to camera frustum.
            bbSpaceLocal.min.y = max(bbSpaceLocal.min.y, +params.fCameraMinDistance);

            // Scale cam dist limits to handle zoom.
            CCamera const& cam = gEnv->p3DEngine->GetRenderingCamera();
            float fHSinCos[2];
            sincos_tpl(cam.GetHorizontalFov() * 0.5f, &fHSinCos[0], &fHSinCos[1]);
            float fVSinCos[2];
            sincos_tpl(cam.GetFov() * 0.5f, &fVSinCos[0], &fVSinCos[1]);
            float fZoom = 1.f / cam.GetFov();

            float fMin = fHSinCos[1] * fVSinCos[1] * (params.fCameraMinDistance * fZoom);

            bbSpaceLocal.max.CheckMin(Vec3(fHSinCos[0], 1.f, fVSinCos[0]) * (bbSpaceLocal.max.y * fZoom));
            bbSpaceLocal.min.CheckMax(Vec3(-bbSpaceLocal.max.x, fMin, -bbSpaceLocal.max.z));
        }

        // Compute params for quick space-loop bounds checking.
        QuatTS const& loc = GetMain().GetLocation();
        context.SpaceLoop.vCenter = loc * bbSpaceLocal.GetCenter();
        context.SpaceLoop.vSize = bbSpaceLocal.GetSize() * (0.5f * loc.s);
        Matrix33 mat(loc.q);
        for (int a = 0; a < 3; a++)
        {
            context.SpaceLoop.vScaledAxes[a] = mat.GetColumn(a);
            context.SpaceLoop.vSize[a] = max(context.SpaceLoop.vSize[a], 1e-6f);
            context.SpaceLoop.vScaledAxes[a] /= context.SpaceLoop.vSize[a];
        }
    }

    ParticleParams::STargetAttraction::ETargeting eTarget = params.TargetAttraction.eTarget;
    context.bHasTarget = eTarget == eTarget.OwnEmitter
        || (eTarget == eTarget.External && GetMain().GetTarget().bTarget);

    context.fMinStepTime = min(fUpdateTime, fMAX_FRAME_LIMIT_TIME) / float(nMAX_ITERATIONS);

    // Compute time and distance limits in certain situations.
    context.fMaxLinearDeviation = fHUGE;
    if (GetHistorySteps() > 0)
    {
        context.fMaxLinearDeviation = fMAX_RELATIVE_TAIL_DEVIATION * params.fSizeY.GetMaxValue();
        context.fMinStepTime = min(context.fMinStepTime, params.fTailLength.GetMaxValue() * 0.5f / GetHistorySteps());
    }
    if (context.nEnvFlags & ENV_COLLIDE_ANY)
    {
        context.fMaxLinearDeviation = min(context.fMaxLinearDeviation, fMAX_COLLIDE_DEVIATION);
    }

    // If random turbulence enabled, limit linear time based on deviation limit.
    // dp(t) = Turb/2 t^(3/2)
    // t = (2 Dev/Turb)^(2/3)
    if (params.fTurbulence3DSpeed)
    {
        context.fMaxLinearStepTime = min(context.fMaxLinearStepTime, powf(2.f * context.fMaxLinearDeviation / params.fTurbulence3DSpeed, 0.666f));
    }

    // If vortex turbulence enabled, find max rotation angle for allowed deviation.
    float fVortexRadius = params.fTurbulenceSize.GetMaxValue() * GetMain().GetParticleScale();
    if (fVortexRadius > context.fMaxLinearDeviation)
    {
        // dev(r, a) = r (1 - sqrt( (cos(a)+1) / 2 ))
        // cos(a) = 2 (1 - dev/r)^2 - 1
        float fCos = sqr(1.f - context.fMaxLinearDeviation / fVortexRadius) * 2.f - 1.f;
        float fAngle = RAD2DEG(acos(clamp_tpl(fCos, -1.f, 1.f)));
        context.fMaxLinearStepTime = div_min(fAngle, fabs(params.fTurbulenceSpeed.GetMaxValue()), context.fMaxLinearStepTime);
    }

    if (NeedsDynamicBounds() || !StaticBoundsStable()
        || (GetCVars()->e_ParticlesDebug & AlphaBits('bx')) || GetMain().IsEditSelected())
    {
        // Accumulate dynamic bounds.
        context.pbbDynamicBounds = &m_bbWorldDyn;
        if (NeedsDynamicBounds() || !StaticBoundsStable())
        {
            m_bbWorldDyn.Reset();
        }
    }
    else
    {
        context.pbbDynamicBounds = NULL;
    }

    context.fDensityAdjust = 1.f;
}

//////////////////////////////////////////////////////////////////////////
void CParticleContainer::UpdateParticles(const CCamera* overrideCamera)
{
    if (m_nNeedJobUpdate > 0)
    {
        m_nNeedJobUpdate--;
    }

    for (ParticleList<CParticleSubEmitter>::iterator emitter(m_Emitters); emitter != m_Emitters.end(); ++emitter)
    {
        emitter->UpdateEmitterBounds();
    }

    float fAge = GetAge();
    float fUpdateTime = fAge - m_fAgeLastUpdate;
    if (fUpdateTime > 0.f)
    {
        FUNCTION_PROFILER_CONTAINER(this);

        if (m_pParentContainer)
        {
            m_pParentContainer->UpdateParticles(overrideCamera);
        }

        SParticleUpdateContext context;
        ComputeUpdateContext(context, fUpdateTime);

        if (overrideCamera)
        {
            context.vMainCamPos = overrideCamera->GetPosition();
        }

        if (this->IsGPUParticle())
        {
            // Confetti TODO: update gpu particles using the update context
        }
        else
        {
            GetMain().GetPhysEnviron().LockAreas(context.nEnvFlags, 1);

            // Update all particle positions etc, delete dead particles.
            UpdateParticleStates(context);

            GetMain().GetPhysEnviron().LockAreas(context.nEnvFlags, -1);
        }

        m_fAgeLastUpdate = fAge;
    }
}

void CParticleContainer::SyncUpdateParticles(const CCamera* overrideCamera)
{
    GetMain().SyncUpdateParticlesJob();
    UpdateParticles(overrideCamera);
}

void CParticleContainer::UpdateParticleStates(SParticleUpdateContext& context)
{
    // GPU particles should never call this code
    assert(!this->IsGPUParticle());

    const ResourceParticleParams& params = GetParams();
    float fLifetimeCheck = 0.f;

    // Emit new particles.
    if ((context.nEnvFlags & REN_SORT) && context.nSortQuality > 0)
    {
        // Allocate particle sort helper array. Overestimate size from current + expected emitted count.
        int nMaxParticles = GetMaxParticleCount(context);
        STACK_ARRAY(SParticleUpdateContext::SSortElem, aParticleSort, nMaxParticles);
        context.aParticleSort.set(aParticleSort, nMaxParticles);
    }

    SParticleUpdateContext::SSortElem* pElem = context.aParticleSort.begin();
#define ERASE_PARTICLE(pPart) if (pPart->NumRefs() == 0) \
    {                                                    \
        pPart.erase();                                   \
        continue;                                        \
    }                                                    \
    else                                                 \
    {                                                    \
        pPart->Hide();                                   \
        m_Counts.ParticlesReject++;                      \
    }
    for (ParticleList<CParticle>::iterator pPart(m_Particles); pPart; )
    {
        // Update the particle before the IsAlive check so it can be removed immediately if it is killed by the update
        pPart->Update(context, context.fUpdateTime);

        if (!pPart->IsAlive(fLifetimeCheck))
        {
            if (m_pParams->bRemainWhileVisible)
            {
                if (!gEnv->p3DEngine->GetRenderingCamera().IsPointVisible(pPart->GetLocation().t))
                {
                    ERASE_PARTICLE(pPart)
                }
            }
            else
            {
                ERASE_PARTICLE(pPart)
            }
        }

        if (!context.aParticleSort.empty() && !params.IsConnectedParticles())
        {
            assert(pElem < context.aParticleSort.end());
            pElem->pPart = pPart;
            pElem->fDist = -pPart->GetMinDist(context.vMainCamPos);

            if (context.nSortQuality == 1 && pElem > context.aParticleSort.begin() && pElem->fDist < pElem[-1].fDist)
            {
                // Force progressive sort order.
                pElem->fDist = pElem[-1].fDist;
            }
            pElem++;
        }

        ++pPart;
    }

    UpdateEmitters(&context);
}

void CParticleContainer::UpdateEmitters(SParticleUpdateContext* pUpdateContext)
{
    for_all_ptrs(CParticleSubEmitter, e, m_Emitters)
    {
        if (e->UpdateState())
        {
            // Still alive.
            if (pUpdateContext)
            {
                // Emit particles.

                // Emit deferred particles first.
                // Array will only be present for 1st container update, which should have just one emitter.
                for_all_ptrs(const EmitParticleData, pEmit, m_DeferredEmitParticles)
                {
                    EmitParticleData emit = *pEmit;
                    if (!emit.bHasLocation)
                    {
                        emit.Location = e->GetSource().GetLocation();
                    }

                    e->EmitParticle(*pUpdateContext, emit);
                }

                e->EmitParticles(*pUpdateContext);
            }
        }
        else if (!(m_nEnvFlags & EFF_ANY) && IsIndirect() && e->NumRefs() == 0)
        {
            // Remove finished emitters; those with effects components removed in UpdateEffects, main thread.
            m_Emitters.erase(e);
        }
    }

    if (GetGPUData())
    {
        GetGPUData()->Update();
    }
}

void CParticleContainer::UpdateEffects()
{
    for_all_ptrs(CParticleSubEmitter, e, m_Emitters)
    {
        if (e->GetSource().IsAlive())
        {
            e->UpdateState();
            if (m_nEnvFlags & EFF_FORCE)
            {
                e->UpdateForce();
            }
            if (m_nEnvFlags & EFF_AUDIO)
            {
                e->UpdateAudio();
            }
        }
        else
        {
            // Emitters with effects removed here.
            e->Deactivate();
            if (IsIndirect() && e->NumRefs() == 0)
            {
                m_Emitters.erase(e);
            }
        }
    }
}

/* Water sorting / filtering:

Effect:::::::::::::::::::::::::::
Emitter Camera      above                   both                    below
------- ------      -----                   ----                    -----
above       above           AFTER                   AFTER                   skip
below           BEFORE              BEFORE              skip

both        above           AFTER\below     AFTER[\below]   BEFORE\above
below           BEFORE\below    AFTER[\above]   AFTER\above

below       above           skip                    BEFORE              BEFORE
below           skip                    AFTER                   AFTER
*/
void CParticleContainer::Render(SRendParams const& RenParams, SPartRenderParams const& PRParams, const SRenderingPassInfo& passInfo)
{
    FUNCTION_PROFILER_CONTAINER(this);

    const ResourceParticleParams* pParams = m_pParams;

    //It doesn't look like the nEnvFlags prefetch will be much use due its use almost immediately afterwards, but we're often cache
    //  missing on pParams->bEnabled, so that will give time for the cache line to be fetched
    PrefetchLine(&pParams->nEnvFlags, 0);
    PrefetchLine(&m_bbWorld, 0);
    PrefetchLine(pParams, 128);

    const float fDistSq = m_bbWorld.GetDistanceSqr(passInfo.GetCamera().GetPosition());

    if (!passInfo.IsAuxWindow())
    {
        // Individual container distance culling.  Don't do this check if rendering in a preview window.  It is assumed that user wants to see their particle systems.
        float fMaxDist = pParams->GetMaxParticleSize() * pParams->fViewDistanceAdjust * PRParams.m_fMaxAngularDensity;

        if (fDistSq > sqr(fMaxDist) && !pParams->fMinPixels)
        {
            return;
        }

        IF (pParams->fCameraMaxDistance, 0)
        {
            float fFOV = passInfo.GetCamera().GetFov();
            if (fDistSq * sqr(fFOV) > sqr(pParams->fCameraMaxDistance))
            {
                return;
            }
        }

        // Water and indoor culling tests.
        if (!(pParams->tVisibleIndoors * GetMain().GetVisEnviron().OriginIndoors()))
        {
            return;
        }
        if (!(pParams->tVisibleUnderwater * GetMain().GetPhysEnviron().m_tUnderWater))
        {
            return;
        }

        if (!m_nNeedJobUpdate)
        {
            // Was not scheduled for threaded update, so sync existing emitter update before creating new job.
            GetMain().SyncUpdateParticlesJob();
        }

        // Flag this and all parents for update this frame; Set count to 2, to automatically prime updates at start of next frame too.
        if (GetCVars()->e_ParticlesThread)
        {
            for (CParticleContainer* pCont = this; pCont; pCont = pCont->GetParent())
            {
                pCont->SetNeedJobUpdate(2);
            }
        }
        else
        {
            UpdateParticles(&passInfo.GetCamera());
        }
    }
    else
    {
        SyncUpdateParticles(&passInfo.GetCamera());
    }

    uint32 nRenderFlags = GetEnvironmentFlags() & PRParams.m_nRenFlags;

    if (nRenderFlags & REN_LIGHTS)
    {
        // We dont support light emitting particles for the GPU particles
        if (!this->IsGPUParticle())
        {
            SyncUpdateParticles(&passInfo.GetCamera());
            RenderLights(RenParams, passInfo);
        }
    }

    if (nRenderFlags & REN_GEOMETRY)
    {
        // We dont support geometry rendering for gpu particles
        if (!this->IsGPUParticle())
        {
            SyncUpdateParticles(&passInfo.GetCamera());
            RenderGeometry(RenParams, passInfo);
        }
    }
    else if (nRenderFlags & REN_DECAL)
    {
        // We dont support decal rendering for gpu particles
        if (!this->IsGPUParticle())
        {
            SyncUpdateParticles(&passInfo.GetCamera());
            RenderDecals(passInfo);
        }
    }
    else if (nRenderFlags & REN_SPRITE)
    {
        // Copy pre-computed render and state flags.
        uint32 nObjFlags = pParams->nRenObjFlags & PRParams.m_nRenObjFlags;

        IF (pParams->eFacing == pParams->eFacing.Water, 0)
        {
            // Water-aligned particles are always rendered before water, to avoid intersection artifacts
            if (passInfo.IsRecursivePass())
            {
                return;
            }
            IF (Get3DEngine()->GetOceanRenderFlags() & OCR_NO_DRAW, 0)
            {
                return;
            }
        }
        else
        {
            // Other particles determine whether to render before or after water, or both
            bool bCameraUnderWater = passInfo.IsCameraUnderWater() ^ passInfo.IsRecursivePass();
            if (!passInfo.IsRecursivePass() && pParams->tVisibleUnderwater == ETrinary() && GetMain().GetPhysEnviron().m_tUnderWater == ETrinary())
            {
                // Must render in 2 passes.
                if (!(nObjFlags & FOB_AFTER_WATER))
                {
                    SPartRenderParams PRParamsAW = PRParams;
                    PRParamsAW.m_nRenObjFlags |= FOB_AFTER_WATER;
                    Render(RenParams, PRParamsAW, passInfo);
                }
            }
            else if (pParams->tVisibleUnderwater * bCameraUnderWater
                     && GetMain().GetPhysEnviron().m_tUnderWater * bCameraUnderWater)
            {
                nObjFlags |= FOB_AFTER_WATER;
            }
        }

        SAddParticlesToSceneJob& job = CParticleManager::Instance()->GetParticlesToSceneJob(passInfo);
        job.pPVC = this;
        job.pRenderObject = gEnv->pRenderer->EF_GetObject_Temp(passInfo.ThreadID());
        SRenderObjData* pOD = gEnv->pRenderer->EF_GetObjData(job.pRenderObject, true, passInfo.ThreadID());

        float fEmissive = pParams->fEmissiveLighting;

        if (nObjFlags & FOB_OCTAGONAL)
        {
            // Size threshold for octagonal rendering.
            static const float fOCTAGONAL_PIX_THRESHOLD = 20;
            if (sqr(pParams->fSizeY.GetMaxValue() * pParams->fFillRateCost * passInfo.GetCamera().GetViewSurfaceZ())
                < sqr(fOCTAGONAL_PIX_THRESHOLD * passInfo.GetCamera().GetFov()) * fDistSq)
            {
                nObjFlags &= ~FOB_OCTAGONAL;
            }
        }

        //Note: it used the lower 8 bits of render object flags from particle parameters for RState. 
        //(No idea why, but any flags from lower 8bits need to set after these)
        job.pRenderObject->m_RState = uint8(nObjFlags);
        job.pRenderObject->m_ObjFlags |= (nObjFlags & ~0xFF) | RenParams.dwFObjFlags;

        //if the particles are not rendered in preview, add flag so it may be rendered after depth of field
        if (!GetParams().DepthOfFieldBlur && !m_pMainEmitter->GetPreviewMode())
        {
            job.pRenderObject->m_ObjFlags |= FOB_RENDER_TRANS_AFTER_DOF;
        }

        // Ambient color for shader incorporates actual ambient lighting, as well as the constant emissive value.
        job.pRenderObject->m_II.m_Matrix.SetIdentity();
        job.pRenderObject->m_II.m_AmbColor = ColorF(fEmissive) + RenParams.AmbientColor * pParams->fDiffuseLighting * Get3DEngine()->m_fParticlesAmbientMultiplier;
        job.pRenderObject->m_II.m_AmbColor.a = pParams->fDiffuseLighting * Get3DEngine()->m_fParticlesLightMultiplier;

        if (RenParams.bIsShowWireframe) 
        {
            // diffuse texture is set to black to cancel out alpha and make wireframe easy to see
            job.pRenderObject->m_nTextureID = gEnv->pRenderer->GetBlackTextureId();
            job.pRenderObject->m_nParticleGlowTexID = gEnv->pRenderer->GetBlackTextureId();
        }
        else
        {
            job.pRenderObject->m_nTextureID = pParams->nTexId;
            job.pRenderObject->m_nParticleGlowTexID = pParams->nGlowTexId;
        }

        pOD->m_FogVolumeContribIdx[0] = pOD->m_FogVolumeContribIdx[1] = PRParams.m_nFogVolumeContribIdx;

        pOD->m_LightVolumeId = PRParams.m_nDeferredLightVolumeId;

        pOD->m_pParticleParams = &GetEffect()->GetParams();

        IF (!!pParams->fHeatScale, 0)
        {
            uint32 nHeatAmount = pParams->fHeatScale.GetStore();
        }

        // Set sort distance based on params and bounding box.
        if (pParams->fSortBoundsScale == PRParams.m_fMainBoundsScale)
        {
            job.pRenderObject->m_fDistance = PRParams.m_fCamDistance * passInfo.GetZoomFactor();
        }
        else
        {
            job.pRenderObject->m_fDistance = GetMain().GetNearestDistance(passInfo.GetCamera().GetPosition(), pParams->fSortBoundsScale) * passInfo.GetZoomFactor();
        }
        job.pRenderObject->m_fDistance += pParams->fSortOffset;
        job.pRenderObject->m_ParticleObjFlags = (pParams->bHalfRes ? CREParticle::ePOF_HALF_RES : 0)
            | (pParams->bVolumeFog ? CREParticle::ePOF_VOLUME_FOG : 0);

        //
        // Set remaining SAddParticlesToSceneJob data.
        //

        if (pParams->pMaterial)
        {
            _smart_ptr<IMaterial> material = pParams->pMaterial;
            SShaderItem shaderItem = material->GetShaderItem();
            if (shaderItem.m_pShader == nullptr)
            {
                //Attempt retrieving sub-materials if the base material was invalid
                bool subMtlExists = false;
                for (size_t i = 0; i < material->GetSubMtlCount(); i++)
                {
                    //Check if the sub-material actually exists. GetSubMtl will 
                    //return nullptr if it doesn't but GetShaderItem(subMtlIndex) will
                    //return the shader item of the default material if it fails. 
                    subMtlExists = material->GetSubMtl(i) != nullptr;

                    //If no valid sub-material exists this will still leave 
                    //us with the default material's shader item
                    shaderItem = material->GetShaderItem(i);

                    if (subMtlExists == true && shaderItem.m_pShader != nullptr)
                    {
                        break;
                    }
                }

                if (!subMtlExists || shaderItem.m_pShader == nullptr)
                {
                    //Ensure that we're using the default material
                    shaderItem = Get3DEngine()->GetMaterialManager()->GetDefaultMaterial()->GetShaderItem();
                    //Issue a warning if we're using the default sub-material's shader item
                    AZ_Warning("Particle Container", false, "No valid shader item was found for %s or any of its sub-materials. Using default material.", material->GetName());
                }

                material->SetShaderItem(shaderItem);
            }

            job.pShaderItem = &material->GetShaderItem();
            if (job.pShaderItem->m_pShader && (job.pShaderItem->m_pShader->GetFlags() & EF_REFRACTIVE))
            {
                SetScreenBounds(passInfo.GetCamera(), pOD->m_screenBounds);
            }
        }
        else
        {
            job.pShaderItem = &CParticleManager::Instance()->GetLightShader(m_pParams->UseTextureMirror())->GetShaderItem();
            if (pParams->fTexAspect == 0.f)
            {
                non_const(*m_pParams).UpdateTextureAspect();
            }
        }

        job.nCustomTexId = RenParams.nTextureID;
        //Set lod alpha blending in temp var. this is used to multiply by the alpha scaling in the d3dhwshader
        pOD->m_fTempVars[0] = m_LodBlend;
    }
}

CLodInfo* CParticleContainer::GetStartLod()
{
    return m_StartLodInfoPtr;
}

void    CParticleContainer::SetEndLod(CLodInfo* lod)
{
    m_EndLodInfoPtr = lod;
}

void    CParticleContainer::SetStartLod(CLodInfo* lod)
{
    m_StartLodInfoPtr = lod;
}

void CParticleContainer::SetScreenBounds(const CCamera& cam, uint8 aScreenBounds[4])
{
    const int32 align16 = (16 - 1);
    const int32 shift16 = 4;
    int iOut[4];
    int nWidth = cam.GetViewSurfaceX();
    int nHeight = cam.GetViewSurfaceZ();
    AABB posAABB = AABB(cam.GetPosition(), 1.00f);
    if (!posAABB.IsIntersectBox(m_bbWorld))
    {
        cam.CalcScreenBounds(&iOut[0], &m_bbWorld, nWidth, nHeight);
        if (((iOut[2] - iOut[0]) == nWidth) && ((iOut[3] - iOut[1]) == nHeight))
        {
            iOut[2] += 16;  // Split fullscreen particles and fullscreen geometry. Better to use some sort of ID/Flag, but this will do the job for now
        }
        aScreenBounds[0] = min(iOut[0] >> shift16, (int32)255);
        aScreenBounds[1] = min(iOut[1] >> shift16, (int32)255);
        aScreenBounds[2] = min((iOut[2] + align16) >> shift16, (int32)255);
        aScreenBounds[3] = min((iOut[3] + align16) >> shift16, (int32)255);
    }
    else
    {
        aScreenBounds[0] = 0;
        aScreenBounds[1] = 0;
        aScreenBounds[2] = min((nWidth >> shift16) + 1, (int32)255);
        aScreenBounds[3] = min((nHeight >> shift16) + 1, (int32)255);
    }
}

bool CParticleContainer::NeedsUpdate() const
{
    return GetMain().GetAge() > m_fAgeLastUpdate;
}

void CParticleContainer::SetDynamicBounds()
{
    m_nEnvFlags |= EFF_DYNAMIC_BOUNDS;
    for (CParticleContainer* pParent = m_pParentContainer; pParent; pParent = pParent->GetParent())
    {
        pParent->m_nChildFlags |= EFF_DYNAMIC_BOUNDS;
    }
    GetMain().AddEnvFlags(EFF_DYNAMIC_BOUNDS);
}

float CParticleContainer::GetEmitterLife() const
{
    float fEmitterLife = GetMain().GetStopAge();
    if (!m_pParentContainer)
    {
        if (!m_pParams->fPulsePeriod)
        {
            if (!m_Emitters.empty())
            {
                fEmitterLife = min(fEmitterLife, m_Emitters.front().GetStopAge());
            }
        }
    }
    else
    {
        fEmitterLife = min(fEmitterLife, m_pParentContainer->GetContainerLife());
        if (!m_pParams->bContinuous)
        {
            fEmitterLife = min(fEmitterLife, m_pParentContainer->GetEmitterLife() + m_pParams->GetMaxSpawnDelay());
        }
    }
    return fEmitterLife;
}

int CParticleContainer::GetMaxParticleCount(const SParticleUpdateContext& context) const
{
    const ResourceParticleParams& params = GetParams();

    // Start count is existing number of particles plus deferred
    int nCount = m_Particles.size() + m_DeferredEmitParticles.size();

    // Quickly (over)estimate max emissions for this frame
    float fEmitCount = params.GetCount().GetMaxValue();
    if (!GetParent())
    {
        fEmitCount *= GetMain().GetEmitCountScale();
    }
    if (params.eGeometryPieces == params.eGeometryPieces.AllPieces && params.pStatObj != 0)
    {
        // Emit 1 particle per piece.
        int nPieces = GetSubGeometryCount(params.pStatObj);
        fEmitCount *= max(nPieces, 1);
    }

    if (params.bContinuous)
    {
        // Compute emission rate
        float fEmitRate = fEmitCount / params.GetMaxParticleLife();
        if (params.fMaintainDensity)
        {
            fEmitRate *= fMAX_DENSITY_ADJUST;
        }

        // Determine time window to update.
        float fEmitTime = min(context.fUpdateTime, GetMaxParticleFullLife());
        nCount += int_ceil(fEmitTime * fEmitRate) * m_Emitters.size();
    }
    else
    {
        // Account for at least one pulse, plus another if pulse repeating
        int nEmitCount = int_ceil(fEmitCount) * m_Emitters.size();
        nCount = max(nCount, nEmitCount);
        if (params.fPulsePeriod)
        {
            nCount += nEmitCount;
        }
    }

    return nCount;
}

void CParticleContainer::UpdateContainerLife(float fAgeAdjust)
{
    m_fAgeLastUpdate += fAgeAdjust;
    m_fAgeStaticBoundsStable += fAgeAdjust;

    if (CParticleSubEmitter* pDirectEmitter = GetDirectEmitter())
    {
        pDirectEmitter->UpdateState(fAgeAdjust);
    }

    m_fContainerLife = GetEmitterLife() + m_pParams->fParticleLifeTime.GetMaxValue();
    if (!m_pParams->fParticleLifeTime && m_pParentContainer)
    {
        // Zero particle life implies particles live as long as parent emitter lives.
        m_fContainerLife += m_pParentContainer->m_fContainerLife;
    }

    if (GetGPUData())
    {
        GetGPUData()->OnContainerUpdateLife();
    }
}

float CParticleContainer::GetAge() const
{
    return GetMain().GetAge();
}

void CParticleContainer::SetGPUData(CParticleContainerGPU* pGPUData)
{
    // Delete previous instance if exists.
    if (pGPUData == m_pGPUData)
    {
        return;
    }
    if (m_pGPUData)
    {
        delete m_pGPUData;
    }

    // Assign new instance.
    m_pGPUData = pGPUData;
}

void CParticleContainer::Reset()
{
    // Free all particle and sub-emitter memory.
    m_Particles.clear();
    if (IsIndirect())
    {
        m_Emitters.clear();
    }
    m_fAgeLastUpdate = 0.f;
    m_bbWorldDyn.Reset();
}

// Stat functions.
void CParticleContainer::GetCounts(SParticleCounts& counts) const
{
    counts.EmittersAlloc += 1.f;
    counts.ParticlesAlloc += m_Particles.size();

    if (GetTimeToUpdate() == 0.f)
    {
        // Was updated this frame.
        AddArray(FloatArray(counts), FloatArray(m_Counts));
        counts.EmittersActive += 1.f;
        counts.ParticlesActive += m_Particles.size();
        counts.SubEmittersActive += m_Emitters.size();

        if (m_Counts.ParticlesCollideTest)
        {
            counts.nCollidingEmitters += 1;
            counts.nCollidingParticles += (int)m_Counts.ParticlesCollideTest;
        }

        if ((m_nEnvFlags & REN_ANY) && !m_bbWorldDyn.IsReset())
        {
            counts.DynamicBoundsVolume += m_bbWorldDyn.GetVolume();
            counts.StaticBoundsVolume += m_bbWorld.GetVolume();
            if (!m_bbWorld.ContainsBox(m_bbWorldDyn))
            {
                AABB bbDynClip = m_bbWorldDyn;
                bbDynClip.ClipToBox(m_bbWorld);
                float fErrorVol = m_bbWorldDyn.GetVolume() - bbDynClip.GetVolume();
                counts.ErrorBoundsVolume += fErrorVol;
            }
        }
    }
}

void CParticleContainer::GetMemoryUsage(ICrySizer* pSizer) const
{
    m_Particles.GetMemoryUsagePlain(pSizer);
    pSizer->AddObject(&m_Particles, CParticle::GetAllocationSize(this) * m_Particles.size());
    m_Emitters.GetMemoryUsage(pSizer);
    m_DeferredEmitParticles.GetMemoryUsagePlain(pSizer);
    m_ExternalStatObjs.GetMemoryUsagePlain(pSizer);
}

void CParticleContainer::OffsetPosition(const Vec3& delta)
{
    m_bbWorld.Move(delta);
    m_bbWorldStat.Move(delta);
    m_bbWorldDyn.Move(delta);

    for_all_ptrs(CParticleSubEmitter, e, m_Emitters)
    {
        e->OffsetPosition(delta);
    }

    for (ParticleList<CParticle>::iterator pPart(m_Particles); pPart; ++pPart)
    {
        pPart->OffsetPosition(delta);
    }
}

void CParticleContainer::SetUsed(bool used)
{
    m_bUsed = used;

    // If the container is used, recursively set the parent container to be used,
    // which will prevent the emitter to erase the parent container.
    if (m_bUsed && m_pParentContainer)
    {
        m_pParentContainer->SetUsed(m_bUsed);
    }
}
