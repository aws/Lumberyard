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

#include "../StandardPluginsConfig.h"
#include <AzCore/std/containers/vector.h>
#include <MCore/Source/StandardHeaders.h>
#include <EMotionFX/Source/Node.h>
#include <EMotionFX/Source/PlayBackInfo.h>
#include <Editor/ActorJointBrowseEdit.h>
#include "../../../../EMStudioSDK/Source/NodeSelectionWindow.h"
#include "CollisionMeshesSetupWindow.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>

QT_FORWARD_DECLARE_CLASS(QPushButton)

namespace EMStudio
{
    // forward declaration
    class SceneManagerPlugin;
    class MirrorSetupWindow;
    class RetargetSetupWindow;

    class ActorPropertiesWindow
        : public QWidget
    {
        Q_OBJECT // AUTOMOC
        MCORE_MEMORYOBJECTCATEGORY(ActorPropertiesWindow, MCore::MCORE_DEFAULT_ALIGNMENT, MEMCATEGORY_STANDARDPLUGINS);

    public:
        enum
        {
            CLASS_ID = 0x00000005
        };

        ActorPropertiesWindow(QWidget* parent, SceneManagerPlugin* plugin);
        ~ActorPropertiesWindow() = default;

        void Init();

        // helper functions
        static void GetNodeName(const MCore::Array<SelectionItem>& selection, AZStd::string* outNodeName, uint32* outActorID);
        static void GetNodeName(const AZStd::vector<SelectionItem>& joints, AZStd::string* outNodeName, uint32* outActorID);

    public slots:
        void UpdateInterface();

        // actor name
        void NameEditChanged();

        void OnMotionExtractionJointSelected(const AZStd::vector<SelectionItem>& selectedJoints);
        void OnFindBestMatchingNode();

        void OnRetargetRootJointSelected(const AZStd::vector<SelectionItem>& selectedJoints);

        void OnMirrorSetup();
        //void OnRetargetSetup();
        void OnCollisionMeshesSetup();

        // nodes excluded from bounding volume calculations
        void OnExcludedJointsFromBoundsSelectionDone(const AZStd::vector<SelectionItem>& selectedJoints);
        void OnExcludedJointsFromBoundsSelectionChanged(const AZStd::vector<SelectionItem>& selectedJoints);

    private:
        ActorJointBrowseEdit* m_motionExtractionJointBrowseEdit = nullptr;
        QPushButton* m_findBestMatchButton = nullptr;

        ActorJointBrowseEdit* m_retargetRootJointBrowseEdit = nullptr;

        ActorJointBrowseEdit* m_excludeFromBoundsBrowseEdit = nullptr;

        AzQtComponents::BrowseEdit*     mCollisionMeshesSetupLink = nullptr;
        CollisionMeshesSetupWindow*     mCollisionMeshesSetupWindow = nullptr;

        AzQtComponents::BrowseEdit*     mMirrorSetupLink = nullptr;
        MirrorSetupWindow*              mMirrorSetupWindow = nullptr;

        //MysticQt::LinkWidget*         mRetargetSetupLink = nullptr;
        //RetargetSetupWindow*          mRetargetSetupWindow = nullptr;

        // actor name
        QLineEdit*                      mNameEdit = nullptr;

        SceneManagerPlugin*             mPlugin = nullptr;
        EMotionFX::Actor*               mActor = nullptr;
        EMotionFX::ActorInstance*       mActorInstance = nullptr;
    };
} // namespace EMStudio
