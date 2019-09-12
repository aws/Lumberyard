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

#include <AzCore/Component/ComponentApplicationBus.h>
#include <SceneAPI/SceneData/Behaviors/Registry.h>
#include <SceneAPI/SceneData/Behaviors/AnimationGroupBehavior.h>
#include <SceneAPI/SceneData/Behaviors/BlendShapeRuleBehavior.h>
#include <SceneAPI/SceneData/Behaviors/LodRuleBehavior.h>
#include <SceneAPI/SceneData/Behaviors/MaterialRuleBehavior.h>
#include <SceneAPI/SceneData/Behaviors/MeshAdvancedRule.h>
#include <SceneAPI/SceneData/Behaviors/MeshGroupBehavior.h>
#include <SceneAPI/SceneData/Behaviors/PhysicsRuleBehavior.h>
#include <SceneAPI/SceneData/Behaviors/SkeletonGroupBehavior.h>
#include <SceneAPI/SceneData/Behaviors/SkinGroupBehavior.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneData
        {
            void Registry::RegisterComponents(ComponentDescriptorList& components)
            {
                components.insert(components.end(),
                {
                    Behaviors::AnimationGroup::CreateDescriptor(),
                    BlendShapeRuleBehavior::CreateDescriptor(),
                    LodRuleBehavior::CreateDescriptor(),
                    MaterialRuleBehavior::CreateDescriptor(),
                    Behaviors::MeshAdvancedRule::CreateDescriptor(),
                    Behaviors::MeshGroup::CreateDescriptor(),
                    PhysicsRuleBehavior::CreateDescriptor(),
                    Behaviors::SkeletonGroup::CreateDescriptor(),
                    Behaviors::SkinGroup::CreateDescriptor()
                });
            }
        } // SceneData
    } // SceneAPI
} // AZ
