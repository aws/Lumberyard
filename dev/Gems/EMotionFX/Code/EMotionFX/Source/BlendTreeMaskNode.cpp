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

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/ActorInstance.h>
#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/AnimGraphAttributeTypes.h>
#include <EMotionFX/Source/AnimGraphInstance.h>
#include <EMotionFX/Source/BlendTreeMaskNode.h>
#include <EMotionFX/Source/EMotionFXManager.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeMaskNode, AnimGraphAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeMaskNode::Mask, AnimGraphAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeMaskNode::UniqueData, AnimGraphObjectUniqueDataAllocator, 0)
    const size_t BlendTreeMaskNode::s_numMasks = 4;

    BlendTreeMaskNode::BlendTreeMaskNode()
        : AnimGraphNode()
    {
        m_masks.resize(s_numMasks);

        // Setup the input ports.
        InitInputPorts(1 + static_cast<AZ::u32>(s_numMasks)); // Base pose and the input poses for the masks.
        SetupInputPort("Base Pose", INPUTPORT_BASEPOSE, AttributePose::TYPE_ID, INPUTPORT_BASEPOSE);
        for (size_t i = 0; i < s_numMasks; ++i)
        {
            const AZ::u32 portNr = static_cast<AZ::u32>(i + INPUTPORT_START);
            SetupInputPort(
                AZStd::string::format("Pose %d", i).c_str(),
                portNr,
                AttributePose::TYPE_ID,
                portNr);
        }

        // Setup the output ports.
        InitOutputPorts(1);
        SetupOutputPortAsPose("Output Pose", OUTPUTPORT_RESULT, PORTID_OUTPUT_RESULT);

        ActorNotificationBus::Handler::BusConnect();
    }

    BlendTreeMaskNode::~BlendTreeMaskNode()
    {
        ActorNotificationBus::Handler::BusDisconnect();
    }

    void BlendTreeMaskNode::Reinit()
    {
        AnimGraphNode::Reinit();

        AZ::u8 maskCounter = 0;
        for (Mask& mask : m_masks)
        {
            mask.m_maskIndex = maskCounter;
            mask.m_parent = this;
            maskCounter++;
        }

        const size_t numAnimGraphInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numAnimGraphInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);

            UniqueData* uniqueData = reinterpret_cast<UniqueData*>(animGraphInstance->FindUniqueObjectData(this));
            if (uniqueData)
            {
                uniqueData->mMustUpdate = true;
            }
        }

        UpdateUniqueDatas();
    }

    bool BlendTreeMaskNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }

    void BlendTreeMaskNode::OnMotionExtractionNodeChanged(Actor* actor, Node* newMotionExtractionNode)
    {
        if (!mAnimGraph)
        {
            return;
        }

        bool needsReinit = false;
        const size_t numAnimGraphInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numAnimGraphInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);
            if (actor == animGraphInstance->GetActorInstance()->GetActor())
            {
                needsReinit = true;
                break;
            }
        }

        if (needsReinit)
        {
            Reinit();
        }
    }

    void BlendTreeMaskNode::OnUpdateUniqueData(AnimGraphInstance* animGraphInstance)
    {
        UniqueData* uniqueData = static_cast<BlendTreeMaskNode::UniqueData*>(animGraphInstance->FindUniqueObjectData(this));
        if (!uniqueData)
        {
            uniqueData = aznew UniqueData(this, animGraphInstance);
            animGraphInstance->RegisterUniqueObjectData(uniqueData);
        }

        uniqueData->mMustUpdate = true;
        UpdateUniqueData(animGraphInstance, uniqueData);
    }

    void BlendTreeMaskNode::Output(AnimGraphInstance* animGraphInstance)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        UpdateUniqueData(animGraphInstance, uniqueData);

        RequestPoses(animGraphInstance);
        AnimGraphPose* outputAnimGraphPose = GetOutputPose(animGraphInstance, OUTPUTPORT_RESULT)->GetValue();
        Pose& outputPose = outputAnimGraphPose->GetPose();

        // Use the input base pose as starting pose to apply the masks onto.
        AnimGraphNode* basePoseNode = GetInputNode(INPUTPORT_BASEPOSE);
        if (basePoseNode)
        {
            OutputIncomingNode(animGraphInstance, basePoseNode);
            *outputAnimGraphPose = *basePoseNode->GetMainOutputPose(animGraphInstance);
        }
        else
        {
            // Use bindpose in case no base pose node is connected.
            outputAnimGraphPose->InitFromBindPose(animGraphInstance->GetActorInstance());
        }

        // Iterate over the non-empty masks and copy over its transforms.
        for (const UniqueData::MaskInstance& maskInstance : uniqueData->m_maskInstances)
        {
            const AZ::u32 inputPortNr = maskInstance.m_inputPortNr;
            AnimGraphNode* inputNode = GetInputNode(inputPortNr);
            if (inputNode)
            {
                OutputIncomingNode(animGraphInstance, inputNode);
                const Pose& inputPose = GetInputPose(animGraphInstance, inputPortNr)->GetValue()->GetPose();

                for (AZ::u32 jointIndex : maskInstance.m_jointIndices)
                {
                    outputPose.SetLocalSpaceTransform(jointIndex, inputPose.GetLocalSpaceTransform(jointIndex));
                }
            }
        }

        if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
        {
            animGraphInstance->GetActorInstance()->DrawSkeleton(outputAnimGraphPose->GetPose(), mVisualizeColor);
        }
    }

    void BlendTreeMaskNode::Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));

        AnimGraphNode* basePoseNode = GetInputNode(INPUTPORT_BASEPOSE);
        if (basePoseNode)
        {
            basePoseNode->PerformUpdate(animGraphInstance, timePassedInSeconds);
            uniqueData->Init(animGraphInstance, basePoseNode);
        }
        else
        {
            uniqueData->Clear();
        }

        for (const UniqueData::MaskInstance& maskInstance : uniqueData->m_maskInstances)
        {
            AnimGraphNode* inputNode = GetInputNode(maskInstance.m_inputPortNr);
            if (inputNode)
            {
                inputNode->PerformUpdate(animGraphInstance, timePassedInSeconds);
            }
        }
    }

    void BlendTreeMaskNode::PostUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        RequestRefDatas(animGraphInstance);
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        AnimGraphRefCountedData* data = uniqueData->GetRefCountedData();
        data->ClearEventBuffer();
        data->ZeroTrajectoryDelta();

        AnimGraphNode* basePoseNode = GetInputNode(INPUTPORT_BASEPOSE);
        if (basePoseNode)
        {
            basePoseNode->PerformPostUpdate(animGraphInstance, timePassedInSeconds);

            const AnimGraphNodeData* basePoseNodeUniqueData = basePoseNode->FindUniqueNodeData(animGraphInstance);
            data->SetEventBuffer(basePoseNodeUniqueData->GetRefCountedData()->GetEventBuffer());
        }

        const size_t numMaskInstances = uniqueData->m_maskInstances.size();
        for (size_t i = 0; i < numMaskInstances; ++i)
        {
            const UniqueData::MaskInstance& maskInstance = uniqueData->m_maskInstances[i];
            const AZ::u32 inputPortNr = maskInstance.m_inputPortNr;
            AnimGraphNode* inputNode = GetInputNode(inputPortNr);
            if (!inputNode)
            {
                continue;
            }

            inputNode->PerformPostUpdate(animGraphInstance, timePassedInSeconds);

            // If we want to output events for this input, add the incoming events to the output event buffer.
            if (GetOutputEvents(inputPortNr))
            {
                const AnimGraphEventBuffer& inputEventBuffer = inputNode->FindUniqueNodeData(animGraphInstance)->GetRefCountedData()->GetEventBuffer();

                AnimGraphEventBuffer& outputEventBuffer = data->GetEventBuffer();
                outputEventBuffer.AddAllEventsFrom(inputEventBuffer);
            }
        }

        // Apply motion extraction delta from either the base pose or one of the masks depending on if a mask has the joint set or not.
        bool motionExtractionApplied = false;
        if (uniqueData->m_motionExtractionInputPortNr.has_value())
        {
            AnimGraphNode* inputNode = GetInputNode(uniqueData->m_motionExtractionInputPortNr.value());
            if (inputNode)
            {
                AnimGraphRefCountedData* sourceData = inputNode->FindUniqueNodeData(animGraphInstance)->GetRefCountedData();
                data->SetTrajectoryDelta(sourceData->GetTrajectoryDelta());
                data->SetTrajectoryDeltaMirrored(sourceData->GetTrajectoryDeltaMirrored());
                motionExtractionApplied = true;
            }
        }

        // In case the motion extraction node is not part of any of the masks while the base pose is connected, use that as a fallback.
        if (!motionExtractionApplied && basePoseNode)
        {
            AnimGraphRefCountedData* sourceData = basePoseNode->FindUniqueNodeData(animGraphInstance)->GetRefCountedData();
            data->SetTrajectoryDelta(sourceData->GetTrajectoryDelta());
            data->SetTrajectoryDeltaMirrored(sourceData->GetTrajectoryDeltaMirrored());
        }
    }

    size_t BlendTreeMaskNode::GetNumUsedMasks() const
    {
        size_t result = 0;
        for (const Mask& mask : m_masks)
        {
            if (!mask.m_jointNames.empty())
            {
                result++;
            }
        }
        return result;
    }

    void BlendTreeMaskNode::UpdateUniqueData(AnimGraphInstance* animGraphInstance, UniqueData* uniqueData)
    {
        if (uniqueData->mMustUpdate)
        {
            Actor* actor = animGraphInstance->GetActorInstance()->GetActor();
            const size_t numMaskInstances = GetNumUsedMasks();
            uniqueData->m_maskInstances.resize(numMaskInstances);
            AZ::u32 maskInstanceIndex = 0;

            uniqueData->m_motionExtractionInputPortNr.reset();
            const AZ::u32 motionExtractionJointIndex = animGraphInstance->GetActorInstance()->GetActor()->GetMotionExtractionNodeIndex();

            const size_t numMasks = m_masks.size();
            for (size_t i = 0; i < numMasks; ++i)
            {
                const Mask& mask = m_masks[i];
                if (!mask.m_jointNames.empty())
                {
                    const AZ::u32 inputPortNr = INPUTPORT_START + static_cast<AZ::u32>(i);

                    // Get the joint indices by joint names and cache them in the unique data
                    // so that we don't have to look them up at runtime.
                    UniqueData::MaskInstance& maskInstance = uniqueData->m_maskInstances[maskInstanceIndex];
                    AnimGraphPropertyUtils::ReinitJointIndices(actor, mask.m_jointNames, maskInstance.m_jointIndices);
                    maskInstance.m_inputPortNr = inputPortNr;

                    // Check if the motion extraction node is part of this mask and cache the mask index in that case.
                    for (AZ::u32 jointIndex : maskInstance.m_jointIndices)
                    {
                        if (jointIndex == motionExtractionJointIndex)
                        {
                            uniqueData->m_motionExtractionInputPortNr = inputPortNr;
                            break;
                        }
                    }

                    maskInstanceIndex++;
                }
            }

            // Don't update the next time again.
            uniqueData->mMustUpdate = false;
        }
    }

    AZStd::string BlendTreeMaskNode::GetMaskJointName(size_t maskIndex, size_t jointIndex) const
    {
        return m_masks[maskIndex].m_jointNames[jointIndex];
    }

    bool BlendTreeMaskNode::GetOutputEvents(size_t inputPortNr) const
    {
        if (inputPortNr > INPUTPORT_BASEPOSE)
        {
            return m_masks[inputPortNr - INPUTPORT_START].m_outputEvents;
        }

        return true;
    }

    void BlendTreeMaskNode::SetMask(size_t maskIndex, const AZStd::vector<AZStd::string>& jointNames)
    {
        m_masks[maskIndex].m_jointNames = jointNames;
    }

    void BlendTreeMaskNode::SetOutputEvents(size_t maskIndex, bool outputEvents)
    {
        m_masks[maskIndex].m_outputEvents = outputEvents;
    }

    void BlendTreeMaskNode::Mask::Reinit()
    {
        if (m_parent)
        {
            m_parent->Reinit();
        }
    }

    AZStd::string BlendTreeMaskNode::Mask::GetMaskName() const
    {
        return AZStd::string::format("GetMask %d", m_maskIndex);
    }

    AZStd::string BlendTreeMaskNode::Mask::GetOutputEventsName() const
    {
        return AZStd::string::format("Output Events %d", m_maskIndex);
    }

    void BlendTreeMaskNode::Mask::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<Mask>()
                ->Version(1)
                ->Field("jointNames", &BlendTreeMaskNode::Mask::m_jointNames)
                ->Field("outputEvents", &BlendTreeMaskNode::Mask::m_outputEvents)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<Mask>("Pose Hello", "Pose mark attributes")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->DataElement(AZ_CRC("ActorNodes", 0x70504714), &BlendTreeMaskNode::Mask::m_jointNames, "Mask", "The mask to apply.")
                        ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::HideChildren)
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &BlendTreeMaskNode::Mask::GetMaskName)
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeMaskNode::Mask::Reinit)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeMaskNode::Mask::m_outputEvents, "Output Events", "Output events.")
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &BlendTreeMaskNode::Mask::GetOutputEventsName)
                    ;
            }
        }
    }

    void BlendTreeMaskNode::Reflect(AZ::ReflectContext* context)
    {
        BlendTreeMaskNode::Mask::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<BlendTreeMaskNode, AnimGraphNode>()
                ->Version(1)
                ->Field("masks", &BlendTreeMaskNode::m_masks)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<BlendTreeMaskNode>("Pose Mask", "Pose mark attributes")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeMaskNode::m_masks, "Masks", "The mask to apply on the Pose 1 input port.")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeMaskNode::Reinit)
                        ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                    ;
            }
        }
    }
} // namespace EMotionFX
