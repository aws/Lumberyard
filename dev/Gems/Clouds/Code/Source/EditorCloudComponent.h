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

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TransformBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include <LmbrCentral/Rendering/RenderNodeBus.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>

#include "CloudComponentBus.h"
#include "CloudComponent.h"
#include "CloudGenerator.h"

namespace CloudsGem
{
    /*! In-editor cloud component. */
    class EditorCloudComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , private EditorCloudComponentRequestBus::Handler
        , private LmbrCentral::RenderNodeRequestBus::Handler
        , private LmbrCentral::ShapeComponentNotificationsBus::Handler
        , private AZ::EntityBus::Handler
        , private AzToolsFramework::EntitySelectionEvents::Bus::Handler
        , private AzToolsFramework::EditorVisibilityNotificationBus::Handler
        , private AzFramework::EntityDebugDisplayEventBus::Handler
    {
    public:
        AZ_COMPONENT(EditorCloudComponent, "{F9C98E6D-AC91-4827-AAFB-84D56AC1FB2A}", AzToolsFramework::Components::EditorComponentBase);

        // Destructor
        EditorCloudComponent() {}
        virtual ~EditorCloudComponent() {}

        /////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        /////////////////////////////////////////////////////////
        void Activate() override;
        void Deactivate() override;

        /////////////////////////////////////////////////////////
        // RenderNodeRequestBus::Handler
        /////////////////////////////////////////////////////////
        IRenderNode* GetRenderNode() override { return &m_renderNode; }
        float GetRenderNodeRequestBusOrder() const override { return 100.f; }

        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        /////////////////////////////////////////////////////////
        // EditorComponentBase
        /////////////////////////////////////////////////////////
        void BuildGameEntity(AZ::Entity* gameEntity) override;

        /////////////////////////////////////////////////////////
        // EntityBus::MultiHandler
        /////////////////////////////////////////////////////////
        void OnEntityActivated(const AZ::EntityId&) override;
        
        /////////////////////////////////////////////////////////
        // EditorCloudComponentRequestBus::Handler
        /////////////////////////////////////////////////////////
        void OnShapeChanged(ShapeChangeReasons changeReason) override;
        void Generate(GenerationFlag flag) override;
        void Refresh() override;
        void OnMaterialAssetChanged() override;

        /////////////////////////////////////////////////////////
        // EditorVisibilityNotificationBus
        /////////////////////////////////////////////////////////
        void OnEntityVisibilityChanged(bool visibility) override;

        /** 
         * Fills out the provided array with services provided by this component
         */
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("SkyCloudService", 0x89fcf917));
        }

        /**
         * Fills out the provided array with services required by this component
         */
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
            required.push_back(AZ_CRC("BoxShapeService", 0x946a0032));
        }

        /**
         * Fills out the provided array with services this component is dependent upon
         */
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("EditorVisibilityService", 0x90888caf));
            dependent.push_back(AZ_CRC("BoxShapeService", 0x946a0032));
        }

        /**
         * Fills out the provided array with services that are incompatible by this component
         */
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("SkyCloudService", 0x89fcf917));
        }

        /**
         * Performs reflection for this component
         * @param context reflection context
         */
        static void Reflect(AZ::ReflectContext* context);

    protected:

        CloudComponentRenderNode m_renderNode;      ///< IRender node implementation
        CloudGenerator m_cloudGenerator;            ///< Controls how the cloud is generated
    };
} // namespace CloudsGem
