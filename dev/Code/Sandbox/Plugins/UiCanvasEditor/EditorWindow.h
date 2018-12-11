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

#include "EditorCommon.h"

#include "Animation/UiEditorAnimationBus.h"
#include "UiEditorDLLBus.h"
#include "UiEditorEntityContext.h"
#include <QList>
#include <QMetaObject>

#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>
#include <LyShine/Bus/UiEditorChangeNotificationBus.h>

#include <IFont.h>

class AssetTreeEntry;

class EditorWindow
    : public QMainWindow
    , public IEditorNotifyListener
    , public UiEditorDLLBus::Handler
    , public UiEditorChangeNotificationBus::Handler
    , public AzToolsFramework::AssetBrowser::AssetBrowserModelNotificationBus::Handler
    , public FontNotificationBus::Handler
{
    Q_OBJECT

public: // types

    struct UiCanvasTabMetadata
    {
        AZ::EntityId m_canvasEntityId;
    };

public: // member functions

    explicit EditorWindow(QWidget* parent = nullptr, Qt::WindowFlags flags = 0);
    virtual ~EditorWindow();

    // you are required to implement this to satisfy the unregister/registerclass requirements on "RegisterQtViewPane"
    // make sure you pick a unique GUID
    static const GUID& GetClassID()
    {
        // {E72CB9F3-DCB5-4525-AEAC-541A8CC778C5}
        static const GUID guid =
        {
            0xe72cb9f3, 0xdcb5, 0x4525, { 0xae, 0xac, 0x54, 0x1a, 0x8c, 0xc7, 0x78, 0xc5 }
        };
        return guid;
    }

    void OnEditorNotifyEvent(EEditorNotifyEvent ev) override;

    // UiEditorDLLInterface
    LyShine::EntityArray GetSelectedElements() override;
    AZ::EntityId GetActiveCanvasId() override;
    UndoStack* GetActiveUndoStack() override;
    void OpenSourceCanvasFile(QString absolutePathToFile) override;
    // ~UiEditorDLLInterface

    // UiEditorChangeNotificationBus
    void OnEditorTransformPropertiesNeedRefresh() override;
    void OnEditorPropertiesRefreshEntireTree() override;
    // ~UiEditorChangeNotificationBus

    // AssetBrowserModelNotificationBus
    void EntryAdded(const AzToolsFramework::AssetBrowser::AssetBrowserEntry* entry) override;
    void EntryRemoved(const AzToolsFramework::AssetBrowser::AssetBrowserEntry* entry) override;
    // ~AssetBrowserModelNotificationBus

    // FontNotifications
    void OnFontsReloaded() override;
    // ~FontNotifications

    AZ::EntityId GetCanvas();

    HierarchyWidget* GetHierarchy();
    ViewportWidget* GetViewport();
    PropertiesWidget* GetProperties();
    MainToolbar* GetMainToolbar();
    ModeToolbar* GetModeToolbar();
    EnterPreviewToolbar* GetEnterPreviewToolbar();
    PreviewToolbar* GetPreviewToolbar();
    NewElementToolbarSection* GetNewElementToolbarSection();
    CoordinateSystemToolbarSection* GetCoordinateSystemToolbarSection();
    CanvasSizeToolbarSection* GetCanvasSizeToolbarSection();

    bool CanExitNow();

    UndoStack* GetActiveStack();

    AssetTreeEntry* GetSliceLibraryTree();

    //! WARNING: This is a VERY slow function.
    void UpdatePrefabFiles();
    IFileUtil::FileArray& GetPrefabFiles();
    void AddPrefabFile(const QString& prefabFilename);

    //! Returns the current mode of the editor (Edit or Preview)
    UiEditorMode GetEditorMode() { return m_editorMode; }

    //! Toggle the editor mode between Edit and Preview
    void ToggleEditorMode();

    //! Get the copy of the canvas that is used in Preview mode (will return invalid entity ID if not in preview mode)
    AZ::EntityId GetPreviewModeCanvas() { return m_previewModeCanvasEntityId; }

    //! Get the preview canvas size.  (0,0) means use viewport size
    AZ::Vector2 GetPreviewCanvasSize();

    //! Set the preview canvas size. (0,0) means use viewport size
    void SetPreviewCanvasSize(AZ::Vector2 previewCanvasSize);

    void SaveEditorWindowSettings();

    UiSliceManager* GetSliceManager();

    UiEditorEntityContext* GetEntityContext();

    void ReplaceEntityContext(UiEditorEntityContext* entityContext);

    QMenu* createPopupMenu() override;
    AZ::EntityId GetCanvasForEntityContext(const AzFramework::EntityContextId& contextId);

signals:

    void EditorModeChanged(UiEditorMode mode);
    void SignalCoordinateSystemCycle();
    void SignalSnapToGridToggle();

protected:

    bool event(QEvent* ev) override;
    void keyReleaseEvent(QKeyEvent* ev) override;
    void paintEvent(QPaintEvent* paintEvent) override;
    void closeEvent(QCloseEvent* closeEvent) override;

public slots:
    void RestoreEditorWindowSettings();

private: // types

    struct UiCanvasEditState
    {
        UiCanvasEditState();

        // Viewport
        ViewportInteraction::TranslationAndScale m_canvasViewportMatrixProps;
        bool m_shouldScaleToFitOnViewportResize;
        ViewportInteraction::InteractionMode m_viewportInteractionMode;
        ViewportInteraction::CoordinateSystem m_viewportCoordinateSystem;

        // Hierarchy
        int m_hierarchyScrollValue;
        EntityHelpers::EntityIdList m_selectedElements;

        // Properties
        float m_propertiesScrollValue;

        // Animation
        UiEditorAnimationStateInterface::UiEditorAnimationEditState m_uiAnimationEditState;

        bool m_inited;
    };

    // Data for a loaded UI canvas
    struct UiCanvasMetadata
    {
        UiCanvasMetadata();
        ~UiCanvasMetadata();

        AZ::EntityId m_canvasEntityId;
        AZStd::string m_canvasSourceAssetPathname;
        AZStd::string m_canvasDisplayName;
        UiEditorEntityContext* m_entityContext;
        UndoStack* m_undoStack;
        //! Specifies whether this canvas was automatically loaded or loaded by the user
        bool m_autoLoaded;
        //! Specifies whether a canvas has been modified and saved since it was loaded/created
        bool m_canvasChangedAndSaved;
        //! State of the viewport and other panes (zoom, pan, scroll, selection, ...)
        UiCanvasEditState m_canvasEditState;
    };

private: // member functions

    QUndoGroup* GetUndoGroup();

    bool GetChangesHaveBeenMade(const UiCanvasMetadata& canvasMetadata);

    //! Return true when ok.
    //! forceAskingForFilename should only be true for "Save As...", not "Save".
    bool SaveCanvasToXml(UiCanvasMetadata& canvasMetadata, bool forceAskingForFilename);

    // Called from menu or shortcut key events
    void NewCanvas();
    void OpenCanvas(const QString& canvasFilename);
    void OpenCanvases(const QStringList& canvasFilenames);
    void CloseCanvas(AZ::EntityId canvasEntityId);
    void CloseAllCanvases();
    void CloseAllOtherCanvases(AZ::EntityId canvasEntityId);

    void LoadCanvas(const QString& canvasFilename, bool autoLoad, bool changeActiveCanvasToThis = true);
    bool CanUnloadCanvas(UiCanvasMetadata& canvasMetadata);
    void UnloadCanvas(AZ::EntityId canvasEntityId);
    void UnloadCanvases(const AZStd::vector<AZ::EntityId>& canvasEntityIds);

    bool CanChangeActiveCanvas();
    void SetActiveCanvas(AZ::EntityId canvasEntityId);

    void SaveActiveCanvasEditState();
    void RestoreActiveCanvasEditState();
    void RestoreActiveCanvasEditStatePostEvents();

    void OnCanvasTabCloseButtonPressed(int index);
    void OnCurrentCanvasTabChanged(int index);
    void OnCanvasTabContextMenuRequested(const QPoint &point);

    void UpdateActionsEnabledState();

    void RefreshEditorMenu();

    //! Check if the given toolbar should only be shown in preview mode
    bool IsPreviewModeToolbar(const QToolBar* toolBar);

    //! Check if the given dockwidget should only be shown in preview mode
    bool IsPreviewModeDockWidget(const QDockWidget* dockWidget);

    void AddMenu_File();
    void AddMenuItems_Edit(QMenu* menu);
    void AddMenu_Edit();
    void AddMenu_View();
    void AddMenu_View_LanguageSetting(QMenu* viewMenu);
    void AddMenu_Preview();
    void AddMenu_PreviewView();
    void AddMenu_Help();
    void EditorMenu_Open(QString optional_selectedFile);

    QAction* CreateSaveCanvasAction(AZ::EntityId canvasEntityId, bool forContextMenu = false);
    QAction* CreateSaveCanvasAsAction(AZ::EntityId canvasEntityId, bool forContextMenu = false);
    QAction* CreateSaveAllCanvasesAction(bool forContextMenu = false);
    QAction* CreateCloseCanvasAction(AZ::EntityId canvasEntityId, bool forContextMenu = false);
    QAction* CreateCloseAllOtherCanvasesAction(AZ::EntityId canvasEntityId, bool forContextMenu = false);
    QAction* CreateCloseAllCanvasesAction(bool forContextMenu = false);

    void SortPrefabsList();

    void SaveModeSettings(UiEditorMode mode, bool syncSettings);
    void RestoreModeSettings(UiEditorMode mode);

    void SubmitUnloadSavedCanvasMetricEvent(AZ::EntityId canvasEntityId);
    int GetCanvasMaxHierarchyDepth(const LyShine::EntityArray& childElements);

    void DeleteSliceLibraryTree();

    void DestroyCanvas(const UiCanvasMetadata& canvasMetadata);

    bool IsCanvasTabMetadataValidForTabIndex(int index);
    AZ::EntityId GetCanvasEntityIdForTabIndex(int index);
    int GetTabIndexForCanvasEntityId(AZ::EntityId canvasEntityId);
    UiCanvasMetadata* GetCanvasMetadataForTabIndex(int index);
    UiCanvasMetadata* GetCanvasMetadata(AZ::EntityId canvasEntityId);
    UiCanvasMetadata* GetActiveCanvasMetadata();

    AZStd::string GetCanvasDisplayNameFromAssetPath(const AZStd::string& canvasAssetPathname);

    void HandleCanvasDisplayNameChanged(const UiCanvasMetadata& canvasMetadata);

private slots:
    // Called when the clean state of the active undo stack changes
    void CleanChanged(bool clean);

private: // data

    QUndoGroup* m_undoGroup;

    UiSliceManager* m_sliceManager;

    QTabBar* m_canvasTabBar;
    QWidget* m_canvasTabSectionWidget;
    HierarchyWidget* m_hierarchy;
    PropertiesWrapper* m_properties;
    ViewportWidget* m_viewport;
    CUiAnimViewDialog* m_animationWidget;
    PreviewActionLog* m_previewActionLog;
    PreviewAnimationList* m_previewAnimationList;

    MainToolbar* m_mainToolbar;
    ModeToolbar* m_modeToolbar;
    EnterPreviewToolbar* m_enterPreviewToolbar;
    PreviewToolbar* m_previewToolbar;

    QDockWidget* m_hierarchyDockWidget;
    QDockWidget* m_propertiesDockWidget;
    QDockWidget* m_animationDockWidget;
    QDockWidget* m_previewActionLogDockWidget;
    QDockWidget* m_previewAnimationListDockWidget;

    UiEditorMode m_editorMode;

    //! This tree caches the folder view of all the slice assets under the slice library path
    AssetTreeEntry* m_sliceLibraryTree = nullptr;

    IFileUtil::FileArray m_prefabFiles;

    //! This is used to change the enabled state
    //! of these actions as the selection changes.
    QList<QAction*> m_actionsEnabledWithSelection;
    QAction* m_pasteAsSiblingAction;
    QAction* m_pasteAsChildAction;

    AZ::EntityId m_previewModeCanvasEntityId;

    AZ::Vector2 m_previewModeCanvasSize;

    QMetaObject::Connection m_clipboardConnection;

    //! Local copy of QSetting value of startup location of localization folder
    QString m_startupLocFolderName;

    std::map< AZ::EntityId, UiCanvasMetadata* > m_canvasMetadataMap;
    AZ::EntityId m_activeCanvasEntityId;

    int m_newCanvasCount;
};

Q_DECLARE_METATYPE(EditorWindow::UiCanvasTabMetadata);