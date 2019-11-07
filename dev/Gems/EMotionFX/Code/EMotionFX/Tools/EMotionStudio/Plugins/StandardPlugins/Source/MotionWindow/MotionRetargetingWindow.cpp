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

// include required headers
#include "MotionRetargetingWindow.h"
#include "MotionWindowPlugin.h"
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QIcon>
#include <MysticQt/Source/ButtonGroup.h>
#include "../../../../EMStudioSDK/Source/EMStudioManager.h"
#include <EMotionFX/CommandSystem/Source/MotionCommands.h>
#include <MCore/Source/Compare.h>


namespace EMStudio
{
    // constructor
    MotionRetargetingWindow::MotionRetargetingWindow(QWidget* parent, MotionWindowPlugin* motionWindowPlugin)
        : QWidget(parent)
    {
        mMotionWindowPlugin         = motionWindowPlugin;
        mMotionRetargetingButton    = nullptr;
        //mRenderMotionBindPose       = nullptr;
    }


    // destructor
    MotionRetargetingWindow::~MotionRetargetingWindow()
    {
    }


    // init after the parent dock window has been created
    void MotionRetargetingWindow::Init()
    {
        QVBoxLayout* layout = new QVBoxLayout();
        layout->setMargin(0);
        setLayout(layout);

        mMotionRetargetingButton = new QCheckBox("Use Motion Retargeting");
        layout->addWidget(mMotionRetargetingButton);
        connect(mMotionRetargetingButton, &QCheckBox::clicked, this, &MotionRetargetingWindow::UpdateMotions);

        //mRenderMotionBindPose = new QCheckBox("Render Motion Bind Pose");
        //mRenderMotionBindPose->setToolTip("Render motion bind pose of the currently selected motion for the selected actor instances");
        //mRenderMotionBindPose->setChecked(false);
        //layout->addWidget(mRenderMotionBindPose);
    }


    void MotionRetargetingWindow::UpdateMotions()
    {
        const CommandSystem::SelectionList& selection = CommandSystem::GetCommandManager()->GetCurrentSelection();

        // create our command group
        MCore::CommandGroup commandGroup("Adjust default motion instances");

        // get the number of selected motions and iterate through them
        const uint32 numMotions = selection.GetNumSelectedMotions();
        for (uint32 i = 0; i < numMotions; ++i)
        {
            MotionWindowPlugin::MotionTableEntry* entry = mMotionWindowPlugin->FindMotionEntryByID(selection.GetMotion(i)->GetID());
            if (entry == nullptr)
            {
                MCore::LogError("Cannot find motion table entry for the given motion.");
                continue;
            }

            EMotionFX::Motion*          motion              = entry->mMotion;
            EMotionFX::PlayBackInfo*    playbackInfo        = motion->GetDefaultPlayBackInfo();

            AZStd::string commandParameters;

            if (playbackInfo->mRetarget != mMotionRetargetingButton->isChecked())
            {
                commandParameters += AZStd::string::format("-retarget %s ", AZStd::to_string(mMotionRetargetingButton->isChecked()).c_str());
            }

            // in case the command parameters are empty it means nothing changed, so we can skip this command
            if (commandParameters.empty() == false)
            {
                commandGroup.AddCommandString(AZStd::string::format("AdjustDefaultPlayBackInfo -filename \"%s\" %s", motion->GetFileName(), commandParameters.c_str()).c_str());
            }
        }

        // execute the group command
        AZStd::string outResult;
        if (CommandSystem::GetCommandManager()->ExecuteCommandGroup(commandGroup, outResult) == false)
        {
            MCore::LogError(outResult.c_str());
        }
    }


    void MotionRetargetingWindow::UpdateInterface()
    {
        const CommandSystem::SelectionList& selection = CommandSystem::GetCommandManager()->GetCurrentSelection();

        // check if there actually is any motion selected
        const uint32 numSelectedMotions = selection.GetNumSelectedMotions();
        const bool isEnabled = (numSelectedMotions != 0);

        mMotionRetargetingButton->setEnabled(isEnabled);
        //mRenderMotionBindPose->setEnabled(isEnabled);

        if (isEnabled == false)
        {
            return;
        }

        // iterate through the selected motions
        for (uint32 i = 0; i < numSelectedMotions; ++i)
        {
            MotionWindowPlugin::MotionTableEntry* entry = mMotionWindowPlugin->FindMotionEntryByID(selection.GetMotion(i)->GetID());
            if (entry == nullptr)
            {
                MCore::LogWarning("Cannot find motion table entry for the given motion.");
                continue;
            }

            EMotionFX::Motion*          motion              = entry->mMotion;
            EMotionFX::PlayBackInfo*    defaultPlayBackInfo = motion->GetDefaultPlayBackInfo();

            mMotionRetargetingButton->setChecked(defaultPlayBackInfo->mRetarget);
        }
    }
} // namespace EMStudio

#include <EMotionFX/Tools/EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionRetargetingWindow.moc>