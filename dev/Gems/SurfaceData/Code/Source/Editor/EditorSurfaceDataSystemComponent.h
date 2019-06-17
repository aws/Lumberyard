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

#include "EditorSurfaceTagListAsset.h"
#include <AzCore/Component/Component.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <SurfaceData/SurfaceDataTagProviderRequestBus.h>

namespace SurfaceData
{
    class EditorSurfaceDataSystemConfig
        : public AZ::ComponentConfig
    {
    public:
        AZ_CLASS_ALLOCATOR(EditorSurfaceDataSystemConfig, AZ::SystemAllocator, 0);
        AZ_RTTI(EditorSurfaceDataSystemConfig, "{13B511DF-B649-474C-AC32-1E1026DBB303}", AZ::ComponentConfig);
        static void Reflect(AZ::ReflectContext* context);
    };

    class EditorSurfaceDataSystemComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzFramework::AssetCatalogEventBus::Handler
        , private SurfaceDataTagProviderRequestBus::Handler
        , private AZ::Data::AssetBus::MultiHandler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorSurfaceDataSystemComponent, "{F3EE5137-856B-4E29-AADD-84F358AEA75F}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

    private:

        void AddAsset(AZ::Data::Asset<AZ::Data::AssetData>& asset);

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        ////////////////////////////////////////////////////////////////////////
        // SurfaceDataTagProviderRequestBus
        void GetRegisteredSurfaceTagNames(SurfaceTagNameSet& names) const override;

        ////////////////////////////////////////////////////////////////////////
        // AzFramework::AssetCatalogEventBus
        void OnCatalogLoaded(const char* /*catalogFile*/) override;
        void OnCatalogAssetChanged(const AZ::Data::AssetId& assetId) override;
        void OnCatalogAssetAdded(const AZ::Data::AssetId& assetId) override;
        void OnCatalogAssetRemoved(const AZ::Data::AssetId& assetId) override;

        ////////////////////////////////////////////////////////////////////////
        // AZ::Data::AssetBus
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        EditorSurfaceDataSystemConfig m_configuration;
        AZStd::unordered_map<AZ::Data::AssetId, AZ::Data::Asset<EditorSurfaceTagListAsset>> m_surfaceTagNameAssets;
    };
}