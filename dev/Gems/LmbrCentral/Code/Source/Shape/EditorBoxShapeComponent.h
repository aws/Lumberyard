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

#include "BoxShape.h"
#include "EditorBaseShapeComponent.h"
#include <AzCore/std/containers/array.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Manipulators/LinearManipulator.h>
#include <AzToolsFramework/Manipulators/BoxManipulators.h>

namespace LmbrCentral
{
    class EditorBoxShapeComponent
        : public EditorBaseShapeComponent
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AzToolsFramework::EntitySelectionEvents::Bus::Handler
        , private AzToolsFramework::BoxManipulatorHandler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorBoxShapeComponent, EditorBoxShapeComponentTypeId, EditorBaseShapeComponent);
        static void Reflect(AZ::ReflectContext* context);

        EditorBoxShapeComponent() = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

    protected:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            EditorBaseShapeComponent::GetProvidedServices(provided);
            provided.push_back(AZ_CRC("BoxShapeService", 0x946a0032));
        }

        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        AZ_DISABLE_COPY_MOVE(EditorBoxShapeComponent)
        
        /// AzToolsFramework::EntitySelectionEvents::Bus::Handler
        void OnSelected() override;
        void OnDeselected() override;

        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntity(bool& handled) override;

        // BoxManipulatorHandler
        AZ::Vector3 GetDimensions() override;
        void SetDimensions(const AZ::Vector3& dimensions) override;
        AZ::Transform GetCurrentTransform() override;

        void OnMouseMoveManipulator(
            const AzToolsFramework::LinearManipulator::Action& action);

        // AZ::TransformNotificationBus::Handler
        void OnTransformChanged(
            const AZ::Transform& /*local*/, const AZ::Transform& /*world*/);

        void ConfigurationChanged();

        BoxShape m_boxShape; ///< Stores underlying box representation for this component.
        AzToolsFramework::BoxManipulator m_boxManipulator; ///< Manipulator for editing box size.
    };
} // namespace LmbrCentral
