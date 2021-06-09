
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include "EMotionFXConfig.h"
#include "AnimGraphParamMotionNode.h"
#include "MotionInstance.h"
#include "ActorInstance.h"
#include "EventManager.h"
#include "EventHandler.h"
#include "AnimGraphInstance.h"
#include "MotionSet.h"
#include "SkeletalSubMotion.h"
#include "SkeletalMotion.h"
#include "AnimGraphManager.h"
#include "MotionManager.h"
#include "MotionInstancePool.h"
#include "EMotionFXManager.h"
#include "MotionEventTable.h"
#include "AnimGraph.h"

#include "Parameter/StringParameter.h"


namespace EMotionFX
{
    static AZStd::string EMPTY_STRING;

    AZ_CLASS_ALLOCATOR_IMPL(AnimGraphParamMotionNode, AnimGraphAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(AnimGraphParamMotionNode::UniqueData, AnimGraphObjectUniqueDataAllocator, 0)

    AnimGraphParamMotionNode::AnimGraphParamMotionNode()
        : AnimGraphNode()
        , m_playSpeed(1.0f)
        , m_loop(true)
        , m_retarget(true)
        , m_reverse(false)
        , m_emitEvents(true)
        , m_mirrorMotion(false)
        , m_motionExtraction(true)
        , m_rewindOnZeroWeight(false)
        , m_inPlace(false)
        , m_parameterIndex(AZ::Failure())
    {
        // setup the input ports
        InitInputPorts(2);
        SetupInputPortAsNumber("Play Speed", INPUTPORT_PLAYSPEED, PORTID_INPUT_PLAYSPEED);
        SetupInputPortAsNumber("In Place", INPUTPORT_INPLACE, PORTID_INPUT_INPLACE);

        // setup the output ports
        InitOutputPorts(2);
        SetupOutputPortAsPose("Output Pose", OUTPUTPORT_POSE, PORTID_OUTPUT_POSE);
        SetupOutputPortAsMotionInstance("Motion", OUTPUTPORT_MOTION, PORTID_OUTPUT_MOTION);
    }


    AnimGraphParamMotionNode::~AnimGraphParamMotionNode()
    {
    }


    void AnimGraphParamMotionNode::Reinit()
    {
        OnParamChanged();
        AnimGraphNode::Reinit();
    }


    bool AnimGraphParamMotionNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }


    const char* AnimGraphParamMotionNode::GetPaletteName() const
    {
        return "ParameterMotion";
    }


    AnimGraphObject::ECategory AnimGraphParamMotionNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_SOURCES;
    }


    bool AnimGraphParamMotionNode::GetIsInPlace(AnimGraphInstance* animGraphInstance) const
    {
        EMotionFX::BlendTreeConnection* inPlaceConnection = GetInputPort(INPUTPORT_INPLACE).mConnection;
        if (inPlaceConnection)
        {
            return GetInputNumberAsBool(animGraphInstance, INPUTPORT_INPLACE);
        }

        return m_inPlace;
    }

    void AnimGraphParamMotionNode::PostUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        if (mDisabled)
        {
            UniqueData* uniqueData = static_cast<UniqueData*>(FindOrCreateUniqueNodeData(animGraphInstance));
            RequestRefDatas(animGraphInstance);
            AnimGraphRefCountedData* data = uniqueData->GetRefCountedData();
            data->ClearEventBuffer();
            data->ZeroTrajectoryDelta();
            return;
        }

        // update the input nodes
        EMotionFX::BlendTreeConnection* playSpeedConnection = GetInputPort(INPUTPORT_PLAYSPEED).mConnection;
        if (playSpeedConnection && mDisabled == false)
        {
            playSpeedConnection->GetSourceNode()->PerformPostUpdate(animGraphInstance, timePassedInSeconds);
        }

        // clear the event buffer
        UniqueData* uniqueData = static_cast<UniqueData*>(FindOrCreateUniqueNodeData(animGraphInstance));
        RequestRefDatas(animGraphInstance);
        AnimGraphRefCountedData* data = uniqueData->GetRefCountedData();
        data->ClearEventBuffer();
        data->ZeroTrajectoryDelta();

        // trigger the motion update
        MotionInstance* motionInstance = uniqueData->mMotionInstance;
        if (motionInstance && !animGraphInstance->GetIsResynced(mObjectIndex))
        {
            // update the time values and extract events into the event buffer
            motionInstance->SetWeight(uniqueData->GetLocalWeight());
            motionInstance->UpdateByTimeValues(uniqueData->GetPreSyncTime(), uniqueData->GetCurrentPlayTime(), &data->GetEventBuffer());

            // mark all events to be emitted from this node
            data->GetEventBuffer().UpdateEmitters(this);
        }
        else
        {
            return;
        }

        // make sure the motion instance is ready for sampling
        if (motionInstance->GetIsReadyForSampling() == false)
        {
            motionInstance->InitForSampling();
        }

        // extract current delta
        Transform trajectoryDelta;
        const bool isMirrored = motionInstance->GetMirrorMotion();
        motionInstance->ExtractMotion(trajectoryDelta);
        data->SetTrajectoryDelta(trajectoryDelta);

        // extract mirrored version of the current delta
        motionInstance->SetMirrorMotion(!isMirrored);
        motionInstance->ExtractMotion(trajectoryDelta);
        data->SetTrajectoryDeltaMirrored(trajectoryDelta);

        // restore current mirrored flag
        motionInstance->SetMirrorMotion(isMirrored);
    }

    const AZStd::string& AnimGraphParamMotionNode::GetParamValue(AnimGraphInstance* animGraphInstance) const
    {
        if (m_parameterIndex.IsSuccess())
        {
            const ValueParameter* valueParameter = mAnimGraph->FindValueParameter(m_parameterIndex.GetValue());

            const AZ::TypeId parameterType = azrtti_typeid(valueParameter);
            if (parameterType == azrtti_typeid<StringParameter>())
            {
                const AZStd::string& paramValue = static_cast<MCore::AttributeString*>(animGraphInstance->GetParameterValue(static_cast<uint32>(m_parameterIndex.GetValue())))->GetValue();
                return paramValue;
            }
        }
        return EMPTY_STRING;
    }

    // top down update
    void AnimGraphParamMotionNode::TopDownUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(FindOrCreateUniqueNodeData(animGraphInstance));

        // rewind when the weight reaches 0 when we want to
        if (!m_loop)
        {
            if (uniqueData->mMotionInstance && uniqueData->GetLocalWeight() < MCore::Math::epsilon && m_rewindOnZeroWeight)
            {
                uniqueData->mMotionInstance->SetCurrentTime(0.0f);
                uniqueData->SetCurrentPlayTime(0.0f);
                uniqueData->SetPreSyncTime(0.0f);
            }
        }

        // sync all input nodes
        HierarchicalSyncAllInputNodes(animGraphInstance, uniqueData);

        // top down update all incoming connections
        for (BlendTreeConnection* connection : mConnections)
        {
            connection->GetSourceNode()->PerformTopDownUpdate(animGraphInstance, timePassedInSeconds);
        }
    }


    // update the motion instance
    void AnimGraphParamMotionNode::Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        // update the input nodes
        EMotionFX::BlendTreeConnection* playSpeedConnection = GetInputPort(INPUTPORT_PLAYSPEED).mConnection;
        if (playSpeedConnection && mDisabled == false)
        {
            UpdateIncomingNode(animGraphInstance, playSpeedConnection->GetSourceNode(), timePassedInSeconds);
        }

        if (!mDisabled)
        {
            UpdateIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_INPLACE), timePassedInSeconds);
        }

        // update the motion instance (current time etc)
        UniqueData* uniqueData = static_cast<UniqueData*>(FindOrCreateUniqueNodeData(animGraphInstance));
        MotionInstance* motionInstance = uniqueData->mMotionInstance;

        const AZStd::string& paramValue = GetParamValue(animGraphInstance);
        if (motionInstance == nullptr || uniqueData->mMotionName != paramValue)
        {
            uniqueData->mReload = true;
        }

        if (motionInstance == nullptr || mDisabled)
        {
            if (GetEMotionFX().GetIsInEditorMode())
            {
                if (mDisabled == false)
                {
                    if (motionInstance == nullptr)
                    {
                        SetHasError(uniqueData, true);
                    }
                }
            }

            uniqueData->Clear();
            return;
        }

        if (GetEMotionFX().GetIsInEditorMode())
        {
            SetHasError(uniqueData, false);
        }

        // if there is a node connected to the speed input port, read that value and use it as internal speed if not use the playspeed property
        const float customSpeed = ExtractCustomPlaySpeed(animGraphInstance);

        // all partial animations were broken: they were played only once
        if (!m_loop)
        {
            if (uniqueData->GetLocalWeight() < MCore::Math::epsilon && m_rewindOnZeroWeight)
            {
                motionInstance->SetCurrentTime(0.0f);
                uniqueData->SetCurrentPlayTime(0.0f);
                //uniqueData->SetPreSyncTime(0.0f);
            }
        }

        // set the internal speed and play speeds etc
        motionInstance->SetPlaySpeed(uniqueData->GetPlaySpeed());
        uniqueData->SetPlaySpeed(customSpeed);
        uniqueData->SetPreSyncTime(motionInstance->GetCurrentTime());

        // Make sure we use the correct play properties.
        motionInstance->SetPlayMode(m_playInfo.mPlayMode);
        motionInstance->SetRetargetingEnabled(m_playInfo.mRetarget && animGraphInstance->GetRetargetingEnabled());
        motionInstance->SetMotionEventsEnabled(m_playInfo.mEnableMotionEvents);
        motionInstance->SetMirrorMotion(m_playInfo.mMirrorMotion);
        motionInstance->SetEventWeightThreshold(m_playInfo.mEventWeightThreshold);
        motionInstance->SetMaxLoops(m_playInfo.mNumLoops);
        motionInstance->SetMotionExtractionEnabled(m_playInfo.mMotionExtractionEnabled);
        motionInstance->SetIsInPlace(GetIsInPlace(animGraphInstance));
        motionInstance->SetFreezeAtLastFrame(m_playInfo.mFreezeAtLastFrame);

        if (!animGraphInstance->GetIsObjectFlagEnabled(mObjectIndex, AnimGraphInstance::OBJECTFLAGS_SYNCED) || animGraphInstance->GetIsObjectFlagEnabled(mObjectIndex, AnimGraphInstance::OBJECTFLAGS_IS_SYNCLEADER))
        {
            // See where we would end up when we would forward in time.
            const MotionInstance::PlayStateOut newPlayState = motionInstance->CalcPlayStateAfterUpdate(timePassedInSeconds);

            // set the current time to the new calculated time
            uniqueData->ClearInheritFlags();
            uniqueData->SetCurrentPlayTime(newPlayState.m_currentTime);
            motionInstance->SetLastCurrentTime(motionInstance->GetCurrentTime());
            motionInstance->SetCurrentTime(newPlayState.m_currentTime, false);
        }

        uniqueData->SetDuration(motionInstance->GetDuration());

        // make sure the motion is not paused
        motionInstance->SetPause(false);

        uniqueData->SetSyncTrack(motionInstance->GetMotion()->GetEventTable()->GetSyncTrack());
        uniqueData->SetIsMirrorMotion(motionInstance->GetMirrorMotion());

        // update some flags
        if (motionInstance->GetPlayMode() == PLAYMODE_BACKWARD)
        {
            uniqueData->SetBackwardFlag();
        }
    }


    void AnimGraphParamMotionNode::UpdatePlayBackInfo(AnimGraphInstance* animGraphInstance)
    {
        m_playInfo.mPlayMode = (m_reverse) ? PLAYMODE_BACKWARD : PLAYMODE_FORWARD;
        m_playInfo.mNumLoops = (m_loop) ? EMFX_LOOPFOREVER : 1;
        m_playInfo.mFreezeAtLastFrame = true;
        m_playInfo.mEnableMotionEvents = m_emitEvents;
        m_playInfo.mMirrorMotion = m_mirrorMotion;
        m_playInfo.mPlaySpeed = ExtractCustomPlaySpeed(animGraphInstance);
        m_playInfo.mMotionExtractionEnabled = m_motionExtraction;
        m_playInfo.mRetarget = m_retarget;
        m_playInfo.mInPlace = GetIsInPlace(animGraphInstance);
    }


    // create the motion instance
    MotionInstance* AnimGraphParamMotionNode::CreateMotionInstance(ActorInstance* actorInstance, UniqueData* uniqueData)
    {
        AnimGraphInstance* animGraphInstance = uniqueData->GetAnimGraphInstance();

        // update the last motion ID
        UpdatePlayBackInfo(animGraphInstance);

        // try to find the motion to use for this actor instance in this blend node
        Motion* motion = nullptr;
        PlayBackInfo    playInfo = m_playInfo;

        // reset playback properties
        const float curLocalWeight = uniqueData->GetLocalWeight();
        const float curGlobalWeight = uniqueData->GetGlobalWeight();
        uniqueData->Clear();

        float currentTime = 0.0f;
        // remove the motion instance if it already exists
        if (uniqueData->mMotionInstance && uniqueData->mReload)
        {
            currentTime = uniqueData->mMotionInstance->GetCurrentTime();
            GetMotionInstancePool().Free(uniqueData->mMotionInstance);
            uniqueData->mMotionInstance = nullptr;
            uniqueData->mMotionSetID = MCORE_INVALIDINDEX32;
            uniqueData->mMotionName = "";
            uniqueData->mReload = false;
        }

        // get the motion set
        MotionSet* motionSet = animGraphInstance->GetMotionSet();
        if (!motionSet)
        {
            if (GetEMotionFX().GetIsInEditorMode())
            {
                SetHasError(uniqueData, true);
            }
            return nullptr;
        }

        // get the motion from the motion set, load it on demand and make sure the motion loaded successfully
        const AZStd::string& requestedMotionName = GetParamValue(animGraphInstance);
        motion = motionSet->RecursiveFindMotionById(requestedMotionName);

        if (!motion)
        {
            if (GetEMotionFX().GetIsInEditorMode())
            {
                SetHasError(uniqueData, true);
            }
            return nullptr;
        }

        uniqueData->mMotionSetID = motionSet->GetID();

        // create the motion instance
        MotionInstance* motionInstance = GetMotionInstancePool().RequestNew(motion, actorInstance, playInfo.mStartNodeIndex);
        motionInstance->InitFromPlayBackInfo(playInfo, true);
        motionInstance->SetRetargetingEnabled(animGraphInstance->GetRetargetingEnabled() && playInfo.mRetarget);

        uniqueData->SetSyncTrack(motionInstance->GetMotion()->GetEventTable()->GetSyncTrack());
        uniqueData->SetIsMirrorMotion(motionInstance->GetMirrorMotion());

        if (!motionInstance->GetIsReadyForSampling() && animGraphInstance->GetInitSettings().mPreInitMotionInstances)
        {
            motionInstance->InitForSampling();
        }

        // make sure it is not in pause mode
        motionInstance->UnPause();
        motionInstance->SetIsActive(true);
        motionInstance->SetWeight(1.0f, 0.0f);
        motionInstance->SetCurrentTime(currentTime);

        // update play info
        uniqueData->mMotionInstance = motionInstance;
        uniqueData->mMotionName = requestedMotionName;
        uniqueData->SetDuration(motionInstance->GetDuration());
        const float curPlayTime = motionInstance->GetCurrentTime();
        uniqueData->SetCurrentPlayTime(curPlayTime);
        uniqueData->SetPreSyncTime(curPlayTime);
        uniqueData->SetGlobalWeight(curGlobalWeight);
        uniqueData->SetLocalWeight(curLocalWeight);

        // trigger an event
        GetEventManager().OnStartMotionInstance(motionInstance, &playInfo);
        return motionInstance;
    }


    // the main process method of the final node
    void AnimGraphParamMotionNode::Output(AnimGraphInstance* animGraphInstance)
    {
        // if this motion is disabled, output the bind pose
        if (mDisabled)
        {
            // request poses to use from the pool, so that all output pose ports have a valid pose to output to we reuse them using a pool system to save memory
            RequestPoses(animGraphInstance);
            AnimGraphPose* outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
            outputPose->InitFromBindPose(actorInstance);
            return;
        }

        // output the playspeed node
        EMotionFX::BlendTreeConnection* playSpeedConnection = GetInputPort(INPUTPORT_PLAYSPEED).mConnection;
        if (playSpeedConnection)
        {
            OutputIncomingNode(animGraphInstance, playSpeedConnection->GetSourceNode());
        }

        // create and register the motion instance when this is the first time its being when it hasn't been registered yet
        ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
        MotionInstance* motionInstance = nullptr;
        UniqueData* uniqueData = static_cast<UniqueData*>(FindOrCreateUniqueNodeData(animGraphInstance));
        if (uniqueData->mReload)
        {
            motionInstance = CreateMotionInstance(actorInstance, uniqueData);
            uniqueData->mReload = false;
        }
        else
        {
            motionInstance = uniqueData->mMotionInstance;
        }

        // update the motion instance output port
        GetOutputMotionInstance(animGraphInstance, OUTPUTPORT_MOTION)->SetValue(motionInstance);

        if (motionInstance == nullptr)
        {
            // request poses to use from the pool, so that all output pose ports have a valid pose to output to we reuse them using a pool system to save memory
            RequestPoses(animGraphInstance);
            AnimGraphPose* outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            outputPose->InitFromBindPose(actorInstance);

            if (GetEMotionFX().GetIsInEditorMode())
            {
                SetHasError(uniqueData, true);
            }
            return;
        }

        if (GetEMotionFX().GetIsInEditorMode())
        {
            SetHasError(uniqueData, false);
        }

        // make sure the motion instance is ready for sampling
        if (motionInstance->GetIsReadyForSampling() == false)
        {
            motionInstance->InitForSampling();
        }

        EMotionFX::BlendTreeConnection* inPlaceConnection = GetInputPort(INPUTPORT_INPLACE).mConnection;
        if (inPlaceConnection)
        {
            OutputIncomingNode(animGraphInstance, inPlaceConnection->GetSourceNode());
        }

        // request poses to use from the pool, so that all output pose ports have a valid pose to output to we reuse them using a pool system to save memory
        RequestPoses(animGraphInstance);
        AnimGraphPose* outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
        Pose& outputTransformPose = outputPose->GetPose();

        // fill the output with the bind pose
        outputPose->InitFromBindPose(actorInstance); // TODO: is this really needed?

        // we use as input pose the same as the output, as this blend tree node takes no input
        motionInstance->GetMotion()->Update(&outputTransformPose, &outputTransformPose, motionInstance);

        // compensate for motion extraction
        // we already moved our actor instance's position and rotation at this point
        // so we have to cancel/compensate this delta offset from the motion extraction node, so that we don't double-transform
        // basically this will keep the motion in-place rather than moving it away from the origin
        if (motionInstance->GetMotionExtractionEnabled() && actorInstance->GetMotionExtractionEnabled() && !motionInstance->GetMotion()->GetIsAdditive())
        {
            outputTransformPose.CompensateForMotionExtractionDirect(motionInstance->GetMotion()->GetMotionExtractionFlags());
        }

        // visualize it
        if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
        {
            actorInstance->DrawSkeleton(outputPose->GetPose(), mVisualizeColor);
        }
    }


    // get the motion instance for a given anim graph instance
    MotionInstance* AnimGraphParamMotionNode::FindMotionInstance(AnimGraphInstance* animGraphInstance) const
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindOrCreateUniqueObjectData(this));
        return uniqueData->mMotionInstance;
    }


    // set the current play time
    void AnimGraphParamMotionNode::SetCurrentPlayTime(AnimGraphInstance* animGraphInstance, float timeInSeconds)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindOrCreateUniqueObjectData(this));
        uniqueData->SetCurrentPlayTime(timeInSeconds);
        if (uniqueData->mMotionInstance)
        {
            uniqueData->mMotionInstance->SetCurrentTime(timeInSeconds);
        }
    }


    // unique data constructor
    AnimGraphParamMotionNode::UniqueData::UniqueData(AnimGraphNode* node, AnimGraphInstance* animGraphInstance)
        : AnimGraphNodeData(node, animGraphInstance)
    {
    }

    AnimGraphParamMotionNode::UniqueData::~UniqueData()
    {
        GetMotionInstancePool().Free(mMotionInstance);
    }

    void AnimGraphParamMotionNode::UniqueData::Reset()
    {
        // stop and delete the motion instance
        if (mMotionInstance)
        {
            mMotionInstance->Stop(0.0f);
            GetMotionInstancePool().Free(mMotionInstance);
        }

        // reset the unique data
        mMotionSetID = MCORE_INVALIDINDEX32;
        mMotionInstance = nullptr;
        mReload = true;
        mPlaySpeed = 1.0f;
        mCurrentTime = 0.0f;
        mDuration = 0.0f;
        SetSyncTrack(nullptr);

        Invalidate();
    }

    void AnimGraphParamMotionNode::UniqueData::Update()
    {
        AnimGraphParamMotionNode* motionNode = azdynamic_cast<AnimGraphParamMotionNode*>(mObject);
        AZ_Assert(motionNode, "Unique data linked to incorrect node type.");

        AnimGraphInstance* animGraphInstance = GetAnimGraphInstance();

        mReload = true;
        motionNode->CreateMotionInstance(animGraphInstance->GetActorInstance(), this);

        // get the id of the currently used the motion set
        MotionSet* motionSet = animGraphInstance->GetMotionSet();
        uint32 motionSetID = MCORE_INVALIDINDEX32;
        if (motionSet)
        {
            motionSetID = motionSet->GetID();
        }

        // update the internally stored playback info
        motionNode->UpdatePlayBackInfo(animGraphInstance);

        // update play info
        if (mMotionInstance)
        {
            MotionInstance* motionInstance = mMotionInstance;
            const float currentTime = motionInstance->GetCurrentTime();
            SetDuration(motionInstance->GetDuration());
            SetCurrentPlayTime(currentTime);
            SetPreSyncTime(currentTime);
            SetSyncTrack(motionInstance->GetMotion()->GetEventTable()->GetSyncTrack());
            SetIsMirrorMotion(motionInstance->GetMirrorMotion());
        }
    }

    // this function will get called to rewind motion nodes as well as states etc. to reset several settings when a state gets exited
    void AnimGraphParamMotionNode::Rewind(AnimGraphInstance* animGraphInstance)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->GetUniqueObjectData(mObjectIndex));

        // rewind is not necessary if unique data is not created yet
        if (!uniqueData)
        {
            return;
        }

        // find the motion instance for the given anim graph and return directly in case it is invalid
        MotionInstance* motionInstance = uniqueData->mMotionInstance;
        if (motionInstance == nullptr)
        {
            return;
        }

        // reset several settings to rewind the motion instance
        motionInstance->ResetTimes();
        motionInstance->SetIsFrozen(false);
        SetSyncIndex(animGraphInstance, MCORE_INVALIDINDEX32);
        uniqueData->SetCurrentPlayTime(motionInstance->GetCurrentTime());
        uniqueData->SetDuration(motionInstance->GetDuration());
        uniqueData->SetPreSyncTime(uniqueData->GetCurrentPlayTime());
        //uniqueData->SetPlaySpeed( uniqueData->GetPlaySpeed() );
    }

    // get the speed from the connection if there is one connected, if not use the node's playspeed
    float AnimGraphParamMotionNode::ExtractCustomPlaySpeed(AnimGraphInstance* animGraphInstance) const
    {
        EMotionFX::BlendTreeConnection* playSpeedConnection = GetInputPort(INPUTPORT_PLAYSPEED).mConnection;

        // if there is a node connected to the speed input port, read that value and use it as internal speed
        float customSpeed;
        if (playSpeedConnection)
        {
            customSpeed = MCore::Max(0.0f, GetInputNumberAsFloat(animGraphInstance, INPUTPORT_PLAYSPEED));
        }
        else
        {
            customSpeed = m_playSpeed; // otherwise use the node's playspeed
        }

        return customSpeed;
    }

    void AnimGraphParamMotionNode::ReloadAndInvalidateUniqueDatas()
    {
        if (!mAnimGraph)
        {
            return;
        }

        const size_t numAnimGraphInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numAnimGraphInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);
            UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->GetUniqueObjectData(mObjectIndex));
            if (uniqueData)
            {
                uniqueData->mReload = true;
                uniqueData->Invalidate();
            }
        }
    }

    void AnimGraphParamMotionNode::OnActorMotionExtractionNodeChanged()
    {
        ReloadAndInvalidateUniqueDatas();
    }

    void AnimGraphParamMotionNode::RecursiveOnChangeMotionSet(AnimGraphInstance* animGraphInstance, MotionSet* newMotionSet)
    {
        AnimGraphNode::RecursiveOnChangeMotionSet(animGraphInstance, newMotionSet);
        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->GetUniqueObjectData(mObjectIndex));
        if (uniqueData)
        {
            uniqueData->mReload = true;
            uniqueData->Invalidate();
        }
    }

    void AnimGraphParamMotionNode::OnParamChanged()
    {
        ReloadAndInvalidateUniqueDatas();

        // Set the node info text.
        UpdateNodeInfo();
        SyncVisualObject();
    }

    void AnimGraphParamMotionNode::UpdateNodeInfo()
    {
        m_parameterIndex = mAnimGraph->FindValueParameterIndexByName(m_parameterName);
        SetNodeInfo(m_parameterName.c_str());
    }

    AZ::Crc32 AnimGraphParamMotionNode::GetRewindOnZeroWeightVisibility() const
    {
        return m_loop ? AZ::Edit::PropertyVisibility::Hide : AZ::Edit::PropertyVisibility::Show;
    }

    void AnimGraphParamMotionNode::SetRewindOnZeroWeight(bool rewindOnZeroWeight)
    {
        m_rewindOnZeroWeight = rewindOnZeroWeight;
    }

    void AnimGraphParamMotionNode::SetMotionPlaySpeed(float playSpeed)
    {
        m_playSpeed = playSpeed;
    }

    void AnimGraphParamMotionNode::SetEmitEvents(bool emitEvents)
    {
        m_emitEvents = emitEvents;
    }

    void AnimGraphParamMotionNode::SetMotionExtraction(bool motionExtraction)
    {
        m_motionExtraction = motionExtraction;
    }

    void AnimGraphParamMotionNode::SetMirrorMotion(bool mirrorMotion)
    {
        m_mirrorMotion = mirrorMotion;
    }

    void AnimGraphParamMotionNode::SetReverse(bool reverse)
    {
        m_reverse = reverse;
    }

    void AnimGraphParamMotionNode::SetRetarget(bool retarget)
    {
        m_retarget = retarget;
    }

    void AnimGraphParamMotionNode::SetLoop(bool loop)
    {
        m_loop = loop;
    }

    bool AnimGraphParamMotionNode::VersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        return true;
    }

    void AnimGraphParamMotionNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<AnimGraphParamMotionNode, AnimGraphNode>()
            ->Version(3, VersionConverter)
            ->Field("loop", &AnimGraphParamMotionNode::m_loop)
            ->Field("retarget", &AnimGraphParamMotionNode::m_retarget)
            ->Field("reverse", &AnimGraphParamMotionNode::m_reverse)
            ->Field("emitEvents", &AnimGraphParamMotionNode::m_emitEvents)
            ->Field("mirrorMotion", &AnimGraphParamMotionNode::m_mirrorMotion)
            ->Field("motionExtraction", &AnimGraphParamMotionNode::m_motionExtraction)
            ->Field("inPlace", &AnimGraphParamMotionNode::m_inPlace)
            ->Field("playSpeed", &AnimGraphParamMotionNode::m_playSpeed)
            ->Field("rewindOnZeroWeight", &AnimGraphParamMotionNode::m_rewindOnZeroWeight)
            ->Field("parameter", &AnimGraphParamMotionNode::m_parameterName)
            ;

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<AnimGraphParamMotionNode>("Motion", "Motion attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ_CRC("AnimGraphParameter", 0x778af55a), &AnimGraphParamMotionNode::m_parameterName, "Parameter", "The parameter name to apply the condition on.")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::Reinit)
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->Attribute(AZ_CRC("AnimGraph", 0x0d53d4b3), &AnimGraphParamMotionNode::GetAnimGraph)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_loop, "Loop", "Loop the motion?")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::InvalidateUniqueDatas)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_retarget, "Retarget", "Is this motion allowed to be retargeted?")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::InvalidateUniqueDatas)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_reverse, "Reverse", "Playback reversed?")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::InvalidateUniqueDatas)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_emitEvents, "Emit Events", "Emit motion events?")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::InvalidateUniqueDatas)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_inPlace, "In Place", "Should the motion be in place and not move? This is most likely only used if you do not use motion extraction but your motion data moves the character away from the origin.")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::InvalidateUniqueDatas)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_mirrorMotion, "Mirror Motion", "Mirror the motion?")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::ReloadAndInvalidateUniqueDatas)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_motionExtraction, "Motion Extraction", "Enable motion extraction?")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &AnimGraphParamMotionNode::InvalidateUniqueDatas)
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &AnimGraphParamMotionNode::m_playSpeed, "Play Speed", "The playback speed factor.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Max, 100.0f)
            ->Attribute(AZ::Edit::Attributes::Step, 0.05f)
            ->DataElement(AZ::Edit::UIHandlers::Default, &AnimGraphParamMotionNode::m_rewindOnZeroWeight, "Rewind On Zero Weight", "Rewind the motion when its local weight is near zero. Useful to restart non-looping motions. Looping needs to be disabled for this to work.")
            ->Attribute(AZ::Edit::Attributes::Visibility, &AnimGraphParamMotionNode::GetRewindOnZeroWeightVisibility)
            ;
    }
} // namespace EMotionFX
