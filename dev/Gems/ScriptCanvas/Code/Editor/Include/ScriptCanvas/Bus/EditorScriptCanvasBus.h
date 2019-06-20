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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/std/string/string.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Vector2.h>

#include <GraphCanvas/Editor/EditorTypes.h>
#include <GraphCanvas/Types/EntitySaveData.h>

#include <Editor/Include/ScriptCanvas/Assets/ScriptCanvasAsset.h>
#include <Editor/Include/ScriptCanvas/Bus/NodeIdPair.h>
#include <ScriptCanvas/Core/ExecutionNotificationsBus.h>
#include <ScriptCanvas/Variable/VariableCore.h>

namespace GraphCanvas
{
    class GraphCanvasTreeItem;
}

namespace ScriptCanvasEditor
{
    //=========================================================================
    // EditorGraphBus
    //=========================================================================
    class EditorScriptCanvasRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;


        //! Sets the name of the ScriptCanvas Graph.
        //! \param name value to set
        virtual void SetName(const AZStd::string& name) = 0;

        //! Gets the name of the ScriptCanvas Graph.
        //! \return reference to Graph name
        virtual const AZStd::string& GetName() const = 0;

        //! Will open the graph in the editor.
        virtual void OpenEditor() = 0;

        //! Used to close a graph that is currently opened in the editor.
        virtual void CloseGraph() = 0;

        //! Retrieves script canvas asset reference
        virtual AZ::Data::Asset<ScriptCanvasAsset> GetAsset() const = 0;

        //! Returns the Entity ID of the Editor Entity that owns this graph.
        virtual AZ::EntityId GetEditorEntityId() const = 0;
        virtual AZ::NamedEntityId GetNamedEditorEntityId() const = 0;
    };

    using EditorScriptCanvasRequestBus = AZ::EBus<EditorScriptCanvasRequests>;

    // Above bus is keyed off of the graph Id. Which I don't really have access to.
    // This bus is here just so I can tell it to open the Editor.
    class EditorContextMenuRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        //! Gets the GraphId for the EditorScriptCanvasComponent on the given entity.
        virtual AZ::EntityId GetGraphId() const = 0;
    };

    using EditorContextMenuRequestBus = AZ::EBus<EditorContextMenuRequests>;

    class EditorScriptCanvasAssetNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::Data::AssetId;

        //! Notification which fires after an EditorGraph has received it's on AssetReady callback
        //! \param scriptCanvasAsset Script Canvas asset which is now ready for use in the Editor
        virtual void OnScriptCanvasAssetReady(const AZ::Data::Asset<ScriptCanvasAsset>& /*scriptCanvasAsset*/) {};

        //! Notification which fires after an EditorGraph has received it's on AssetReloaded callback
        //! \param scriptCanvasAsset Script Canvas asset which is now ready for use in the Editor
        virtual void OnScriptCanvasAssetReloaded(const AZ::Data::Asset<ScriptCanvasAsset>& /*scriptCanvaAsset */) {};

        //! Notification which fires after an EditorGraph has received it's on AssetReady callback
        //! \param AssetId AssetId of unloaded ScriptCanvas
        virtual void OnScriptCanvasAssetUnloaded(const AZ::Data::AssetId& /*assetId*/) {};

        //! Notification which fires after an EditorGraph has received an onAssetSaved callback
        //! \param scriptCanvasAsset Script Canvas asset which was attempted to be saved
        //! \param isSuccessful specified where the Script Canvas asset was successfully saved
        virtual void OnScriptCanvasAssetSaved(const AZ::Data::Asset<ScriptCanvasAsset>& /*scriptCanvasAsset*/, bool /*isSuccessful*/) {};
    };
    using EditorScriptCanvasAssetNotificationBus = AZ::EBus<EditorScriptCanvasAssetNotifications>;
    
    class EditorGraphRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void CreateGraphCanvasScene() = 0;
        virtual void ClearGraphCanvasScene() = 0;
        virtual GraphCanvas::GraphId GetGraphCanvasGraphId() const = 0;

        virtual void DisplayGraphCanvasScene() = 0;

        virtual void UpdateGraphCanvasSaveData(const AZStd::unordered_map< AZ::EntityId, GraphCanvas::EntitySaveDataContainer* >& saveData) = 0;
        virtual AZStd::unordered_map< AZ::EntityId, GraphCanvas::EntitySaveDataContainer* > GetGraphCanvasSaveData() = 0;

        virtual NodeIdPair CreateCustomNode(const AZ::Uuid& typeId, const AZ::Vector2& position) = 0;

        virtual void AddCrcCache(const AZ::Crc32& crcValue, const AZStd::string& cacheString) = 0;
        virtual void RemoveCrcCache(const AZ::Crc32& crcValue) = 0;
        virtual AZStd::string DecodeCrc(const AZ::Crc32& crcValue) = 0;

        virtual void ClearHighlights() = 0;
        virtual void HighlightMembersFromTreeItem(const GraphCanvas::GraphCanvasTreeItem* treeItem) = 0;
        virtual void HighlightVariables(const AZStd::unordered_set< ScriptCanvas::VariableId>& variableIds) = 0;
        virtual void HighlightNodes(const AZStd::vector<NodeIdPair>& nodes) = 0;

        virtual AZStd::vector<NodeIdPair> GetNodesOfType(const ScriptCanvas::NodeTypeIdentifier&) = 0;
        virtual AZStd::vector<NodeIdPair> GetVariableNodes(const ScriptCanvas::VariableId&) = 0;        

        virtual void RemoveUnusedVariables() = 0;

        virtual void QueueVersionUpdate(const AZ::EntityId& graphCanvasNodeId) = 0;
    };
    
    using EditorGraphRequestBus = AZ::EBus<EditorGraphRequests>;

    class EditorGraphNotifications
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void OnGraphCanvasSceneDisplayed() {};
    };

    using EditorGraphNotificationBus = AZ::EBus<EditorGraphNotifications>;

    class EditorNodeNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void OnGraphCanvasNodeDisplayed(AZ::EntityId /*graphCanvasNodeId*/) {}
    };

    using EditorNodeNotificationBus = AZ::EBus<EditorNodeNotifications>;

    // Mainly expected to be used from an aggregator.
    class EditorScriptCanvasComponentLogging 
        : public AZ::ComponentBus
    {
    public:
        
        virtual AZ::NamedEntityId FindNamedEntityId() const = 0;
        virtual ScriptCanvas::GraphIdentifier GetGraphIdentifier() const = 0;
    };

    using EditorScriptCanvasComponentLoggingBus = AZ::EBus<EditorScriptCanvasComponentLogging>;

    class EditorLoggingComponentNotifications
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        virtual void OnEditorScriptCanvasComponentActivated(const AZ::NamedEntityId& namedEntityId, const ScriptCanvas::GraphIdentifier& graphIdentifier) = 0;
        virtual void OnEditorScriptCanvasComponentDeactivated(const AZ::NamedEntityId& namedEntityId, const ScriptCanvas::GraphIdentifier& graphIdentifier) = 0;
        virtual void OnAssetSwitched(const AZ::NamedEntityId& namedEntityId, const ScriptCanvas::GraphIdentifier& newGraphIdentifier, const ScriptCanvas::GraphIdentifier& oldGraphIdentifier) = 0;
    };

    using EditorLoggingComponentNotificationBus = AZ::EBus<EditorLoggingComponentNotifications>;
}
