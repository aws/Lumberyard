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

#include <AzCore/std/containers/vector.h>
#include <QtCore/QObject>
#include <MCore/Source/StandardHeaders.h>

QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QPersistentModelIndex)

namespace EMotionFX
{
    class AnimGraph;
    class AnimGraphNode;
    class AnimGraphReferenceNode;
    class MotionSet;
}

namespace EMStudio
{
    class AnimGraphPlugin;

    class AnimGraphActionManager
        : public QObject
    {
        Q_OBJECT

    public:
        AnimGraphActionManager(AnimGraphPlugin* plugin);
        ~AnimGraphActionManager();

        bool GetIsReadyForPaste() const;

        void ShowNodeColorPicker(EMotionFX::AnimGraphNode* animGraphNode);

    public slots:
        void Copy();
        void Cut();
        void Paste(const QModelIndex& parentIndex, const QPoint& pos);

        // Sets the first-selected node as an entry state
        void SetEntryState();

        // Adds a wild card transition to the first-selected node
        void AddWildCardTransition();

        void EnableSelected();
        void DisableSelected();

        void MakeVirtualFinalNode();
        void RestoreVirtualFinalNode();

        // Deletes all selected nodes
        void DeleteSelectedNodes();

        void NavigateToNode();

        void OpenReferencedAnimGraph(EMotionFX::AnimGraphReferenceNode* referenceNode);

        void ActivateGraphForSelectedActors(EMotionFX::AnimGraph* animGraph, EMotionFX::MotionSet* motionSet);

    private:
        void SetSelectedEnabled(bool enabled);

        // Copy/Cut and Paste holds some state so the user can change selection
        // while doing the operation. We will store the list of selected items
        // and the type of operation until the users paste.
        // TODO: in a future, we should use something like QClipboard so users can
        // copy/cut/paste through the application and across instances. 
        enum class PasteOperation
        {
            None,
            Copy,
            Cut
        };

        AnimGraphPlugin* m_plugin;
        AZStd::vector<QPersistentModelIndex> m_pasteItems;
        PasteOperation m_pasteOperation;
    };

} // namespace EMStudio