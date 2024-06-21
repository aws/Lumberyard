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
#include "StdAfx.h"

#include "CloudComponent.h"
#include "CloudComponentRenderNode.h"
#include "CloudVolumeSprite.h"
#include "CloudVolumeVoxel.h"
#include "CloudVolumeTexture.h"

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/RTTI/BehaviorContext.h>

#include <MathConversion.h>
#include <I3DEngine.h>
#include <ICryAnimation.h>

namespace CloudsGem
{
    // Default aterial used to render sprite based clouds
    static const char* s_baseCloudsMaterial = "materials/clouds/baseclouds";
    
    // Default aterial used to render volumetric clouds
    static const char* s_volumeCloudsMaterial = "materials/volumedata/volumeClouds";

    // Threshold for movement when auto move is disabled
    static const float s_movementThreshold = EPSILON;

    void CloudComponentRenderNode::MovementProperties::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            using MoveProps = CloudComponentRenderNode::MovementProperties;
            serializeContext->Class<MoveProps>()
                ->Version(1)
                ->Field("AutoMove", &MoveProps::m_autoMove)
                ->Field("Speed", &MoveProps::m_speed)
                ->Field("FadeDistance", &MoveProps::m_fadeDistance)
                ->Field("LoopBoxDimensions", &MoveProps::m_loopBoxDimensions);
        }
    }

    void CloudComponentRenderNode::VolumetricProperties::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            using VolumetricProps = CloudComponentRenderNode::VolumetricProperties;
            serializeContext->Class<VolumetricProps>()
                ->Version(1)
                ->Field("IsVolumetric", &VolumetricProps::m_isVolumetricRenderingEnabled)
                ->Field("VolumeMaterial", &VolumetricProps::m_material)
                ->Field("Density", &VolumetricProps::m_density);
        }
    }

    void CloudComponentRenderNode::VolumetricProperties::OnIsVolumetricPropertyChanged()
    {
        EditorCloudComponentRequestBus::Event(m_parentEntityId, &EditorCloudComponentRequestBus::Events::Refresh);
    }
    
    void CloudComponentRenderNode::VolumetricProperties::OnDensityChanged()
    {
        EditorCloudComponentRequestBus::Event(m_parentEntityId, &EditorCloudComponentRequestBus::Events::Refresh);
    }

    void CloudComponentRenderNode::VolumetricProperties::OnMaterialAssetPropertyChanged()
    {
        EditorCloudComponentRequestBus::Event(m_parentEntityId, &EditorCloudComponentRequestBus::Events::OnMaterialAssetChanged);
    }
    
    void CloudComponentRenderNode::DisplayProperties::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            using DisplayProps = CloudComponentRenderNode::DisplayProperties;
            serializeContext->Class<DisplayProps>()
                ->Version(1)
                ->Field("DisplaySpheres", &DisplayProps::m_isDisplaySpheresEnabled)
                ->Field("DisplayVolumes", &DisplayProps::m_isDisplayVolumesEnabled)
                ->Field("DisplayBounds", &DisplayProps::m_isDisplayBoundsEnabled);
        }
    }

    void CloudComponentRenderNode::Reflect(AZ::ReflectContext* context)
    {
        MovementProperties::Reflect(context);
        VolumetricProperties::Reflect(context);
        DisplayProperties::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<CloudComponentRenderNode>()
                ->Version(1)
                ->Field("Visible", &CloudComponentRenderNode::m_isVisible)
                ->Field("ParticleData", &CloudComponentRenderNode::m_cloudParticleData)
                ->Field("BaseMaterial", &CloudComponentRenderNode::m_material)
                ->Field("Volumetric", &CloudComponentRenderNode::m_volumeProperties)
                ->Field("Movement", &CloudComponentRenderNode::m_movementProperties)
                ->Field("Display", &CloudComponentRenderNode::m_displayProperties);
        }
    }
    CloudComponentRenderNode::CloudComponentRenderNode()
    {
        m_material.SetAssetPath(s_baseCloudsMaterial);
        m_volumeProperties.GetMaterialAsset().SetAssetPath(s_volumeCloudsMaterial);
        m_worldMatrix.SetIdentity();
    }

    CloudComponentRenderNode::~CloudComponentRenderNode()
    {
        gEnv->p3DEngine->FreeRenderNodeState(this);

        AZ::SystemTickBus::Handler::BusDisconnect();
        CloudComponentBehaviorRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
    }

    const AABB CloudComponentRenderNode::GetBBox() const 
    { 
        return m_cloudVolume ? m_cloudVolume->GetBoundingBox() : AABB(AABB::RESET);
    }

    void CloudComponentRenderNode::SetBBox(const AABB& box)
    { 
        if (m_cloudVolume)
        {
            m_cloudVolume->SetBoundingBox(box);
        }
    }

    void CloudComponentRenderNode::CopyPropertiesTo(CloudComponentRenderNode& rhs) const
    {
        rhs.m_isVisible = m_isVisible;
        rhs.m_cloudParticleData = m_cloudParticleData;
        rhs.m_material = m_material;
        rhs.m_volumeProperties = m_volumeProperties;
        rhs.m_movementProperties = m_movementProperties;
        rhs.m_displayProperties = m_displayProperties;
    }

    float CloudComponentRenderNode::GetMaxViewDist()
    {
        float viewDistanceMin = gEnv->pRenderer->GetFloatConfigurationValue("e_ViewDistMin", 1.0f);
        float viewDistanceRatio = gEnv->pRenderer->GetFloatConfigurationValue("e_ViewDistRatio", 1.0f);
        return max(viewDistanceMin, GetBBox().GetRadius() * viewDistanceRatio * GetViewDistanceMultiplier());
    }
        
    void CloudComponentRenderNode::AttachToEntity(AZ::EntityId id)
    {
        using namespace AZ;
        if (id.IsValid())
        {
            m_attachedToEntityId = id;
            m_volumeProperties.SetEntityId(m_attachedToEntityId);

            Transform parentTransform = Transform::CreateIdentity();
            EBUS_EVENT_ID_RESULT(parentTransform, m_attachedToEntityId, TransformBus, GetWorldTM);
            SetWorldTransform(parentTransform);

            SystemTickBus::Handler::BusConnect();
            CloudComponentBehaviorRequestBus::Handler::BusConnect(m_attachedToEntityId);
            AZ::TransformNotificationBus::Handler::BusConnect(m_attachedToEntityId);
            Refresh();
        }
        else
        {
            CloudComponentBehaviorRequestBus::Handler::BusDisconnect();
            SystemTickBus::Handler::BusConnect();
            AZ::TransformNotificationBus::Handler::BusDisconnect();

            gEnv->p3DEngine->FreeRenderNodeState(this);
            m_cloudVolume = nullptr;
        }
    }

    void CloudComponentRenderNode::SetWorldTransform(const AZ::Transform & entityTransform)
    {
        m_worldMatrix = AZTransformToLYTransform(entityTransform);
        m_entityWorldMatrix = m_worldMatrix;
    }

    void CloudComponentRenderNode::UpdateWorldTransform(const AZ::Transform& entityTransform)
    {
        SetWorldTransform(entityTransform);
        Update();
    }

    void CloudComponentRenderNode::Update()
    {
        if (m_cloudVolume)
        {
            m_cloudVolume->Update(m_worldMatrix, m_cloudParticleData.GetOffset());
        }
    }

    void CloudComponentRenderNode::Refresh()
    {
        gEnv->p3DEngine->FreeRenderNodeState(this);

        if (IsVolumetricRenderingEnabled())
        {
            m_cloudVolume = AZStd::make_shared<CloudVolumeVoxel>();
        }
        else
        {
            m_cloudVolume = AZStd::make_shared<CloudVolumeSprite>();
        }

        m_cloudVolume->SetDensity(m_volumeProperties.m_density);
        m_cloudVolume->Refresh(m_cloudParticleData, m_worldMatrix);
        gEnv->p3DEngine->RegisterEntity(this);

        OnMaterialAssetChanged();
    }
    
    void CloudComponentRenderNode::OnTransformChanged(const AZ::Transform&, const AZ::Transform& parentWorld)
    {
        UpdateWorldTransform(parentWorld);
    }

    void CloudComponentRenderNode::SetMaterial(MaterialPtr pMat) 
    {
        if (m_cloudVolume && pMat)
        {
            MaterialAssetRef asset = IsVolumetricRenderingEnabled() ? m_volumeProperties.GetMaterialAsset() : m_material;
            m_cloudVolume->SetMaterial(gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(pMat->GetName()));
        }
    }
    
    MaterialPtr CloudComponentRenderNode::GetMaterial(Vec3* /* pHitPos*/) 
    { 
        return m_cloudVolume ? m_cloudVolume->GetMaterial() : nullptr;
    }
   
    void CloudComponentRenderNode::OnMaterialAssetChanged()
    {
        if (m_cloudVolume)
        {
            MaterialAssetRef& materialAsset = IsVolumetricRenderingEnabled() ? m_volumeProperties.GetMaterialAsset() : m_material;
            m_cloudVolume->SetMaterial(gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialAsset.GetAssetPath().c_str()));
        }
    }

    void CloudComponentRenderNode::OnGeneralPropertyChanged()
    {
        Refresh();
    }

    void CloudComponentRenderNode::Move()
    {
        // update position
        float deltaTime = gEnv->pTimer->GetFrameTime();
        if (deltaTime > 0)
        {
            // Transform loop box and velocity vectors to world space
            float invScale = 1.0f / m_worldMatrix.GetColumn0().GetLength();
            const Vec3 loopBoxDimensions = AZVec3ToLYVec3(m_movementProperties.m_loopBoxDimensions) * 0.5 * invScale;
            AABB loopBox(-loopBoxDimensions, loopBoxDimensions);
            loopBox.SetTransformedAABB(m_entityWorldMatrix, loopBox);

            // Apply entity rotation to the velocity vector
            Matrix33 rotation;
            m_worldMatrix.GetRotation33(rotation);
            const Vec3 velocity = rotation * AZVec3ToLYVec3(deltaTime * m_movementProperties.m_speed);

            // Handle loop box crossings
            Vec3 pos = GetPos() + velocity;
            const Vec3 origin = m_entityWorldMatrix.GetTranslation();
            const bool currentPositionInsideLoopBox = loopBox.IsContainPoint(pos);
            if (m_isInsideLoopBox && !currentPositionInsideLoopBox)
            {
                pos = origin - (pos - origin);
                m_isInsideLoopBox = false;
            }
            else
            {
                m_isInsideLoopBox = currentPositionInsideLoopBox;
            }

            // Update world matrix with new position of cloud
            m_worldMatrix.SetTranslation(pos);

            // Fade out clouds at the borders of the loop box
            const float fadeDistance = m_movementProperties.m_fadeDistance;
            if (fadeDistance > 0.0f)
            {
                // Find fade distance along each axis. 
                const Vec3 distance = pos - origin;
                const float x = max(loopBoxDimensions.x, fadeDistance) - fabs(distance.x);
                const float y = max(loopBoxDimensions.y, fadeDistance) - fabs(distance.y);
                const float z = max(loopBoxDimensions.z, fadeDistance) - fabs(distance.z);

                // Determine how much to fade based on the closest face of the loop box
                const float smallestComponent = min(min(x, y), z);
                const float normalizedDistance = smallestComponent / fadeDistance;
                m_alpha = clamp_tpl(normalizedDistance, 0.0f, 1.0f);
            }
            Update();
        }
    }

    void CloudComponentRenderNode::OnSystemTick()
    {
        if (m_movementProperties.m_autoMove)
        {
            Move();
            return;
        }
        
        // Handle case where the entity has moved under user or script control
        const Vec3 origin = m_entityWorldMatrix.GetTranslation();
        if ((origin - GetPos()).GetLengthSquared() > s_movementThreshold)
        {
            // Reset alpha in case the cloud was already faded out
            m_alpha = 1.0f;
            m_worldMatrix.SetTranslation(origin);
            Update();
        }
    }

    void CloudComponentRenderNode::Render(const struct SRendParams& params, const struct SRenderingPassInfo& passInfo)
    {
        if (m_isVisible && m_cloudVolume && !passInfo.IsRecursivePass() && passInfo.RenderClouds())
        {
            m_dwRndFlags |= ERF_OUTDOORONLY;

            Vec3 position = m_worldMatrix.GetTranslation();
            const float viewDistanceLimit = GetMaxViewDist();
            const float maxViewDistanceAttenuation = 0.9f;
            const float minViewDistanceAttenuation = 0.1f;
            const float maxViewDistance = viewDistanceLimit * maxViewDistanceAttenuation;
            const float minViewDistance = viewDistanceLimit * minViewDistanceAttenuation;
            const float viewDistance = (passInfo.GetCamera().GetPosition() - position).GetLength();
            if (viewDistance > maxViewDistance)
            {
                float distance = viewDistance - maxViewDistance;
                float normalizedDistance = distance / minViewDistance;
                m_alpha = clamp_tpl(normalizedDistance, 1.0f, 0.0f);
            }

            const int isAfterWater = gEnv->p3DEngine->GetObjManager()->IsAfterWater(position, passInfo) ? 1 : 0;
            m_cloudVolume->Render(params, passInfo, m_alpha, isAfterWater);
        }
    }

}
