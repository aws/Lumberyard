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

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/vector.h>

#include <AzFramework/Physics/RigidBody.h>

#include <NvBlastExtDamageShaders.h>
#include <NvBlastTypes.h>

namespace AZ
{
    class Transform;
}

namespace Nv::Blast
{
    class TkActor;
} // namespace Nv::Blast

namespace Blast
{
    class BlastFamily;

    //! Represents a part of the destructed object by holding a reference to an entity with a rigid body.
    class BlastActor
    {
    public:
        AZ_CLASS_ALLOCATOR(BlastActor, AZ::SystemAllocator, 0);

        virtual ~BlastActor() = default;

        // Applies pending damage to the actor. Actual damage will be simulated by BlastSystemComponent.
        virtual void Damage(const NvBlastDamageProgram& program, NvBlastExtProgramParams* programParams) = 0;

        virtual AZ::Transform GetTransform() const = 0;
        virtual const BlastFamily& GetFamily() const = 0;
        virtual Nv::Blast::TkActor& GetTkActor() const = 0;
        virtual Physics::WorldBody* GetWorldBody() = 0;
        virtual const Physics::WorldBody* GetWorldBody() const = 0;
        virtual const AZ::Entity* GetEntity() const = 0;
        virtual const AZStd::vector<uint32_t>& GetChunkIndices() const = 0;
        virtual bool IsStatic() const = 0;
    };
} // namespace Blast
