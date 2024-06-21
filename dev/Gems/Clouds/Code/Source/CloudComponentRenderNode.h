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
#pragma once


#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/EntityBus.h>

#include <AzFramework/Asset/SimpleAsset.h>

#include <LmbrCentral/Rendering/MaterialOwnerBus.h>
#include <LmbrCentral/Rendering/MaterialAsset.h>

#include "CloudRenderElement.h"
#include "CloudComponentBus.h"
#include "CloudParticleData.h"

namespace CloudsGem
{
    class CloudImposterRenderElement;
    class CloudVolume;
    class CloudParticleData;
    using MaterialPtr = _smart_ptr<IMaterial>;

    struct ICloudVolume
    {
        virtual const AABB& GetBoundingBox() const = 0;
        virtual void SetDensity(float density) = 0;
        virtual void SetBoundingBox(const AABB& WSBBox) = 0;
        virtual void Render(const struct SRendParams& rParams, const SRenderingPassInfo& passInfo, float alpha, int isAfterWater) = 0;
        virtual void Update(const Matrix34& worldMatrix, const Vec3& offset) = 0;
        virtual void Refresh(CloudParticleData& data, const Matrix34& worldMatrix) = 0;
        virtual MaterialPtr GetMaterial() = 0;
        virtual void SetMaterial(MaterialPtr material) = 0;
    };

    class CloudComponentRenderNode
        : public IRenderNode
        , public CloudComponentBehaviorRequestBus::Handler
        , public AZ::SystemTickBus::Handler
        , public AZ::TransformNotificationBus::Handler
    {
        friend class EditorCloudComponent;
    public:

        using MaterialAssetRef = AzFramework::SimpleAssetReference<LmbrCentral::MaterialAsset>;

        AZ_TYPE_INFO(CloudComponentRenderNode, "{D21B87A9-32C9-4B74-879B-0721F7F1543C}");

        class MovementProperties
        {
        public:
            AZ_TYPE_INFO(MovementProperties, "{B621849E-DBB7-47E7-BBD4-4FCBFC16CA77}");
            MovementProperties() = default;

            bool m_autoMove{ false };
            float m_fadeDistance{ 0.0f };                           ///< Distance from edge of bounds at which cloud fades
            AZ::Vector3 m_speed{ 0.0f, 0.0f, 0.0f };
            AZ::Vector3 m_loopBoxDimensions{ 0.0f, 0.0f, 0.0f };    ///< Dimensions of loop box

            static void Reflect(AZ::ReflectContext* context);
        };

        class VolumetricProperties
        {
        public:
            AZ_TYPE_INFO(MovementProperties, "{93449B86-19AF-4750-AC99-32C42834F853}");
            VolumetricProperties() = default;

            MaterialAssetRef& GetMaterialAsset() { return m_material; }

            void SetEntityId(AZ::EntityId entityId) { m_parentEntityId = entityId; }
            AZ::EntityId GetEntityId() { return m_parentEntityId; }

            void OnIsVolumetricPropertyChanged();
            void OnMaterialAssetPropertyChanged();
            void OnDensityChanged();

            bool m_isVolumetricRenderingEnabled{ false };
            float m_density{ 1.0f };
            MaterialAssetRef m_material;
            AZ::EntityId m_parentEntityId;

            static void Reflect(AZ::ReflectContext* context);
        };

        CloudComponentRenderNode();
        ~CloudComponentRenderNode() override;

        // AZ::TransformNotificationBus::Handler interface implementation
        virtual void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

        // AZ::SystemTickBus interface implementation
        void OnSystemTick() override;

        // IRenderNode interface implementation
        EERType GetRenderNodeType() override { return eERType_Cloud; }
        const char* GetEntityClassName() const override { return "SkyCloud"; }
        const char* GetName() const override { return "SkyCloud"; }
        Vec3 GetPos(bool bWorldOnly = true) const override { return m_worldMatrix.GetTranslation(); }
        void Render(const struct SRendParams& inRenderParams, const struct SRenderingPassInfo& passInfo) override;
        const AABB GetBBox() const override;
        void SetBBox(const AABB& WSBBox) override;
        void FillBBox(AABB& aabb) override { aabb = GetBBox(); }
        void OffsetPosition(const Vec3& delta) override {}
        void SetMaterial(MaterialPtr pMat) override;
        MaterialPtr GetMaterial(Vec3* pHitPos = nullptr) override;
        MaterialPtr GetMaterialOverride() override { return GetMaterial(); }
        
        // CloudComponentBehaviorRequestBus::Handler
        bool GetAutoMove() override { return m_movementProperties.m_autoMove; }
        AZ::Vector3 GetVelocity() override { return m_movementProperties.m_speed; }
        float GetFadeDistance() override { return m_movementProperties.m_fadeDistance; }
        bool GetVolumetricRenderingEnabled() override { return m_volumeProperties.m_isVolumetricRenderingEnabled; }
        float GetVolumetricDensity() override { return m_volumeProperties.m_density; }
        void SetAutoMove(bool state) override { m_movementProperties.m_autoMove = state; }
        void SetVelocity(AZ::Vector3 velocity) override { m_movementProperties.m_speed = velocity; }
        void SetFadeDistance(float distance) override { m_movementProperties.m_fadeDistance = distance; }
        void SetVolumetricRenderingEnabled(bool enabled) override{ m_volumeProperties.m_isVolumetricRenderingEnabled = enabled; }
        void SetVolumetricDensity(float density) override { m_volumeProperties.m_density = density; Refresh(); }

        /**
         * Gets the maximum view distance before the cloud should fade out
         * @return Returns the maximum view distance
         */
        float GetMaxViewDist() override;

        /**
         * Report memory usage
         * @param sizer memory usage tracker
         */
        void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }

        /**
         * Notifies render node which entity owns it, for subscribing to transform bus, etc.
         * @param id of entity that this component is attached to.
         */
        void AttachToEntity(AZ::EntityId id);

        /**
         * Copy properties over when creating via component drop
         * @param rhs source render node
         */
        void CopyPropertiesTo(CloudComponentRenderNode& rhs) const;

        /**
         * Get the cloud particle data. This data contains the actual particle data for the cloud along
         * with the bounds and offset required to render the cloud
         * @return Returns the cloud particle data 
         */
        CloudParticleData& GetCloudParticleData() { return m_cloudParticleData; }

        /**
        * Sets the render node's world transform properties based on given transform.
        * @param entityTransform transform from entity
        */
        void SetWorldTransform(const AZ::Transform& entityTransform);

        /**
         * Updates the render node's world transform based on the entity.
         * @param entityTransform transform from entity
         */
        void UpdateWorldTransform(const AZ::Transform& entityTransform);

        /**
         * Refresh
         */
        void Refresh();

        /**
         * Reflection
         * @param context refelction context
         */
        static void Reflect(AZ::ReflectContext* context);

        /**
         * Sets dimensions in which thge cloud will be bound while moving
         * @param dimensions dimensions of loop box
         */
        void SetLoopCloudDimensions(const AZ::Vector3& dimensions) { m_movementProperties.m_loopBoxDimensions = dimensions; }

        /**
         * Callback for when the material asset changes
         */
        void OnMaterialAssetChanged();
        
        /**
         * Indicates if particles should be drawn as spheres for visualization
         * @return true if a sphere should be displayed for each cloud particle, false otherwise
         */
        bool IsDisplaySpheresEnabled() { return m_displayProperties.m_isDisplaySpheresEnabled; }

        /**
         * Indicates if volumes should be drawn
         * @return true if the bounding volume for each cloud area should be displayed, false otherwise
         */
        bool IsDisplayVolumesEnabled() { return m_displayProperties.m_isDisplayVolumesEnabled; }

        /**
         * Indicates if bounds should be drawn
         * @return true if the bounding volume for the entire cloud should be displayed, false otherwise
         */
        bool IsDisplayBoundsEnabled() { return m_displayProperties.m_isDisplayBoundsEnabled; }

    protected:

        class DisplayProperties
        {
        public:
            AZ_TYPE_INFO(MovementProperties, "{23BF41D0-9E7C-429C-B770-694589BE141A}");
            DisplayProperties() = default;

            bool m_isDisplaySpheresEnabled{ false };        ///< Turns on additional sphere rendering for each sprite generated.
            bool m_isDisplayVolumesEnabled{ false };        ///< Indicates if boxes are being rendered for debug / display purposes
            bool m_isDisplayBoundsEnabled{ false };         ///< Indicates if bounding volume of all particles should be displayed for debug / display purposes

            static void Reflect(AZ::ReflectContext* context);
        };

        bool IsVolumetricRenderingEnabled() { return m_volumeProperties.m_isVolumetricRenderingEnabled; }
        void OnGeneralPropertyChanged();

        void Update();
        void Move();

        // Members
        AZStd::string name;
        AZ::EntityId m_attachedToEntityId;

        bool m_isVisible{ true };
        bool m_isInsideLoopBox{ true };
        float m_alpha{ 1.0f };

        Matrix34 m_entityWorldMatrix;
        Matrix34 m_worldMatrix;
        
        AZStd::shared_ptr<ICloudVolume> m_cloudVolume{ nullptr };
        CloudParticleData m_cloudParticleData;
        MaterialAssetRef m_material;

        MovementProperties m_movementProperties;
        VolumetricProperties m_volumeProperties;
        DisplayProperties m_displayProperties;
    };
}