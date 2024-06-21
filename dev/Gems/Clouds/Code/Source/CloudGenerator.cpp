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

// Description : CCloudGroup implementation.


#include "StdAfx.h"
#include "Util/PathUtil.h"
#include "Util/FileUtil.h"

#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/make_shared.h>

#include <AzFramework/Asset/AssetSystemBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/API/EntityPropertyEditorRequestsBus.h>

#include <LmbrCentral/Shape/BoxShapeComponentBus.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>
#include <QApplication>
#include <QMessageBox>

namespace CloudsGem
{
    void CloudGenerator::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<CloudGenerator>()
                ->Version(1)
                ->Field("Seed", &CloudGenerator::m_seed)
                ->Field("Rows", &CloudGenerator::m_rows)
                ->Field("Columns", &CloudGenerator::m_columns)
                ->Field("Sprites", &CloudGenerator::m_spriteCount)
                ->Field("SpriteRow", &CloudGenerator::m_spriteRow)
                ->Field("Scale", &CloudGenerator::m_spriteSize)
                ->Field("SizeVar", &CloudGenerator::m_sizeVariation)
                ->Field("MinDistance", &CloudGenerator::m_minDistance)
                ->Field("FillByArea", &CloudGenerator::m_fillByVolume)
                ->Field("FillByLoopbox", &CloudGenerator::m_fillByLoopbox)
                ->Field("Generate", &CloudGenerator::m_generateButton);
        }
    }

    void CloudGenerator::Initialize(CloudParticleData& cloudData)
    {
        // Synchronize the generator with the cloud particle data
        for (auto& particle : cloudData.GetParticles())
        {
            m_generatedParticles.push_back(AZStd::make_shared<CloudParticle>(particle));
        }
        FilterByMinDistance();
        UpdateParticleData(cloudData);
    }

    void CloudGenerator::Destroy()
    {
        m_generatedParticles.clear();
        m_filteredParticles.clear();
        m_numChildrenEntities = 0;
        m_totalArea = 0.f;
    }

    float CloudGenerator::GetBoxDimensions(const AZ::EntityId& entityId)
    {
        LmbrCentral::BoxShapeConfiguration boxShapeConfiguration;
        boxShapeConfiguration.m_dimensions = AZ::Vector3::CreateZero();
        EBUS_EVENT_ID_RESULT(boxShapeConfiguration, entityId, LmbrCentral::BoxShapeComponentRequestsBus, GetBoxConfiguration);
        const Vec3 dimensions = AZVec3ToLYVec3(boxShapeConfiguration.m_dimensions);
        return dimensions.GetLengthSquaredFloat();
    }

    void CloudGenerator::FillParticleUVs(AZStd::shared_ptr<CloudParticle> particle)
    {
        const int textureId = particle->GetTextureId() + m_spriteRow * m_columns;
        const float uStep = 1.0f / m_columns;
        const float vStep = 1.0f / m_rows;
        const int32 u = textureId % m_columns;
        const int32 v = textureId / m_columns;
        particle->SetUVs(AZ::Vector3{ u * uStep, v * vStep, 0 }, AZ::Vector3{ (u + 1) * uStep,  (v + 1) * vStep, 0 });
    }

    void CloudGenerator::FillParticle(AZStd::shared_ptr<CloudParticle> particle, const AABB& bounds, GenerationFlag flag)
    {
        bool isAll = flag == GenerationFlag::GF_ALL;
        if (isAll && !bounds.IsReset())
        {
            particle->SetPosition(m_rng.GetRandomComponentwise(bounds.min, bounds.max));
            particle->SetSize(m_spriteSize);
            particle->SetSizeVariance(m_rng.GetRandom<float>(-1.0f, 1.0f));
        }

        if (isAll || flag == GenerationFlag::GF_TEXTURE)
        {
            particle->SetTextureId(m_rng.GetRandom<uint32>(0, m_columns - 1));
            FillParticleUVs(particle);
        }

        if (isAll || flag == GenerationFlag::GF_ROW)
        {
            FillParticleUVs(particle);
        }

        if (isAll || flag == GenerationFlag::GF_SIZE_VAR)
        {
            particle->SetRadius(fabs(particle->GetSize() + (particle->GetSizeVariance() * m_sizeVariation * particle->GetSize())));
        }

        if (isAll || flag == GenerationFlag::GF_SIZE)
        {
            particle->SetSize(m_spriteSize);
            particle->SetRadius(fabs(particle->GetSize() + (particle->GetSizeVariance() * m_sizeVariation * particle->GetSize())));
        }
    }

    void CloudGenerator::FillVolumeWithParticles(const AZ::EntityId& entityId, bool useLocalTranslation, uint32 particleCount)
    {
        // Calculate bounding box
        LmbrCentral::BoxShapeConfiguration boxShapeConfiguration;
        EBUS_EVENT_ID_RESULT(boxShapeConfiguration, entityId, LmbrCentral::BoxShapeComponentRequestsBus, GetBoxConfiguration);
        const Vec3 dimensions = AZVec3ToLYVec3(boxShapeConfiguration.m_dimensions * 0.5f);
        AABB box(-dimensions, dimensions);

        // Determine position of box
        AZ::Vector3 translation {0.0f, 0.0f, 0.0f};
        if (useLocalTranslation)
        {
            EBUS_EVENT_ID_RESULT(translation, entityId, AZ::TransformBus, GetLocalTranslation);
            const Vec3 boxPosition = AZVec3ToLYVec3(translation);
            box.Move(boxPosition);
        }

        // Determine the number of particles to generate
        int count = m_fillByVolume ? aznumeric_cast<int>((dimensions.GetLengthSquaredFloat() / m_totalArea) * particleCount) : particleCount / m_numChildrenEntities;
        for (int i = 0; i < count; i++)
        {
            auto particle = AZStd::make_shared<CloudParticle>();
            FillParticle(particle, box);
            m_generatedParticles.emplace_back(particle);
        }
    }

    void CloudGenerator::UpdateParticleData(CloudParticleData& cloudData)
    {
        // Add remaining particles to the cloud while updating bounds
        AZStd::vector<CloudParticle>& particles = cloudData.GetParticles();
        particles.clear();

        AABB bounds(0.0f);
        for (auto particle : m_filteredParticles)
        {
            if (particle)
            {
                bounds.Add(particle->GetPosition(), particle->GetRadius());
                particles.push_back(*particle);
            }
        }

        // Offset particles so that bounding box is centered at origin of cloud system
        Vec3 offset = -bounds.GetCenter();
        cloudData.SetBounds(bounds);
        cloudData.SetOffset(offset);

        // Update particle positions taking offset into account
        for (CloudParticle& particle : particles)
        {
            particle.SetPosition(particle.GetPosition() + offset);
        }
    }

    void CloudGenerator::FilterByMinDistance()
    {
        // Make a copy of the particles in order to filter out the ones we actually want.
        m_filteredParticles.clear();
        for (auto& p : m_generatedParticles)
        {
            m_filteredParticles.push_back(p);
        }

        // Prune away particles that are too close together
        if (m_minDistance > 0.0f)
        {
            float distanceSquared = m_minDistance * m_minDistance;

            // Check distance between pairs of particles
            uint32 numParticles = m_filteredParticles.size();
            for (int i = 0; i < numParticles; i++)
            {
                for (int j = i + 1; j < numParticles; j++)
                {
                    auto particle0 = m_filteredParticles[i];
                    auto particle1 = m_filteredParticles[j];
                    if (particle0 && particle1)
                    {
                        if (particle0->GetPosition().GetSquaredDistance(particle1->GetPosition()) < distanceSquared)
                        {
                            m_filteredParticles[j] = nullptr;
                        }
                    }
                }
            }
        }
    }

    void CloudGenerator::AddParticles(const AZ::EntityId& entityId, uint32 particleCount)
    {
        m_totalArea = 0;
        AZStd::vector<AZ::EntityId> children;

        if (m_fillByLoopbox)
        {
            LmbrCentral::BoxShapeConfiguration boxShapeConfiguration;
            boxShapeConfiguration.m_dimensions = AZ::Vector3::CreateZero();
            EBUS_EVENT_ID_RESULT(boxShapeConfiguration, entityId, LmbrCentral::BoxShapeComponentRequestsBus, GetBoxConfiguration);
            const Vec3 dimensions = AZVec3ToLYVec3(boxShapeConfiguration.m_dimensions);
            m_totalArea = dimensions.GetLengthSquaredFloat();
            m_numChildrenEntities = 1;

            FillVolumeWithParticles(entityId, false, particleCount);
            return;
        }
        
        // Fill children
        EBUS_EVENT_ID_RESULT(children, entityId, AZ::TransformBus, GetChildren);
        m_numChildrenEntities = children.size();
        if (m_numChildrenEntities > 0)
        {
            // Compute total volume of all bounding boxes in which clouds will be placed
            for (uint32 i = 0; i < m_numChildrenEntities; ++i)
            {
                m_totalArea += GetBoxDimensions(children[i]);
            }

            // Fill each box
            for (uint32 i = 0; i < m_numChildrenEntities; ++i)
            {
                FillVolumeWithParticles(children[i], true, particleCount);
            }
            
            return;
        }
        
        QMessageBox::warning(QApplication::activeWindow(), QStringLiteral("Cannot generate cloud"), QString(
                "This entity has no child entities with BoxShape components to generate clouds within. "
                "Before attempting to Generate, create one or more entities with a BoxShape Component, "
                "and parent them in the entity hierarchy to the cloud entity. Their position and size will "
                "describe the desired volume(s) for generating individual clouds."), QMessageBox::Ok);
        
    }
    
    void CloudGenerator::UpdateParticles(CloudParticleData& cloudData, GenerationFlag flag)
    {
        for (auto particle : m_generatedParticles)
        {
            FillParticle(particle, cloudData.GetBounds(), flag);
        }
    }

    void CloudGenerator::Generate(CloudParticleData& cloudData, const AZ::EntityId entityId, GenerationFlag flag)
    {
        switch(flag)
        {
            case GenerationFlag::GF_ALL:
            case GenerationFlag::GF_SEED:
                m_generatedParticles.clear();
                m_seed = flag == GenerationFlag::GF_SEED ? m_seed : aznumeric_cast<uint32>(AZStd::GetTimeUTCMilliSecond());
                m_rng = CRndGen(m_seed);
                AddParticles(entityId, m_spriteCount);
                break;

            case GenerationFlag::GF_SPRITE_COUNT:
                if (m_generatedParticles.size() > m_spriteCount)
                {
                    m_generatedParticles.resize(m_spriteCount);
                }
                else
                {
                    AddParticles(entityId, std::max<int>(0, m_spriteCount - m_generatedParticles.size()));
                }
                break;

            case GenerationFlag::GF_MIN_DISTANCE:
                FilterByMinDistance();
                break;

            case GenerationFlag::GF_TEXTURE:
            case GenerationFlag::GF_SIZE:
            case GenerationFlag::GF_SIZE_VAR:
            default:
                UpdateParticles(cloudData, flag);
                break;
        }
        FilterByMinDistance();
        UpdateParticleData(cloudData);
    }

    void CloudGenerator::RequestGeneration(GenerationFlag flag)
    {
        // We need to use GetSelectedAndPinnedEntities as we have to handle the pinned inspector.
        AzToolsFramework::EntityIdList selectedEntityIds;
        AzToolsFramework::EntityPropertyEditorRequestBus::Broadcast(
            &AzToolsFramework::EntityPropertyEditorRequestBus::Events::GetSelectedAndPinnedEntities, selectedEntityIds);
        for (auto entityId : selectedEntityIds)
        {
            EditorCloudComponentRequestBus::Event(entityId, &EditorCloudComponentRequestBus::Events::Generate, flag);
        }
    }

    AZ::Crc32 CloudGenerator::OnGenerateButtonPressed()
    {
        RequestGeneration(GenerationFlag::GF_ALL);
        return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
    }

    AZ::Crc32 CloudGenerator::OnAtlasRowsChanged()
    {
        RequestGeneration(GenerationFlag::GF_TEXTURE);
        return AZ::Edit::PropertyRefreshLevels::AttributesAndValues;
    }

    void CloudGenerator::OnAtlasColumnsChanged()
    {
        RequestGeneration(GenerationFlag::GF_TEXTURE);
    }

    void CloudGenerator::OnSelectedAtlasRowChanged()
    {
        RequestGeneration(GenerationFlag::GF_ROW);
    }

    void CloudGenerator::OnSizeChanged()
    {
        RequestGeneration(GenerationFlag::GF_SIZE);
    }

    void CloudGenerator::OnSizeVarianceChanged()
    {
        RequestGeneration(GenerationFlag::GF_SIZE_VAR);
    }

    void CloudGenerator::OnMinimumDistanceChanged()
    {
        RequestGeneration(GenerationFlag::GF_MIN_DISTANCE);
    }

    void CloudGenerator::OnSpriteCountChanged()
    {
        RequestGeneration(GenerationFlag::GF_SPRITE_COUNT);
    }

    void CloudGenerator::OnSeedChanged()
    {
        RequestGeneration(GenerationFlag::GF_SEED);
    }
}
