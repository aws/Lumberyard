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

#include "LmbrCentral_precompiled.h"
#include "EditorSplineComponent.h"

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/Entity/EditorEntityInfoBus.h>
#include <AzToolsFramework/Manipulators/ManipulatorView.h>

namespace LmbrCentral
{
    SplineHoverSelection::SplineHoverSelection() {}

    SplineHoverSelection::~SplineHoverSelection() {}

    void SplineHoverSelection::Create(AZ::EntityId entityId, AzToolsFramework::ManipulatorManagerId managerId)
    {
        m_splineSelectionManipulator = AZStd::make_unique<AzToolsFramework::SplineSelectionManipulator>(entityId);
        m_splineSelectionManipulator->Register(managerId);

        if (const AZStd::shared_ptr<const AZ::Spline> spline = m_spline.lock())
        {
            const float splineWidth = 0.05f;
            m_splineSelectionManipulator->SetSpline(spline);
            m_splineSelectionManipulator->SetView(CreateManipulatorViewSplineSelect(
                *m_splineSelectionManipulator, AZ::Color(0.0f, 1.0f, 0.0f, 1.0f), splineWidth));
        }

        m_splineSelectionManipulator->InstallLeftMouseUpCallback([this](
            const AzToolsFramework::SplineSelectionManipulator::Action& action)
        {
            if (AZStd::shared_ptr<AZ::Spline> spline = m_spline.lock())
            {
                // wrap vertex container in variable vertices interface
                AzToolsFramework::VariableVerticesVertexContainer<AZ::Vector3> vertices(spline->m_vertexContainer);
                AzToolsFramework::InsertVertex(vertices,
                    action.m_splineAddress.m_segmentIndex,
                    action.m_localSplineHitPosition);
            }
        });
    }

    void SplineHoverSelection::Destroy()
    {
        if (m_splineSelectionManipulator)
        {
            m_splineSelectionManipulator->Unregister();
            m_splineSelectionManipulator.reset();
        }
    }

    void SplineHoverSelection::Register(AzToolsFramework::ManipulatorManagerId managerId)
    {
        if (m_splineSelectionManipulator)
        {
            m_splineSelectionManipulator->Register(managerId);
        }
    }

    void SplineHoverSelection::Unregister()
    {
        if (m_splineSelectionManipulator)
        {
            m_splineSelectionManipulator->Unregister();
        }
    }

    void SplineHoverSelection::SetBoundsDirty()
    {
        if (m_splineSelectionManipulator)
        {
            m_splineSelectionManipulator->SetBoundsDirty();
        }
    }

    void SplineHoverSelection::Refresh()
    {
        SetBoundsDirty();
    }

    static const float s_controlPointSize = 0.1f;
    static const AZ::Vector4 s_splineColor = AZ::Vector4(1.0f, 1.0f, 0.78f, 0.5f);

    void EditorSplineComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorSplineComponent, EditorComponentBase>()
                ->Version(1)
                ->Field("Configuration", &EditorSplineComponent::m_splineCommon);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorSplineComponent>(
                    "Spline", "Defines a sequence of points that can be interpolated.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Shape")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/Spline.png")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/Spline.png")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AZ::Edit::Attributes::HelpPageURL, "http://docs.aws.amazon.com/console/lumberyard/userguide/spline-component")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorSplineComponent::m_splineCommon, "Configuration", "Spline Configuration")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void EditorSplineComponent::Activate()
    {
        EditorComponentBase::Activate();

        const AZ::EntityId entityId = GetEntityId();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(entityId);
        EntitySelectionEvents::Bus::Handler::BusConnect(entityId);
        SplineComponentRequestBus::Handler::BusConnect(entityId);
        AZ::TransformNotificationBus::Handler::BusConnect(entityId);
        ToolsApplicationEvents::Bus::Handler::BusConnect();

        bool selected = false;
        AzToolsFramework::EditorEntityInfoRequestBus::EventResult(
            selected, GetEntityId(), &AzToolsFramework::EditorEntityInfoRequestBus::Events::IsSelected);

        // placeholder - create initial spline if empty
        AZ::VertexContainer<AZ::Vector3>& vertexContainer = m_splineCommon.m_spline->m_vertexContainer;
        if (selected && vertexContainer.Empty())
        {
            vertexContainer.AddVertex(AZ::Vector3(-3.0f, 0.0f, 0.0f));
            vertexContainer.AddVertex(AZ::Vector3(-1.0f, 0.0f, 0.0f));
            vertexContainer.AddVertex(AZ::Vector3(1.0f, 0.0f, 0.0f));
            vertexContainer.AddVertex(AZ::Vector3(3.0f, 0.0f, 0.0f));
            CreateManipulators();
        }

        auto containerChanged = [this]()
        {
            // destroy and recreate manipulators when container is modified (vertices are added or removed)
            m_vertexSelection.Destroy();
            CreateManipulators();
            SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
        };

        auto elementChanged = [this]()
        {
            SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
            m_vertexSelection.Refresh();
        };

        auto vertexAdded = [this, containerChanged](size_t index)
        {
            containerChanged();

            AzToolsFramework::ManipulatorManagerId managerId = AzToolsFramework::ManipulatorManagerId(1);
            m_vertexSelection.CreateTranslationManipulator(GetEntityId(), managerId,
                AzToolsFramework::TranslationManipulator::Dimensions::Three,
                m_splineCommon.m_spline->m_vertexContainer.GetVertices()[index], index,
                AzToolsFramework::ConfigureTranslationManipulatorAppearance3d);
        };

        m_splineCommon.SetCallbacks(
            vertexAdded,
            [containerChanged](size_t) { containerChanged(); },
            elementChanged,
            containerChanged,
            containerChanged,
            [this]() {
                OnChangeSplineType();
            });
    }

    void EditorSplineComponent::Deactivate()
    {
        m_vertexSelection.Destroy();

        EditorComponentBase::Deactivate();

        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        EntitySelectionEvents::Bus::Handler::BusDisconnect();
        SplineComponentRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        ToolsApplicationEvents::Bus::Handler::BusConnect();
    }

    void EditorSplineComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        if (auto component = gameEntity->CreateComponent<SplineComponent>())
        {
            component->m_splineCommon = m_splineCommon;
        }
    }

    static void DrawSpline(const AZ::Spline& spline, size_t begin, size_t end, AzFramework::EntityDebugDisplayRequests* displayContext)
    {
        const size_t granularity = spline.GetSegmentGranularity();

        for (size_t i = begin; i < end; ++i)
        {
            AZ::Vector3 p1 = spline.GetVertex(i - 1);
            for (size_t j = 1; j <= granularity; ++j)
            {
                AZ::Vector3 p2 = spline.GetPosition(AZ::SplineAddress(i - 1, j / static_cast<float>(granularity)));
                displayContext->DrawLine(p1, p2);
                p1 = p2;
            }
        }
    }

    void EditorSplineComponent::DisplayEntity(bool& handled)
    {
        if (!IsSelected())
        {
            return;
        }

        handled = true;

        AzFramework::EntityDebugDisplayRequests* displayContext = AzFramework::EntityDebugDisplayRequestBus::FindFirstHandler();
        AZ_Assert(displayContext, "Invalid display context.");

        const AZ::Spline* spline = m_splineCommon.m_spline.get();
        const size_t vertexCount = spline->GetVertexCount();
        if (vertexCount == 0)
        {
            return;
        }

        displayContext->PushMatrix(GetWorldTM());

        displayContext->SetColor(s_splineColor);

        // render spline
        if (spline->RTTI_IsTypeOf(AZ::LinearSpline::RTTI_Type()) || spline->RTTI_IsTypeOf(AZ::BezierSpline::RTTI_Type()))
        {
            DrawSpline(*spline, 1, spline->IsClosed() ? vertexCount + 1 : vertexCount, displayContext);
        }
        else if (spline->RTTI_IsTypeOf(AZ::CatmullRomSpline::RTTI_Type()))
        {
            // catmull-rom splines use the first and last points as control points only, omit those for display
            DrawSpline(*spline, spline->IsClosed() ? 1 : 2, spline->IsClosed() ? vertexCount + 1 : vertexCount - 1, displayContext);
        }

        displayContext->PopMatrix();
    }

    void EditorSplineComponent::OnSelected()
    {
        // ensure any maniulators are destroyed before recreated - (for undo/redo)
        m_vertexSelection.Destroy();
        CreateManipulators();
    }

    void EditorSplineComponent::OnDeselected()
    {
        m_vertexSelection.Destroy();
    }

    void EditorSplineComponent::OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& /*world*/)
    {
        // refresh all manipulator bounds when entity moves
        m_vertexSelection.SetBoundsDirty();
    }

    void EditorSplineComponent::CreateManipulators()
    {
        // if we have no vertices, do not attempt to create any manipulators
        if (m_splineCommon.m_spline->m_vertexContainer.Empty())
        {
            return;
        }

        AZStd::unique_ptr<SplineHoverSelection> splineHoverSelection =
            AZStd::make_unique<SplineHoverSelection>();
        splineHoverSelection->m_spline = m_splineCommon.m_spline;
        m_vertexSelection.m_hoverSelection = AZStd::move(splineHoverSelection);

        // create interface wrapping internal vertex container for use by vertex selection
        m_vertexSelection.m_vertices =
            AZStd::make_unique<AzToolsFramework::VariableVerticesVertexContainer<AZ::Vector3>>(
                m_splineCommon.m_spline->m_vertexContainer);

        const AzToolsFramework::ManipulatorManagerId managerId = AzToolsFramework::ManipulatorManagerId(1);
        m_vertexSelection.Create(GetEntityId(), managerId,
            AzToolsFramework::TranslationManipulator::Dimensions::Three,
            AzToolsFramework::ConfigureTranslationManipulatorAppearance3d);
    }

    void EditorSplineComponent::AfterUndoRedo()
    {
        bool selected;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(
            selected, &AzToolsFramework::ToolsApplicationRequests::IsSelected, GetEntityId());
        if (selected)
        {
            m_vertexSelection.Destroy();
            CreateManipulators();
        }
    }

    void EditorSplineComponent::OnChangeSplineType()
    {
        m_vertexSelection.Destroy();
        CreateManipulators();

        SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
    }

    AZ::ConstSplinePtr EditorSplineComponent::GetSpline()
    {
        return m_splineCommon.m_spline;
    }

    void EditorSplineComponent::ChangeSplineType(AZ::u64 splineType)
    {
        m_splineCommon.ChangeSplineType(splineType);
    }

    void EditorSplineComponent::SetClosed(bool closed)
    {
        m_splineCommon.m_spline->SetClosed(closed);
        SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
    }

    bool EditorSplineComponent::GetVertex(size_t index, AZ::Vector3& vertex) const
    {
        return m_splineCommon.m_spline->m_vertexContainer.GetVertex(index, vertex);
    }

    bool EditorSplineComponent::UpdateVertex(size_t index, const AZ::Vector3& vertex)
    {
        if (m_splineCommon.m_spline->m_vertexContainer.UpdateVertex(index, vertex))
        {
            SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
            return true;
        }

        return false;
    }

    void EditorSplineComponent::AddVertex(const AZ::Vector3& vertex)
    {
        m_splineCommon.m_spline->m_vertexContainer.AddVertex(vertex);
        SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
    }

    bool EditorSplineComponent::InsertVertex(size_t index, const AZ::Vector3& vertex)
    {
        if (m_splineCommon.m_spline->m_vertexContainer.InsertVertex(index, vertex))
        {
            SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
            return true;
        }

        return false;
    }

    bool EditorSplineComponent::RemoveVertex(size_t index)
    {
        if (m_splineCommon.m_spline->m_vertexContainer.RemoveVertex(index))
        {
            SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
            return true;
        }

        return false;
    }

    void EditorSplineComponent::SetVertices(const AZStd::vector<AZ::Vector3>& vertices)
    {
        m_splineCommon.m_spline->m_vertexContainer.SetVertices(vertices);
        SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
    }

    void EditorSplineComponent::ClearVertices()
    {
        m_splineCommon.m_spline->m_vertexContainer.Clear();
        SplineComponentNotificationBus::Event(GetEntityId(), &SplineComponentNotificationBus::Events::OnSplineChanged);
    }

    size_t EditorSplineComponent::Size() const
    {
        return m_splineCommon.m_spline->m_vertexContainer.Size();
    }

    bool EditorSplineComponent::Empty() const
    {
        return m_splineCommon.m_spline->m_vertexContainer.Empty();
    }
} // namespace LmbrCentral