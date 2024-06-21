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
#pragma once

namespace CloudsGem
{
    class EditorCloudComponent;
    class CloudGenerator
    {
    public:
        AZ_TYPE_INFO(CloudGenerator, "{E63163E8-FCE9-45A8-B8AE-ABD3473E5D23}");

        void Generate(CloudParticleData& cloudData, const AZ::EntityId entityId, GenerationFlag flag);
        void Initialize(CloudParticleData& cloudData);
        void Destroy();

        static void Reflect(AZ::ReflectContext* context);
        static void OnAtlasColumnsChanged();
        static void OnSelectedAtlasRowChanged();
        static void OnSpriteCountChanged();
        static void OnSizeChanged();
        static void OnSizeVarianceChanged();
        static void OnMinimumDistanceChanged();
        static void OnPropertiesChanged() {}
        static void OnSeedChanged();
        static AZ::Crc32 OnGenerateButtonPressed();
        static AZ::Crc32 OnAtlasRowsChanged();

        AZ::s64 GetMaxSelectableRow() { return m_rows - 1; }

        // Members
        uint32 m_rows{ 4 };             ///< The number of rows in the clouds texture. Leave at 4 when using the default texture
        uint32 m_columns{ 4 };          ///< The number of columns in the clouds texture. Leave at 4 when using the default texture
        uint32 m_spriteRow{ 0 };        ///< Designates a rows in the cloud texture for rendering
        uint32 m_spriteCount{ 256 };    ///< Sets the number of sprites to be generated in the cloud
        uint32 m_seed{0};               ///< Seed for random number generator

        float m_spriteSize{ 10.0f };    ///< Sets the scale of the sprites in the cloud
        float m_sizeVariation{ 1.0f }; ///< Defines the randomization in size of the sprites within the cloud
        float m_minDistance{ 2.0f };    ///< Defines the minimum distance between the generated sprites within the cloud

        bool m_fillByVolume{ true };    ///< Indicates if boxes should be filled based on volume
        bool m_fillByLoopbox{ true };   ///< Indicates if the loopbox should be filled.
        bool m_generateButton{ false }; ///< Maintains state of the generation button

    protected:

        using CloudParticles = AZStd::vector<AZStd::shared_ptr<CloudParticle>>;
        static void RequestGeneration(GenerationFlag flags);
        
        void AddParticles(const AZ::EntityId& entityId, uint32 particleCount);
        void FilterByMinDistance();
        void FillParticleUVs(AZStd::shared_ptr<CloudParticle> particle);
        void FillVolumeWithParticles(const AZ::EntityId& entityId, bool useLocalTranslation, uint32 particleCount);
        void FillParticle(AZStd::shared_ptr<CloudParticle> particle, const AABB& box = AABB(AABB::RESET), GenerationFlag flags = GenerationFlag::GF_ALL);
        void UpdateParticleData(CloudParticleData& cloudData);
        void UpdateParticles(CloudParticleData& cloudData, GenerationFlag flag);
            
        // Returns the dimensions of the bounding box for the entity specified
        float GetBoxDimensions(const AZ::EntityId& entityId);
        
        CRndGen m_rng;                          ///< RNG used to generate particle attributes
        CloudParticles m_generatedParticles;    ///< Particles that have previously been generated
        CloudParticles m_filteredParticles;     ///< Particles that have been filtered by distance
        uint32 m_numChildrenEntities{ 0 };      ///< Number of children entities under the cloud
        float m_totalArea{ 0.0f };              ///< Total length of children shapes dimensions

    };
} // namespace CloudsGem


