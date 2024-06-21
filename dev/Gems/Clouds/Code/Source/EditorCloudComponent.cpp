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


#include <QMessageBox>
#include <QApplication>
#include <QFileDialog>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Asset/AssetManager.h>
#include <LmbrCentral/Shape/BoxShapeComponentBus.h>
#include <LmbrCentral/Shape/EditorShapeComponentBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>
#include <AzToolsFramework/UI/UICore/ProgressShield.hxx>

#include <IAISystem.h>

#include <MathConversion.h>
#include <Random.h>

#include "EditorCloudComponent.h"
#include "CloudGenerator.h"
#include "CloudParticleData.h"

namespace CloudsGem
{
    void EditorCloudComponent::Reflect(AZ::ReflectContext* context)
    {
        CloudGenerator::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<EditorCloudComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("RenderNode", &EditorCloudComponent::m_renderNode)
                ->Field("Generation", &EditorCloudComponent::m_cloudGenerator);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                using namespace AZ::Edit::Attributes;
                using namespace AZ::Edit::UIHandlers;
                using namespace AZ::Edit::ClassElements;

                using VolumetricProps = CloudComponentRenderNode::VolumetricProperties;
                editContext->Class<VolumetricProps>("Volumetric properties", "Volumetric properties.")
                    ->ClassElement(EditorData, "")
                        ->Attribute(AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AutoExpand, true)
                        ->Attribute(Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
                    ->ClassElement(Group, "Volumetric Rendering")
                        ->Attribute(AutoExpand, true)
                        ->DataElement(Default, &VolumetricProps::m_isVolumetricRenderingEnabled, "Enabled", "Check to use voxelized volumetric rendering")
                            ->Attribute(ChangeNotify, &VolumetricProps::OnIsVolumetricPropertyChanged)
                        ->DataElement(Default, &VolumetricProps::m_material, "Volume material", "Material for volumetric cloud rendering.")
                            ->Attribute(ChangeNotify, &VolumetricProps::OnMaterialAssetPropertyChanged)
                        ->DataElement(Slider, &VolumetricProps::m_density, "Density", "Defines the cloud density")
                            ->Attribute(Min, 0.f)
                            ->Attribute(Max, 1.f)
                            ->Attribute(Step, 0.1f)
                            ->Attribute(ChangeNotify, &VolumetricProps::OnDensityChanged);

                using MoveProps = CloudComponentRenderNode::MovementProperties;
                editContext->Class<MoveProps>("Movement Properties", "Cloud movement properties.")
                    ->ClassElement(EditorData, "")
                        ->Attribute(AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AutoExpand, true)
                        ->Attribute(Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
                    ->ClassElement(Group, "Movement")
                        ->Attribute(AutoExpand, true)
                        ->DataElement(Default, &MoveProps::m_autoMove, "AutoMove", "If checked the cloud moves automatically")
                        ->DataElement(Default, &MoveProps::m_speed, "Velocity", "Cloud speed and direction")
                        ->DataElement(Slider, &MoveProps::m_fadeDistance, "FadeDistance", "Fade distance")
                            ->Attribute(Min, 0.f)
                            ->Attribute(Max, 1000.f)
                            ->Attribute(Step, 0.1f);

                using DisplayProps = CloudComponentRenderNode::DisplayProperties;
                editContext->Class<DisplayProps>("Display Properties", "Display properties.")
                    ->ClassElement(EditorData, "")
                        ->Attribute(AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AutoExpand, true)
                        ->Attribute(Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
                    ->ClassElement(Group, "Display")
                        ->Attribute(AutoExpand, true)
                        ->DataElement(Default, &DisplayProps::m_isDisplaySpheresEnabled, "Display Spheres", "Turns on additional sphere rendering for each sprite generated")
                        ->DataElement(Default, &DisplayProps::m_isDisplayVolumesEnabled, "Display Volumes", "Turns on additional rendering for displaying volumes attached to cloud")
                        ->DataElement(Default, &DisplayProps::m_isDisplayBoundsEnabled, "Display Bounds", "Turns on additional rendering for displaying bounds of all particles attached to cloud");

                using GenProps = CloudGenerator;
                editContext->Class<GenProps>("Generation Properties", "Cloud generation properties.")
                    ->ClassElement(EditorData, "")
                        ->Attribute(AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AutoExpand, true)
                        ->Attribute(Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
                    ->ClassElement(Group, "Generation")
                        ->Attribute(AutoExpand, true)
                        ->DataElement(Crc, &GenProps::m_seed, "Seed", "Seed for random number generator. This value controls random number generation")
                            ->Attribute(ChangeNotify, &GenProps::OnSeedChanged)
                        ->DataElement(Slider, &GenProps::m_rows, "Rows", "Number of rows in cloud texture. Leave at 4 when using the default texture")
                            ->Attribute(Min, 1)
                            ->Attribute(Max, 64)
                            ->Attribute(Step, 1)
                            ->Attribute(ChangeNotify, &GenProps::OnAtlasRowsChanged)
                        ->DataElement(Slider, &GenProps::m_columns, "Columns", "Number of columns in cloud texture atlas. Leave at 4 when using the default texture")
                            ->Attribute(Min, 1)
                            ->Attribute(Max, 64)
                            ->Attribute(Step, 1)
                            ->Attribute(ChangeNotify, &GenProps::OnAtlasColumnsChanged)
                        ->DataElement(Slider, &GenProps::m_spriteCount, "Sprites", "Sets the number of sprites to be generated in the cloud")
                            ->Attribute(Min, 0)
                            ->Attribute(Max, 4096)
                            ->Attribute(Step, 1)
                            ->Attribute(ChangeNotify, &GenProps::OnSpriteCountChanged)
                        ->DataElement(Slider, &GenProps::m_spriteRow, "Render Row", "Designates the row in the cloud texture for rendering")
                            ->Attribute(Min, 0)
                            ->Attribute(Max, &GenProps::GetMaxSelectableRow)
                            ->Attribute(Step, 1)
                            ->Attribute(ChangeNotify, &GenProps::OnSelectedAtlasRowChanged)
                        ->DataElement(Slider, &GenProps::m_spriteSize, "Scale", "Sets the scale of the sprites in the cloud")
                            ->Attribute(Min, 0.1f)
                            ->Attribute(Max, 1000.f)
                            ->Attribute(ChangeNotify, &GenProps::OnSizeChanged)
                        ->DataElement(Slider, &GenProps::m_sizeVariation, "Size Variation", "Defines the randomization in size of the sprites within the cloud")
                            ->Attribute(Min, 0.0f)
                            ->Attribute(Max, 2.0f)
                            ->Attribute(Step, 0.1f)
                            ->Attribute(ChangeNotify, &GenProps::OnSizeVarianceChanged)
                        ->DataElement(Slider, &GenProps::m_minDistance, "Min Distance", "Defines the minimum distance between the generated sprites within the cloud")
                            ->Attribute(Min, 0.1f)
                            ->Attribute(Max, 250.0f)
                            ->Attribute(ChangeNotify, &GenProps::OnMinimumDistanceChanged)
                        ->DataElement(Default, &GenProps::m_fillByVolume, "Fill By Volume", "Fills cloud boxes based on volume")
                            ->Attribute(ChangeNotify, &GenProps::OnPropertiesChanged)
                        ->DataElement(Default, &GenProps::m_fillByLoopbox, "Fill By Loopbox", "Fills the loopbox volume instead of child entities")
                            ->Attribute(ChangeNotify, &GenProps::OnPropertiesChanged)
                        ->DataElement(Button, &GenProps::m_generateButton, "", "Create a cloud from the generation properties")
                            ->Attribute(ButtonText, "Generate")
                            ->Attribute(ChangeNotify, &GenProps::OnGenerateButtonPressed);
                
                editContext->Class<CloudComponentRenderNode>("Rendering", "Attach geometry to the entity.")
                    ->ClassElement(EditorData, "")
                        ->Attribute(AutoExpand, true)
                        ->Attribute(Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
                    ->ClassElement(Group, "")
                        ->Attribute(AutoExpand, true)
                        ->DataElement(Default, &CloudComponentRenderNode::m_material, "Cloud material", "Material for simple sprite cloud rendering.")
                            ->Attribute(ChangeNotify, &CloudComponentRenderNode::OnMaterialAssetChanged)
                        ->DataElement(Default, &CloudComponentRenderNode::m_volumeProperties, "Volumetric", "Volumetric properties")
                            ->Attribute(ChangeNotify, &CloudComponentRenderNode::OnGeneralPropertyChanged)
                        ->DataElement(Default, &CloudComponentRenderNode::m_movementProperties, "Movement", "Cloud movement properties")
                            ->Attribute(ChangeNotify, &CloudComponentRenderNode::OnGeneralPropertyChanged)
                        ->DataElement(Default, &CloudComponentRenderNode::m_displayProperties, "Display", "Displayproperties")
                            ->Attribute(ChangeNotify, &CloudComponentRenderNode::OnGeneralPropertyChanged);

               editContext->Class<EditorCloudComponent>("Sky Cloud", "Attach clouds to the entity.")
                    ->ClassElement(EditorData, "")
                        ->Attribute(Category, "Environment")
                        ->Attribute(Icon, "Editor/Icons/Components/SkyClouds.svg")
                        ->Attribute(ViewportIcon, "Editor/Icons/Components/Viewport/SkyClouds.png")
                        ->Attribute(PreferNoViewportIcon, true)
                        ->Attribute(AutoExpand, true)
                        ->Attribute(AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(HelpPageURL, "http://docs.aws.amazon.com/console/lumberyard/userguide/sky-cloud-component")
                        ->DataElement(Default, &EditorCloudComponent::m_renderNode)
                        ->DataElement(Default, &EditorCloudComponent::m_cloudGenerator);
            }
        }

        AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext)
        {
            behaviorContext->Class<EditorCloudComponent>()->RequestBus("CloudComponentBehaviorRequestBus");
        }
    }

    void EditorCloudComponent::Activate()
    {
        EditorComponentBase::Activate();

        m_renderNode.AttachToEntity(GetEntityId());
        m_cloudGenerator.Initialize(m_renderNode.GetCloudParticleData());

        const AZ::EntityId& entityId = GetEntityId();
        EditorCloudComponentRequestBus::Handler::BusConnect(entityId);
        LmbrCentral::RenderNodeRequestBus::Handler::BusConnect(entityId);
        LmbrCentral::ShapeComponentNotificationsBus::Handler::BusConnect(entityId);
        AZ::EntityBus::Handler::BusConnect(entityId);
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusConnect(entityId);
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(entityId);
    }

    void EditorCloudComponent::Deactivate()
    {
        LmbrCentral::RenderNodeRequestBus::Handler::BusDisconnect();
        LmbrCentral::ShapeComponentNotificationsBus::Handler::BusDisconnect();
        AZ::EntityBus::Handler::BusDisconnect();
        AzToolsFramework::EditorVisibilityNotificationBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        EditorCloudComponentRequestBus::Handler::BusDisconnect();

        m_renderNode.AttachToEntity(AZ::EntityId());
        m_cloudGenerator.Destroy();

        EditorComponentBase::Deactivate();
    }

    void EditorCloudComponent::OnEntityActivated(const AZ::EntityId& entityId)
    {
        if (entityId == GetEntityId())
        {
            // Reduce the alpha on the cloud loop box to make the cloud inside more visible
            static const AZ::Vector4 shapeColor(1.00f, 1.00f, 0.78f, 0.1f);
            EBUS_EVENT_ID(GetEntityId(), LmbrCentral::EditorShapeComponentRequestsBus, SetShapeColor, shapeColor);

            OnShapeChanged(ShapeComponentNotifications::ShapeChangeReasons::ShapeChanged);
        }
    }

    void EditorCloudComponent::OnShapeChanged(LmbrCentral::ShapeComponentNotifications::ShapeChangeReasons changeReason)
    {
        // Callback to handle loop box shape changes
        if (changeReason == ShapeComponentNotifications::ShapeChangeReasons::ShapeChanged)
        {
            // Update loop cloud dimension settings based on the box attached to this entity
            LmbrCentral::BoxShapeConfiguration boxShapeConfiguration;
            EBUS_EVENT_ID_RESULT(boxShapeConfiguration, GetEntityId(), LmbrCentral::BoxShapeComponentRequestsBus, GetBoxConfiguration);
            m_renderNode.SetLoopCloudDimensions(boxShapeConfiguration.m_dimensions);
        }
    }

    void EditorCloudComponent::DisplayEntityViewport(
        const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        if (IsSelected())
        {
            CloudParticleData& cloudData = m_renderNode.GetCloudParticleData();

            debugDisplay.PushMatrix(
                LYTransformToAZTransform(Matrix34::CreateTranslationMat(m_renderNode.GetPos())));
            debugDisplay.DepthTestOff();

            if (m_renderNode.IsDisplaySpheresEnabled())
            {
                if (cloudData.HasParticles())
                {
                    Vec3 offset = cloudData.GetOffset();
                    for (auto& particle : cloudData.GetParticles())
                    {
                        debugDisplay.SetColor(0.3f, 0.3f, 0.8f, 0.05f);
                        debugDisplay.DrawBall(LYVec3ToAZVec3(particle.GetPosition() - offset), particle.GetRadius());
                    }
                }
            }

            if (m_renderNode.IsDisplayBoundsEnabled())
            {
                AABB bounds = cloudData.GetBounds();
                debugDisplay.SetColor(0.4f, 0.6f, 0.8f, 0.3f);
                debugDisplay.DrawSolidBox(LYVec3ToAZVec3(bounds.min), LYVec3ToAZVec3(bounds.max));
            }

            if (m_renderNode.IsDisplayVolumesEnabled())
            {
                AZStd::vector<AZ::EntityId> children;
                EBUS_EVENT_ID_RESULT(children, GetEntityId(), AZ::TransformBus, GetChildren);
                for (uint32 i = 0; i < children.size(); ++i)
                {
                    // Get translation from child
                    AZ::Vector3 translation{ 0.0f, 0.0f, 0.0f };
                    EBUS_EVENT_ID_RESULT(translation, children[i], AZ::TransformBus, GetLocalTranslation);

                    // Get dimensions of box
                    LmbrCentral::BoxShapeConfiguration boxShapeConfiguration;
                    boxShapeConfiguration.m_dimensions = AZ::Vector3::CreateZero();
                    EBUS_EVENT_ID_RESULT(boxShapeConfiguration, children[i], LmbrCentral::BoxShapeComponentRequestsBus, GetBoxConfiguration);

                    const AZ::Vector3 dimensions = boxShapeConfiguration.m_dimensions * 0.5f;

                    debugDisplay.SetColor(1.0f, 1.0f, 0.8f, 0.2f);
                    debugDisplay.DrawSolidBox(translation - dimensions, translation + dimensions);
                }
            }

            debugDisplay.DepthTestOn();
            debugDisplay.PopMatrix();
        }
    }

    void EditorCloudComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        if (CloudComponent* cloudComponent = gameEntity->CreateComponent<CloudComponent>())
        {
            m_renderNode.CopyPropertiesTo(cloudComponent->m_renderNode);
        }
    }

    void EditorCloudComponent::Generate(GenerationFlag flag)
    {
        m_cloudGenerator.Generate(m_renderNode.GetCloudParticleData(), GetEntityId(), flag);
        Refresh();
    }

    void EditorCloudComponent::OnMaterialAssetChanged()
    {
        m_renderNode.OnMaterialAssetChanged();
    }

    void EditorCloudComponent::Refresh()
    {
        m_renderNode.Refresh();
    }
}