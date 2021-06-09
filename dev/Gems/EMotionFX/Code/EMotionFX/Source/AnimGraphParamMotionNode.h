
#pragma once

#include "EMotionFXConfig.h"
#include "AnimGraphNode.h"
#include "AnimGraphNodeData.h"
#include "PlayBackInfo.h"
#include <AzCore/Serialization/SerializeContext.h>

namespace EMotionFX
{
    // forward declarations
    class Motion;
    class ActorInstance;
    class MotionSet;

    /**
     *
     *
     */
    class EMFX_API AnimGraphParamMotionNode
        : public AnimGraphNode
    {
    public:
        AZ_RTTI(AnimGraphMotionNode, "{2A41BB98-9FF2-4788-B24F-0AD17862AAED}", AnimGraphNode)
        AZ_CLASS_ALLOCATOR_DECL

        enum
        {
            INPUTPORT_PLAYSPEED = 0,
            INPUTPORT_INPLACE = 1,

            OUTPUTPORT_POSE = 0,
            OUTPUTPORT_MOTION = 1
        };

        enum
        {
            PORTID_INPUT_PLAYSPEED = 0,
            PORTID_INPUT_INPLACE = 1,

            PORTID_OUTPUT_POSE = 0,
            PORTID_OUTPUT_MOTION = 1
        };

        class EMFX_API UniqueData
            : public AnimGraphNodeData
        {
            EMFX_ANIMGRAPHOBJECTDATA_IMPLEMENT_LOADSAVE
        public:
            AZ_CLASS_ALLOCATOR_DECL

            UniqueData(AnimGraphNode* node, AnimGraphInstance* animGraphInstance);
            ~UniqueData() override;

            void Reset() override;
            void Update() override;

        public:
            uint32 mMotionSetID = InvalidIndex32;
            AZStd::string mMotionName = "";
            MotionInstance* mMotionInstance = nullptr;
            bool mReload = false;
        };

        AnimGraphParamMotionNode();
        ~AnimGraphParamMotionNode();

        void Reinit() override;
        bool InitAfterLoading(AnimGraph* animGraph) override;

        bool GetHasOutputPose() const override { return true; }
        bool GetCanActAsState() const override { return true; }
        bool GetSupportsDisable() const override { return true; }
        bool GetSupportsVisualization() const override { return true; }
        bool GetSupportsPreviewMotion() const override { return true; }
        bool GetNeedsNetTimeSync() const override { return true; }
        AZ::Color GetVisualColor() const override { return AZ::Color(0.38f, 0.24f, 0.91f, 1.0f); }

        AnimGraphObjectData* CreateUniqueData(AnimGraphInstance* animGraphInstance) override { return aznew UniqueData(this, animGraphInstance); }
        void OnActorMotionExtractionNodeChanged() override;
        void RecursiveOnChangeMotionSet(AnimGraphInstance* animGraphInstance, MotionSet* newMotionSet) override;

        const char* GetPaletteName() const override;
        AnimGraphObject::ECategory GetPaletteCategory() const override;

        MotionInstance* FindMotionInstance(AnimGraphInstance* animGraphInstance) const;

        AnimGraphPose* GetMainOutputPose(AnimGraphInstance* animGraphInstance) const override { return GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue(); }

        void SetCurrentPlayTime(AnimGraphInstance* animGraphInstance, float timeInSeconds) override;
        void Rewind(AnimGraphInstance* animGraphInstance) override;

        void UpdatePlayBackInfo(AnimGraphInstance* animGraphInstance);

        float ExtractCustomPlaySpeed(AnimGraphInstance* animGraphInstance) const;

        float GetMotionPlaySpeed() const { return m_playSpeed; }
        bool GetIsLooping() const { return m_loop; }
        bool GetIsRetargeting() const { return m_retarget; }
        bool GetIsReversed() const { return m_reverse; }
        bool GetEmitEvents() const { return m_emitEvents; }
        bool GetMirrorMotion() const { return m_mirrorMotion; }
        bool GetIsMotionExtraction() const { return m_motionExtraction; }
        float GetDefaultPlaySpeed() const { return m_playSpeed; }

        void SetLoop(bool loop);
        void SetRetarget(bool retarget);
        void SetReverse(bool reverse);
        void SetEmitEvents(bool emitEvents);
        void SetMirrorMotion(bool mirrorMotion);
        void SetMotionExtraction(bool motionExtraction);
        void SetMotionPlaySpeed(float playSpeed);
        void SetRewindOnZeroWeight(bool rewindOnZeroWeight);

        bool GetIsInPlace(AnimGraphInstance* animGraphInstance) const;

        static void Reflect(AZ::ReflectContext* context);
        static bool VersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement);

    private:
        static const float                                  s_defaultWeight;

        PlayBackInfo                                        m_playInfo;
        float                                               m_playSpeed;
        bool                                                m_loop;
        bool                                                m_retarget;
        bool                                                m_reverse;
        bool                                                m_emitEvents;
        bool                                                m_mirrorMotion;
        bool                                                m_motionExtraction;
        bool                                                m_rewindOnZeroWeight;
        bool                                                m_inPlace;
        AZStd::string                                       m_parameterName;
        AZ::Outcome<size_t>                                 m_parameterIndex;

        void ReloadAndInvalidateUniqueDatas();
        MotionInstance* CreateMotionInstance(ActorInstance* actorInstance, UniqueData* uniqueData);
        void TopDownUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;
        void Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;
        void Output(AnimGraphInstance* animGraphInstance) override;
        void PostUpdate(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;
        const AZStd::string& GetParamValue(AnimGraphInstance* animGraphInstance) const;

        void OnParamChanged();

        AZ::Crc32 GetRewindOnZeroWeightVisibility() const;

        void UpdateNodeInfo();
    };
} // namespace EMotionFX