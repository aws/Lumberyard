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
#include "StdAfx.h"
#include "Resource.h"
#include "MainWindow.h"
#include "Core/LevelEditorMenuHandler.h"
#include "ShortcutDispatcher.h"
#include "LayoutWnd.h"
#include "Crates/Crates.h"
#include <AssetImporter/AssetImporterManager/AssetImporterManager.h>
#include <AssetImporter/AssetImporterManager/AssetImporterDragAndDropHandler.h>
#include "CryEdit.h"
#include "Controls/RollupBar.h"
#include "Controls/ConsoleSCB.h"
#include "AI/AIManager.h"
#include "Grid.h"
#include "ViewManager.h"
#include "EditorCoreAPI.h"
#include "CryEditDoc.h"
#include "ToolBox.h"
#include "LevelIndependentFileMan.h"
#include "GameEngine.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QMessageBox>
#include <QWidget>
#include "MainStatusBar.h"
#include "IEditorMaterialManager.h"
#include "PanelDisplayLayer.h"
#include "ToolbarCustomizationDialog.h"
#include "ToolbarManager.h"
#include "Core/QtEditorApplication.h"
#include "IEditor.h"
#include "Plugins/MaglevControlPanel/IAWSResourceManager.h"
#include "Commands/CommandManager.h"

#include <AzQtComponents/Components/Widgets/SpinBox.h>

#include <AzToolsFramework/Metrics/LyEditorMetricsBus.h>
#include <AzToolsFramework/API/EditorAnimationSystemRequestBus.h>

#include <QWidgetAction>
#include <QInputDialog>
#include <QShortcut>
#include <qkeysequence.h>
#include <QShortcutEvent>

#include "UndoDropDown.h"
#include "CVarMenu.h"

#include "KeyboardCustomizationSettings.h"
#include "CustomizeKeyboardDialog.h"
#include "QtViewPaneManager.h"
#include "Viewport.h"
#include "ViewPane.h"

#include "EditorPreferencesPageGeneral.h"
#include "SettingsManagerDialog.h"

#ifdef LY_TERRAIN_EDITOR
#include "TerrainTexture.h"
#endif //#ifdef LY_TERRAIN_EDITOR

// This is the "Sun Trajectory Tool", so it's not directly related to the rest of the Terrain Editor code above.
#include "TerrainLighting.h"

#include "TimeOfDayDialog.h"
#include "TrackView/TrackViewDialog.h"
#include "DataBaseDialog.h"
#include "ErrorReportDialog.h"
#include "Material/MaterialDialog.h"
#include "SmartObjects/SmartObjectsEditorDialog.h"
#include "LensFlareEditor/LensFlareEditor.h"
#include "DialogEditor/DialogEditorDialog.h"
#include "TimeOfDayDialog.h"
#include "AssetBrowser/AssetBrowserDialog.h"
#include "Mannequin/MannequinDialog.h"
#include "AI/AIDebugger.h"
#include "VisualLogViewer/VisualLogWnd.h"
#include "SelectObjectDlg.h"

#ifdef LY_TERRAIN_EDITOR
#include "TerrainDialog.h"
#endif //#ifdef LY_TERRAIN_EDITOR
#include "TerrainPanel.h"

#include "Dialogs/PythonScriptsDialog.h"
#include "AssetResolver/AssetResolverDialog.h"
#include "ObjectCreateTool.h"
#include "Material/MaterialManager.h"
#include "ScriptTermDialog.h"
#include "MeasurementSystem/MeasurementSystem.h"
#include "VegetationDataBasePage.h"
#include <AzCore/Math/Uuid.h>
#include "IGemManager.h"
#include "ISystem.h"
#include "Settings.h"
#include <ISourceControl.h>
#include <EngineSettingsManager.h>

#include <algorithm>
AZ_PUSH_DISABLE_WARNING(4251 4355 4996, "-Wunknown-warning-option")
#include <aws/core/auth/AWSCredentialsProvider.h>
AZ_POP_DISABLE_WARNING

#include <QDesktopServices>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QFileInfo>
#include <QWidgetAction>
#include <QSettings>

#include "AzCore/std/smart_ptr/make_shared.h"
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzQtComponents/Buses/ShortcutDispatch.h>
#include <AzQtComponents/Components/DockMainWindow.h>
#include <AzQtComponents/Components/EditorProxyStyle.h>
#include <AzQtComponents/Components/WindowDecorationWrapper.h>
#include <AzQtComponents/DragAndDrop/MainWindowDragAndDrop.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Asset/AssetSystemBus.h>
#include <AzFramework/Network/SocketConnection.h>
#include <AzFramework/Network/AssetProcessorConnection.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzToolsFramework/Application/Ticker.h>
#include <algorithm>
#include <LyMetricsProducer/LyMetricsAPI.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <AzToolsFramework/SourceControl/QtSourceControlNotificationHandler.h>

#include <AzQtComponents/Components/Titlebar.h>
#include <NetPromoterScore/DayCountManager.h>
#include <NetPromoterScore/NetPromoterScoreDialog.h>
#include "MaterialSender.h"

#include <Editor/AzAssetBrowser/AzAssetBrowserWindow.h>
#include <Editor/AssetEditor/AssetEditorWindow.h>
#include <Editor/GridSettingsDialog.h>

#include <AzCore/RTTI/BehaviorContext.h>

// uncomment this to show thumbnail demo widget
// #define ThumbnailDemo

#ifdef ThumbnailDemo
#include <Editor/Thumbnails/Example/ThumbnailsSampleWidget.h>
#endif

#include "MaterialSender.h"

#include "ActionManager.h"

using namespace AZ;
using namespace AzQtComponents;
using namespace AzToolsFramework;

#define LAYOUTS_PATH "Editor\\Layouts\\"
#define LAYOUTS_EXTENSION ".layout"
#define LAYOUTS_WILDCARD "*.layout"
#define DUMMY_LAYOUT_NAME "Dummy_Layout"

static const char* g_openViewPaneEventName = "OpenViewPaneEvent"; //Sent when users open view panes;
static const char* g_viewPaneAttributeName = "ViewPaneName"; //Name of the current view pane
static const char* g_openLocationAttributeName = "OpenLocation"; //Indicates where the current view pane is opened from

static const char* g_assetImporterName = "AssetImporter";

static const char* g_snapToGridEnabled = "mainwindow/snapGridEnabled";
static const char* g_snapToGridSize = "mainwindow/snapGridSize";
static const char* g_snapAngleEnabled = "mainwindow/snapAngleEnabled";
static const char* g_snapAngle = "mainwindow/snapAngle";

class CEditorOpenViewCommand
    : public _i_reference_target_t
{
    QString m_className;
public:
    CEditorOpenViewCommand(IEditor* pEditor, const QString& className)
        : m_pEditor(pEditor)
        , m_className(className)
    {
        assert(m_pEditor);
    }
    void Execute()
    {
        // Create browse mode for this category.
        m_pEditor->OpenView(m_className);
    }

private:
    IEditor* m_pEditor;
};

namespace
{
    // The purpose of this vector is just holding shared pointers, so CEditorOpenViewCommand dtors are called at exit
    std::vector<_smart_ptr<CEditorOpenViewCommand> > s_openViewCmds;
}

class EngineConnectionListener
    : public AzFramework::EngineConnectionEvents::Bus::Handler
    , public AzFramework::AssetSystemInfoBus::Handler
{
public:
    using EConnectionState = AzFramework::SocketConnection::EConnectionState;

public:
    EngineConnectionListener()
        : m_state(EConnectionState::Disconnected)
    {
        AzFramework::EngineConnectionEvents::Bus::Handler::BusConnect();
        AzFramework::AssetSystemInfoBus::Handler::BusConnect();

        AzFramework::SocketConnection* engineConnection = AzFramework::SocketConnection::GetInstance();
        if (engineConnection)
        {
            m_state = engineConnection->GetConnectionState();
        }
    }

    ~EngineConnectionListener()
    {
        AzFramework::AssetSystemInfoBus::Handler::BusDisconnect();
        AzFramework::EngineConnectionEvents::Bus::Handler::BusDisconnect();
    }

public:
    virtual void Connected(AzFramework::SocketConnection* connection)
    {
        m_state = EConnectionState::Connected;
    }
    virtual void Connecting(AzFramework::SocketConnection* connection)
    {
        m_state = EConnectionState::Connecting;
    }
    virtual void Listening(AzFramework::SocketConnection* connection)
    {
        m_state = EConnectionState::Listening;
    }
    virtual void Disconnecting(AzFramework::SocketConnection* connection)
    {
        m_state = EConnectionState::Disconnecting;
    }
    virtual void Disconnected(AzFramework::SocketConnection* connection)
    {
        m_state = EConnectionState::Disconnected;
    }

    virtual void AssetCompilationSuccess(const AZStd::string& assetPath) override
    {
        m_lastAssetProcessorTask = assetPath;
    }

    virtual void AssetCompilationFailed(const AZStd::string& assetPath) override
    {
        m_failedJobs.insert(assetPath);
    }

    virtual void CountOfAssetsInQueue(const int& count) override
    {
        m_pendingJobsCount = count;
    }

    int GetJobsCount() const
    {
        return m_pendingJobsCount;
    }

    AZStd::set<AZStd::string> FailedJobsList() const
    {
        return m_failedJobs;
    }

    AZStd::string LastAssetProcessorTask() const
    {
        return m_lastAssetProcessorTask;
    }

public:
    EConnectionState GetState() const
    {
        return m_state;
    }

private:
    EConnectionState m_state;
    int m_pendingJobsCount = 0;
    AZStd::set<AZStd::string> m_failedJobs;
    AZStd::string m_lastAssetProcessorTask;
};

namespace
{
    void PyOpenViewPane(const char* viewClassName)
    {
        QtViewPaneManager::instance()->OpenPane(viewClassName);
    }

    void PyCloseViewPane(const char* viewClassName)
    {
        QtViewPaneManager::instance()->ClosePane(viewClassName);
    }

    bool PyIsViewPaneVisible(const char* viewClassName)
    {
        return QtViewPaneManager::instance()->IsVisible(viewClassName);
    }

    AZStd::string PyGetStatusText()
    {
        if (GetIEditor()->GetEditTool())
        {
            return AZStd::string(GetIEditor()->GetEditTool()->GetStatusText().toUtf8().data());
        }
        return AZStd::string("");
    }

    AZStd::vector<AZStd::string> PyGetViewPaneNames()
    {
        const QtViewPanes panes = QtViewPaneManager::instance()->GetRegisteredPanes();

        AZStd::vector<AZStd::string> names;
        names.reserve(panes.size());

        AZStd::transform(panes.begin(), panes.end(), AZStd::back_inserter(names), [](const QtViewPane& pane)
        {
            return pane.m_name.toUtf8().data();
        });

        return names;
    }

    void PyExit()
    {
        // Adding a single-shot QTimer to PyExit delays the QApplication::closeAllWindows call until
        // all the events in the event queue have been processed. Calling QApplication::closeAllWindows instead
        // of MainWindow::close ensures the Metal render window is cleaned up on macOS.
        QTimer::singleShot(0, qApp, &QApplication::closeAllWindows);
    }

    void PyExitNoPrompt()
    {
        // Set the level to "unmodified" so that it doesn't prompt to save on exit.
        GetIEditor()->GetDocument()->SetModifiedFlag(false);
        PyExit();
    }
}

//////////////////////////////////////////////////////////////////////////
// Select Displayed Navigation Agent Type
//////////////////////////////////////////////////////////////////////////
class CNavigationAgentTypeMenu
    : public DynamicMenu
{
protected:
    void OnMenuChange(int id, QAction* action) override
    {
        if (id < ID_AI_NAVIGATION_SELECT_DISPLAY_AGENT_RANGE_BEGIN || id > ID_AI_NAVIGATION_SELECT_DISPLAY_AGENT_RANGE_END)
        {
            return;
        }

        const size_t newSelection = id - ID_AI_NAVIGATION_SELECT_DISPLAY_AGENT_RANGE_BEGIN;

        // Check if toggle/untoggle navigation displaying
        CAIManager* pAIMgr = GetIEditor()->GetAI();
        const bool shouldBeDisplayed = gSettings.navigationDebugAgentType != newSelection || !gSettings.bNavigationDebugDisplay;
        pAIMgr->EnableNavigationDebugDisplay(shouldBeDisplayed);
        gSettings.bNavigationDebugDisplay = pAIMgr->GetNavigationDebugDisplayState();

        gSettings.navigationDebugAgentType = newSelection;
        SetNavigationDebugDisplayAgent(newSelection);
    }
    void OnMenuUpdate(int id, QAction* action) override
    {
        if (id < ID_AI_NAVIGATION_SELECT_DISPLAY_AGENT_RANGE_BEGIN || id > ID_AI_NAVIGATION_SELECT_DISPLAY_AGENT_RANGE_END)
        {
            return;
        }
        CAIManager* pAIMgr = GetIEditor()->GetAI();
        const size_t current = id - ID_AI_NAVIGATION_SELECT_DISPLAY_AGENT_RANGE_BEGIN;
        const bool shouldTheItemBeChecked = (current == gSettings.navigationDebugAgentType) && pAIMgr->GetNavigationDebugDisplayState();
        action->setChecked(shouldTheItemBeChecked);
    }
    void CreateMenu() override
    {
        CAIManager* manager = GetIEditor()->GetAI();

        const size_t agentTypeCount = manager->GetNavigationAgentTypeCount();

        for (size_t i = 0; i < agentTypeCount; ++i)
        {
            const char* name = manager->GetNavigationAgentTypeName(i);
            AddAction(ID_AI_NAVIGATION_SELECT_DISPLAY_AGENT_RANGE_BEGIN + i, QString(name)).SetCheckable(true);
        }
    }

private:
    void SetNavigationDebugDisplayAgent(int nId)
    {
        CAIManager* manager = GetIEditor()->GetAI();
        manager->SetNavigationDebugDisplayAgentType(nId);
    }
};

class SnapToWidget
    : public QWidget
    , public CGridSettingsDialog::NotificationBus::Handler
{
public:

    typedef AZStd::function<void(double)> SetValueCallback;
    typedef AZStd::function<double()> GetValueCallback;

    SnapToWidget(QAction* defaultAction, SetValueCallback setValueCallback, GetValueCallback getValueCallback)
        : m_setValueCallback(setValueCallback)
        , m_getValueCallback(getValueCallback)
    {
        QHBoxLayout* layout = new QHBoxLayout();
        setLayout(layout);

        m_toolButton = new QToolButton();
        m_toolButton->setAutoRaise(true);
        m_toolButton->setCheckable(false);
        m_toolButton->setDefaultAction(defaultAction);

        m_spinBox = new AzQtComponents::DoubleSpinBox();

        layout->addWidget(m_toolButton);
        layout->addWidget(m_spinBox);

        m_spinBox->setEnabled(defaultAction->isChecked());
        m_spinBox->setMinimum(1e-2f);

        OnGridValuesUpdated();

        QObject::connect(m_spinBox, QOverload<double>::of(&AzQtComponents::DoubleSpinBox::valueChanged), this, &SnapToWidget::OnValueChanged);
        QObject::connect(defaultAction, &QAction::changed, this, &SnapToWidget::OnActionChanged);

        CGridSettingsDialog::NotificationBus::Handler::BusConnect();
    }

    void SetIcon(QIcon icon)
    {
        m_toolButton->setIcon(icon);
    }

    void OnGridValuesUpdated() override
    {
        // Blocking signals to not trigger the valueChanged callback when we set the value on the spin box.
        QSignalBlocker signalBlocker(m_spinBox);
        double value = m_getValueCallback();
        m_spinBox->setValue(value);
    }

protected:

    void OnValueChanged(double value)
    {
        m_setValueCallback(value);
    }

    void OnActionChanged()
    {
        m_spinBox->setEnabled(m_toolButton->isChecked());
    }

private:

    QToolButton* m_toolButton = nullptr;
    AzQtComponents::DoubleSpinBox* m_spinBox = nullptr;

    SetValueCallback m_setValueCallback;
    GetValueCallback m_getValueCallback;
};

/////////////////////////////////////////////////////////////////////////////
// MainWindow
/////////////////////////////////////////////////////////////////////////////
MainWindow* MainWindow::m_instance = nullptr;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_oldMainFrame(nullptr)
    , m_viewPaneManager(QtViewPaneManager::instance())
    , m_shortcutDispatcher(new ShortcutDispatcher(this))
    , m_actionManager(new ActionManager(this, QtViewPaneManager::instance(), m_shortcutDispatcher))
    , m_undoStateAdapter(new UndoStackStateAdapter(this))
    , m_keyboardCustomization(nullptr)
    , m_activeView(nullptr)
    , m_settings("amazon", "lumberyard") // TODO_KDAB: Replace with a central settings class
    , m_NetPromoterScoreDialog(new NetPromoterScoreDialog(this))
    , m_dayCountManager(new DayCountManager(this))
    , m_toolbarManager(new ToolbarManager(m_actionManager, this))
    , m_assetImporterManager(new AssetImporterManager(this))
    , m_levelEditorMenuHandler(new LevelEditorMenuHandler(this, m_viewPaneManager, m_settings))
    , m_sourceControlNotifHandler(new AzToolsFramework::QtSourceControlNotificationHandler(this))
    , m_enableLegacyCryEntities(false)
    , m_viewPaneHost(nullptr)
    , m_autoSaveTimer(nullptr)
    , m_autoRemindTimer(nullptr)
    , m_backgroundUpdateTimer(nullptr)
    , m_connectionLostTimer(new QTimer(this))
{
    setObjectName("MainWindow"); // For IEditor::GetEditorMainWindow to work in plugins, where we can't link against MainWindow::instance()
    m_instance = this;

    //for new docking, create a DockMainWindow to host dock widgets so we can call QMainWindow::restoreState to restore docks without affecting our main toolbars.
    m_viewPaneHost = new AzQtComponents::DockMainWindow();

    m_viewPaneHost->setDockOptions(QMainWindow::GroupedDragging | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

    m_connectionListener = AZStd::make_shared<EngineConnectionListener>();
    QObject::connect(m_connectionLostTimer, &QTimer::timeout, this, &MainWindow::ShowConnectionDisconnectedDialog);

    setStatusBar(new MainStatusBar(this));

    setAttribute(Qt::WA_DeleteOnClose, true);

    connect(m_viewPaneManager, &QtViewPaneManager::viewPaneCreated, this, &MainWindow::OnViewPaneCreated);
    GetIEditor()->RegisterNotifyListener(this);

    AssetImporterDragAndDropHandler* assetImporterDragAndDropHandler = new AssetImporterDragAndDropHandler(this, m_assetImporterManager);
    connect(assetImporterDragAndDropHandler, &AssetImporterDragAndDropHandler::OpenAssetImporterManager, this, &MainWindow::OnOpenAssetImporterManager);

    connect(m_levelEditorMenuHandler, &LevelEditorMenuHandler::ActivateAssetImporter, this, [this]() {
        m_assetImporterManager->Exec();
    });

    setFocusPolicy(Qt::StrongFocus);

    connect(m_actionManager, &ActionManager::SendMetricsSignal, this, &MainWindow::SendMetricsEvent);

    connect(m_NetPromoterScoreDialog, &NetPromoterScoreDialog::UserInteractionCompleted, m_dayCountManager, &DayCountManager::OnUpdatePreviousUsedData);
    setAcceptDrops(true);

#ifdef Q_OS_WIN
    if (auto aed = QAbstractEventDispatcher::instance())
    {
        aed->installNativeEventFilter(this);
    }
#endif

    // special handling for escape key (outside ActionManager)
    auto* escapeAction = new QAction(this);
    escapeAction->setShortcut(QKeySequence(Qt::Key_Escape));
    addAction(escapeAction);
    connect(escapeAction, &QAction::triggered, this, &MainWindow::OnEscapeAction);

    const QSize minSize(800, 600);
    if (size().height() < minSize.height() || size().width() < minSize.width())
    {
        resize(size().expandedTo(minSize));
    }
}

void MainWindow::SystemTick()
{
    AZ::ComponentApplication* componentApplication = nullptr;
    EBUS_EVENT_RESULT(componentApplication, AZ::ComponentApplicationBus, GetApplication);
    if (componentApplication)
    {
        componentApplication->TickSystem();
    }
}

#ifdef Q_OS_WIN
HWND MainWindow::GetNativeHandle()
{
    // if the parent widget is set, it's a window decoration wrapper
    // we use that instead, to ensure we're in lock step the code in CryEdit.cpp when it calls
    // InitGameSystem
    if (parentWidget() != nullptr)
    {
        assert(qobject_cast<AzQtComponents::WindowDecorationWrapper*>(parentWidget()));
        return QtUtil::getNativeHandle(parentWidget());
    }

    return QtUtil::getNativeHandle(this);
}
#endif // #ifdef Q_OS_WIN

void MainWindow::SendMetricsEvent(const char* viewPaneName, const char* openLocation)
{
    //Send metrics event to check how many times the pane is open via main menu View->Open View Pane
    auto eventId = LyMetrics_CreateEvent(g_openViewPaneEventName);

    // Add attribute to show what pane is opened
    LyMetrics_AddAttribute(eventId, g_viewPaneAttributeName, viewPaneName);

    // Add attribute to tell where this pane is opened from
    LyMetrics_AddAttribute(eventId, g_openLocationAttributeName, openLocation);

    LyMetrics_SubmitEvent(eventId);
}

void MainWindow::OnOpenAssetImporterManager(const QStringList& dragAndDropFileList)
{
    // Asset Importer metrics event: open the Asset Importer by dragging and dropping files/folders from local directories to the Editor
    EditorMetricsEventsBus::Broadcast(&EditorMetricsEventsBus::Events::MenuTriggered, g_assetImporterName, MetricsActionTriggerType::DragAndDrop);
    m_assetImporterManager->Exec(dragAndDropFileList);
}

CLayoutWnd* MainWindow::GetLayout() const
{
    return m_pLayoutWnd;
}

CLayoutViewPane* MainWindow::GetActiveView() const
{
    return m_activeView;
}

QtViewport* MainWindow::GetActiveViewport() const
{
    return m_activeView ? qobject_cast<QtViewport*>(m_activeView->GetViewport()) : nullptr;
}

void MainWindow::SetActiveView(CLayoutViewPane* v)
{
    m_activeView = v;
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    if (auto aed = QAbstractEventDispatcher::instance())
    {
        aed->removeNativeEventFilter(this);
    }
#endif

    AzToolsFramework::SourceControlNotificationBus::Handler::BusDisconnect();

    delete m_toolbarManager;
    m_connectionListener.reset();
    GetIEditor()->UnregisterNotifyListener(this);

    // tear down the ActionOverride (clear the overrideWidget's parent)
    ActionOverrideRequestBus::Event(
        GetEntityContextId(), &ActionOverrideRequests::TeardownActionOverrideHandler);

    m_instance = nullptr;
}

void MainWindow::InitCentralWidget()
{
    m_pLayoutWnd = new CLayoutWnd(&m_settings);

    // Set the central widgets before calling CreateLayout to avoid reparenting everything later
    setCentralWidget(m_viewPaneHost);
    m_viewPaneHost->setCentralWidget(m_pLayoutWnd);

    if (MainWindow::instance()->IsPreview())
    {
        m_pLayoutWnd->CreateLayout(ET_Layout0, true, ET_ViewportModel);
    }
    else
    {
        if (!m_pLayoutWnd->LoadConfig())
        {
            m_pLayoutWnd->CreateLayout(ET_Layout0);
        }
    }

    // make sure the layout wnd knows to reset it's layout and settings
    connect(m_viewPaneManager, &QtViewPaneManager::layoutReset, m_pLayoutWnd, &CLayoutWnd::ResetLayout);
}

void MainWindow::Initialize()
{
    m_enableLegacyCryEntities = GetIEditor()->IsLegacyUIEnabled();
    m_viewPaneManager->SetMainWindow(m_viewPaneHost, &m_settings, /*unused*/ QByteArray(), m_enableLegacyCryEntities);

    RegisterStdViewClasses();
    InitCentralWidget();

    LoadConfig();
    InitActions();

    // load toolbars ("shelves") and macros
    GetIEditor()->GetToolBoxManager()->Load(m_actionManager);

    InitToolActionHandlers();

    m_levelEditorMenuHandler->Initialize();

    InitToolBars();
    InitStatusBar();

    AzToolsFramework::SourceControlNotificationBus::Handler::BusConnect();
    m_sourceControlNotifHandler->Init();

    m_keyboardCustomization = new KeyboardCustomizationSettings(QStringLiteral("Main Window"), this);

    if (!IsPreview())
    {
        RegisterOpenWndCommands();
    }

    ResetBackgroundUpdateTimer();

    ICVar* pBackgroundUpdatePeriod = gEnv->pConsole->GetCVar("ed_backgroundUpdatePeriod");
    if (pBackgroundUpdatePeriod)
    {
        pBackgroundUpdatePeriod->SetOnChangeCallback([](ICVar*) {
            MainWindow::instance()->ResetBackgroundUpdateTimer();
        });
    }

    AzFramework::ApplicationRequests::Bus::BroadcastResult(
        m_projectExternal, &AzFramework::ApplicationRequests::IsEngineExternal);

    // setup the ActionOverride (set overrideWidgets parent to be the MainWindow)
    ActionOverrideRequestBus::Event(
        GetEntityContextId(), &ActionOverrideRequests::SetupActionOverrideHandler, this);

    // This function only happens after we're pretty sure that the engine has successfully started - so now would be a good time to start ticking the message pumps/etc.
    AzToolsFramework::Ticker* ticker = new AzToolsFramework::Ticker(this);
    ticker->Start();
    connect(ticker, &AzToolsFramework::Ticker::Tick, this, &MainWindow::SystemTick);
}

void MainWindow::InitStatusBar()
{
    StatusBar()->Init();
    connect(qobject_cast<StatusBarItem*>(StatusBar()->GetItem("connection")), &StatusBarItem::clicked, this, &MainWindow::OnConnectionStatusClicked);
    connect(StatusBar(), &MainStatusBar::requestStatusUpdate, this, &MainWindow::OnUpdateConnectionStatus);
}

CMainFrame* MainWindow::GetOldMainFrame() const
{
    return m_oldMainFrame;
}

MainWindow* MainWindow::instance()
{
    return m_instance;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    auto cryEdit = CCryEditApp::instance();
    // Don't show the net promoter when running in batch mode.
    if (!cryEdit || !cryEdit->IsInConsoleMode() || !cryEdit->IsInTestMode() || !cryEdit->IsInAutotestMode())
    {
        if (m_dayCountManager->ShouldShowNetPromoterScoreDialog())
        {
            m_NetPromoterScoreDialog->SetRatingInterval(m_dayCountManager->GetRatingInterval());
            m_NetPromoterScoreDialog->exec();
        }
    }

    AzFramework::SystemCursorState currentCursorState;
    bool isInGameMode = false;
    if (GetIEditor()->IsInGameMode())
    {
        isInGameMode = true;
        // Storecurrent state in case we need to restore Game Mode.
        AzFramework::InputSystemCursorRequestBus::EventResult(currentCursorState, AzFramework::InputDeviceMouse::Id,
            &AzFramework::InputSystemCursorRequests::GetSystemCursorState);
        // make sure the mouse is turned on before popping up any dialog boxes.
        AzFramework::InputSystemCursorRequestBus::Event(AzFramework::InputDeviceMouse::Id,
            &AzFramework::InputSystemCursorRequests::SetSystemCursorState,
            AzFramework::SystemCursorState::UnconstrainedAndVisible);
    }
    if (GetIEditor()->GetDocument() && !GetIEditor()->GetDocument()->CanCloseFrame())
    {
        if (isInGameMode)
        {
            // make sure the mouse is turned back off if returning to the game.
            AzFramework::InputSystemCursorRequestBus::Event(AzFramework::InputDeviceMouse::Id,
                &AzFramework::InputSystemCursorRequests::SetSystemCursorState,
                currentCursorState);
        }
        event->ignore();
        return;
    }

    KeyboardCustomizationSettings::EnableShortcutsGlobally(true);
    SaveConfig();

    // Some of the panes may ask for confirmation to save changes before closing.
    if (!QtViewPaneManager::instance()->ClosePanesWithRollback(QVector<QString>()) ||
        !GetIEditor() ||
        !GetIEditor()->GetLevelIndependentFileMan()->PromptChangedFiles())
    {
        if (isInGameMode)
        {
            // make sure the mouse is turned back off if returning to the game.
            AzFramework::InputSystemCursorRequestBus::Event(AzFramework::InputDeviceMouse::Id,
                &AzFramework::InputSystemCursorRequests::SetSystemCursorState,
                currentCursorState);

        }
        event->ignore();
        return;
    }

    Editor::EditorQtApplication::instance()->EnableOnIdle(false);

    if (GetIEditor()->GetDocument())
    {
        GetIEditor()->GetDocument()->SetModifiedFlag(FALSE);
        GetIEditor()->GetDocument()->SetModifiedModules(eModifiedNothing);
    }
    // Close all edit panels.
    GetIEditor()->ClearSelection();
    GetIEditor()->SetEditTool(0);
    GetIEditor()->GetObjectManager()->EndEditParams();

    // force clean up of all deferred deletes, so that we don't have any issues with windows from plugins not being deleted yet
    qApp->sendPostedEvents(0, QEvent::DeferredDelete);

    QMainWindow::closeEvent(event);
}

void MainWindow::LoadConfig()
{
    CGrid* grid = gSettings.pGrid;
    Q_ASSERT(grid);

    ReadConfigValue(g_snapAngleEnabled, grid->bAngleSnapEnabled);
    ReadConfigValue(g_snapAngle, grid->angleSnap);
    ReadConfigValue(g_snapToGridEnabled, grid->bEnabled);
    ReadConfigValue(g_snapToGridSize, grid->size);
}

void MainWindow::SaveConfig()
{
    CGrid* grid = gSettings.pGrid;
    Q_ASSERT(grid);

    m_settings.setValue(g_snapAngleEnabled, grid->bAngleSnapEnabled);
    m_settings.setValue(g_snapAngle, grid->angleSnap);
    m_settings.setValue(g_snapToGridEnabled, grid->bEnabled);
    m_settings.setValue(g_snapToGridSize, grid->size);

    m_settings.setValue("mainWindowState", saveState());
    QtViewPaneManager::instance()->SaveLayout();
    if (m_pLayoutWnd)
    {
        m_pLayoutWnd->SaveConfig();
    }
    GetIEditor()->GetToolBoxManager()->Save();
}

void MainWindow::ShowKeyboardCustomization()
{
    CustomizeKeyboardDialog dialog(*m_keyboardCustomization, this);
    dialog.exec();
}

void MainWindow::ExportKeyboardShortcuts()
{
    KeyboardCustomizationSettings::ExportToFile(this);
}

void MainWindow::ImportKeyboardShortcuts()
{
    KeyboardCustomizationSettings::ImportFromFile(this);
    KeyboardCustomizationSettings::SaveGlobally();
}

void MainWindow::InitActions()
{
    auto am = m_actionManager;
    auto cryEdit = CCryEditApp::instance();
    cryEdit->RegisterActionHandlers();

    am->AddAction(ID_TOOLBAR_SEPARATOR, QString());

    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_TOOLBAR_WIDGET_SELECTION_MASK, QString());
    }

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
       am->AddAction(ID_TOOLBAR_WIDGET_REF_COORD, QString());
    }

    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_TOOLBAR_WIDGET_SELECT_OBJECT, QString());
    }

    am->AddAction(ID_TOOLBAR_WIDGET_UNDO, QString());
    am->AddAction(ID_TOOLBAR_WIDGET_REDO, QString());
    am->AddAction(ID_TOOLBAR_WIDGET_SNAP_ANGLE, QString());
    am->AddAction(ID_TOOLBAR_WIDGET_SNAP_GRID, QString());
    am->AddAction(ID_TOOLBAR_WIDGET_ENVIRONMENT_MODE, QString());
    am->AddAction(ID_TOOLBAR_WIDGET_DEBUG_MODE, QString());

    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_TOOLBAR_WIDGET_LAYER_SELECT, QString());
    }

    // File actions
    am->AddAction(ID_FILE_NEW, tr("New Level"))
        .SetShortcut(tr("Ctrl+N"))
        .Connect(&QAction::triggered, [cryEdit]()
        {
            cryEdit->OnCreateLevel();
        })
        .SetMetricsIdentifier("MainEditor", "NewLevel")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateNewLevel);
    am->AddAction(ID_FILE_OPEN_LEVEL, tr("Open Level..."))
        .SetShortcut(tr("Ctrl+O"))
        .SetMetricsIdentifier("MainEditor", "OpenLevel")
        .SetStatusTip(tr("Open an existing level"))
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateFileOpen);
    am->AddAction(ID_FILE_RELOAD_LEVEL, tr("Reload Level"))
        .SetShortcut(tr("Ctrl+R"))
        .SetMetricsIdentifier("MainEditor", "ReloadLevel")
        .SetStatusTip(tr("Reload the current level"))
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateReloadLevel);
#ifdef ENABLE_SLICE_EDITOR
    am->AddAction(ID_FILE_NEW_SLICE, tr("New Slice"))
        .SetMetricsIdentifier("MainEditor", "NewSlice")
        .SetStatusTip(tr("Create a new slice"));
    am->AddAction(ID_FILE_OPEN_SLICE, tr("Open Slice..."))
        .SetMetricsIdentifier("MainEditor", "OpenSlice")
        .SetStatusTip(tr("Open an existing slice"));
#endif
    am->AddAction(ID_FILE_SAVE_SELECTED_SLICE, tr("Save selected slice")).SetShortcut(tr("Alt+S"))
        .SetMetricsIdentifier("MainEditor", "SaveSliceToFirstLevelRoot")
        .SetStatusTip(tr("Save the selected slice to the first level root"));
    am->AddAction(ID_FILE_SAVE_SLICE_TO_ROOT, tr("Save Slice to root")).SetShortcut(tr("Ctrl+Alt+S"))
        .SetMetricsIdentifier("MainEditor", "SaveSliceToTopLevelRoot")
        .SetStatusTip(tr("Save the selected slice to the top level root"));
    am->AddAction(ID_FILE_SAVE_LEVEL, tr("&Save"))
        .SetShortcut(tr("Ctrl+S"))
        .SetReserved()
        .SetStatusTip(tr("Save the current level"))
        .SetMetricsIdentifier("MainEditor", "SaveLevel")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateDocumentReady);
    am->AddAction(ID_FILE_SAVE_AS, tr("Save &As..."))
        .SetShortcut(tr("Ctrl+Shift+S"))
        .SetReserved()
        .SetStatusTip(tr("Save the active document with a new name"))
        .SetMetricsIdentifier("MainEditor", "SaveLevelAs")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateDocumentReady);
    am->AddAction(ID_PANEL_LAYERS_SAVE_EXTERNAL_LAYERS, tr("Save Modified External Layers"))
        .SetStatusTip(tr("Save All Modified External Layers"))
        .SetMetricsIdentifier("MainEditor", "SaveModifiedExternalLayers")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateDocumentReady);
    am->AddAction(ID_CONVERT_LEGACY_ENTITIES, tr("Upgrade Legacy Entities..."))
        .SetStatusTip(tr("Attempt to convert legacy CryEntities to Component Entities instead"))
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateDocumentReady);
    am->AddAction(ID_FILE_SAVELEVELRESOURCES, tr("Save Level Resources..."))
        .SetStatusTip(tr("Save Resources"))
        .SetMetricsIdentifier("MainEditor", "SaveLevelResources")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateDocumentReady);
    am->AddAction(ID_IMPORT_ASSET, tr("Import &FBX..."))
        .SetMetricsIdentifier("MainEditor", "FileMenuImportFBX");
    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_SELECTION_LOAD, tr("&Load Object(s)..."))
            .SetIcon(EditorProxyStyle::icon("Load"))
        .SetApplyHoverEffect()
            .SetShortcut(tr("Shift+Ctrl+L"))
            .SetMetricsIdentifier("MainEditor", "LoadObjects")
            .SetStatusTip(tr("Load Objects"));
        am->AddAction(ID_SELECTION_SAVE, tr("&Save Object(s)..."))
            .SetIcon(EditorProxyStyle::icon("Save"))
        .SetApplyHoverEffect()
            .SetStatusTip(tr("Save Selected Objects"))
            .SetMetricsIdentifier("MainEditor", "SaveSelectedObjects")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    }
    am->AddAction(ID_PROJECT_CONFIGURATOR_PROJECTSELECTION, tr("Switch Projects"))
        .SetMetricsIdentifier("MainEditor", "SwitchGems");
    am->AddAction(ID_PROJECT_CONFIGURATOR_GEMS, tr("Gems"))
        .SetMetricsIdentifier("MainEditor", "ConfigureGems");
    am->AddAction(ID_FILE_EXPORTTOGAMENOSURFACETEXTURE, tr("&Export to Engine"))
        .SetShortcut(tr("Ctrl+E"))
        .SetMetricsIdentifier("MainEditor", "ExpotToEngine")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateDocumentReady);
    am->AddAction(ID_FILE_EXPORT_SELECTEDOBJECTS, tr("Export Selected &Objects"))
        .SetMetricsIdentifier("MainEditor", "ExportSelectedObjects")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_FILE_EXPORTOCCLUSIONMESH, tr("Export Occlusion Mesh"))
        .SetMetricsIdentifier("MainEditor", "ExportOcclusionMesh");
    am->AddAction(ID_FILE_EDITLOGFILE, tr("Show Log File"))
        .SetMetricsIdentifier("MainEditor", "ShowLogFile");
    am->AddAction(ID_FILE_RESAVESLICES, tr("Resave All Slices"))
        .SetMetricsIdentifier("MainEditor", "ResaveSlices");
    am->AddAction(ID_GAME_PC_ENABLEVERYHIGHSPEC, tr("Very High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecPCVeryHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_PC_ENABLEHIGHSPEC, tr("High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecPCHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_PC_ENABLEMEDIUMSPEC, tr("Medium")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecPCMedium")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_PC_ENABLELOWSPEC, tr("Low")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecPCLow")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_OSXMETAL_ENABLEVERYHIGHSPEC, tr("Very High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecOSXMetalVeryHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_OSXMETAL_ENABLEHIGHSPEC, tr("High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecOSXMetalHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_OSXMETAL_ENABLEMEDIUMSPEC, tr("Medium")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecOSXMetalMedium")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_OSXMETAL_ENABLELOWSPEC, tr("Low")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecOSXMetalLow")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_ANDROID_ENABLEVERYHIGHSPEC, tr("Very High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecAndroidVeryHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_ANDROID_ENABLEHIGHSPEC, tr("High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecAndroidHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_ANDROID_ENABLEMEDIUMSPEC, tr("Medium")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecAndroidMedium")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_ANDROID_ENABLELOWSPEC, tr("Low")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecAndroidLow")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_IOS_ENABLEVERYHIGHSPEC, tr("Very High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecIosVeryHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_IOS_ENABLEHIGHSPEC, tr("High")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecIosHigh")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_IOS_ENABLEMEDIUMSPEC, tr("Medium")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecIosMedium")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_GAME_IOS_ENABLELOWSPEC, tr("Low")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecIosLow")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
#if defined(AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS)
#if defined(TOOLS_SUPPORT_XENIA)
    #include "Xenia/MainWindow_cpp_xenia.inl"
#endif
#if defined(TOOLS_SUPPORT_PROVO)
    #include "Provo/MainWindow_cpp_provo.inl"
#endif
#if defined(TOOLS_SUPPORT_SALEM)
    #include "Salem/MainWindow_cpp_salem.inl"
#endif
#endif
    am->AddAction(ID_GAME_APPLETV_ENABLESPEC, tr("Apple TV")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SetSpecAppleTV")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGameSpec);
    am->AddAction(ID_TOOLS_CUSTOMIZEKEYBOARD, tr("Customize &Keyboard..."))
        .SetMetricsIdentifier("MainEditor", "CustomizeKeyboard")
        .Connect(&QAction::triggered, this, &MainWindow::ShowKeyboardCustomization);
    am->AddAction(ID_TOOLS_EXPORT_SHORTCUTS, tr("&Export Keyboard Settings..."))
        .SetMetricsIdentifier("MainEditor", "ExportKeyboardShortcuts")
        .Connect(&QAction::triggered, this, &MainWindow::ExportKeyboardShortcuts);
    am->AddAction(ID_TOOLS_IMPORT_SHORTCUTS, tr("&Import Keyboard Settings..."))
        .SetMetricsIdentifier("MainEditor", "ImportKeyboardShortcuts")
        .Connect(&QAction::triggered, this, &MainWindow::ImportKeyboardShortcuts);
    am->AddAction(ID_TOOLS_PREFERENCES, tr("Global Preferences..."))
        .SetMetricsIdentifier("MainEditor", "ModifyGlobalSettings");
    am->AddAction(ID_GRAPHICS_SETTINGS, tr("&Graphics Settings..."))
        .SetMetricsIdentifier("MainEditor", "ModifyGraphicsSettings");

    for (int i = ID_FILE_MRU_FIRST; i <= ID_FILE_MRU_LAST; ++i)
    {
        am->AddAction(i, QString());
    }

#if AZ_TRAIT_OS_PLATFORM_APPLE
    const QString appExitText = tr("&Quit");
#else
    const QString appExitText = tr("E&xit");
#endif

    am->AddAction(ID_APP_EXIT, appExitText)
        .SetMetricsIdentifier("MainEditor", "Exit")
        .SetReserved();

    // Edit actions
    am->AddAction(ID_UNDO, tr("&Undo"))
        .SetShortcut(QKeySequence::Undo)
        .SetReserved()
        .SetStatusTip(tr("Undo last operation"))
        //.SetMenu(new QMenu("FIXME"))
        .SetIcon(EditorProxyStyle::icon("undo"))
        .SetApplyHoverEffect()
        .SetMetricsIdentifier("MainEditor", "Undo")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateUndo);
    am->AddAction(ID_REDO, tr("&Redo"))
        .SetShortcut(AzQtComponents::RedoKeySequence)
        .SetReserved()
        //.SetMenu(new QMenu("FIXME"))
        .SetIcon(EditorProxyStyle::icon("Redo"))
        .SetApplyHoverEffect()
        .SetStatusTip(tr("Redo last undo operation"))
        .SetMetricsIdentifier("MainEditor", "Redo")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateRedo);
    // Not quite ready to implement these globally. Need to properly respond to selection changes and clipboard changes first.
    // And figure out if these will cause problems with Cut/Copy/Paste shortcuts in the sub-editors (Particle Editor / UI Editor / etc).
    // am->AddAction(ID_EDIT_CUT, tr("Cut"))
    //     .SetShortcut(QKeySequence::Cut)
    //     .SetStatusTip(tr("Cut the current selection to the clipboard"))
    //     .SetMetricsIdentifier("MainEditor", "Cut");
    // am->AddAction(ID_EDIT_COPY, tr("Copy"))
    //     .SetShortcut(QKeySequence::Copy)
    //     .SetStatusTip(tr("Copy the current selection to the clipboard"))
    //     .SetMetricsIdentifier("MainEditor", "Copy");
    // am->AddAction(ID_EDIT_PASTE, tr("Paste"))
    //     .SetShortcut(QKeySequence::Paste)
    //     .SetStatusTip(tr("Paste the contents of the clipboard"))
    //     .SetMetricsIdentifier("MainEditor", "Paste");

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_EDIT_SELECTALL, tr("Select &All"))
            .SetShortcut(tr("Ctrl+A"))
            .SetMetricsIdentifier("MainEditor", "SelectObjectsAll")
            .SetStatusTip(tr("Select all objects"))
            ->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        am->AddAction(ID_EDIT_SELECTNONE, tr("Deselect All"))
            .SetShortcut(tr("Ctrl+Shift+D"))
            .SetMetricsIdentifier("MainEditor", "SelectObjectsNone")
            .SetStatusTip(tr("Remove selection from all objects"));
        am->AddAction(ID_EDIT_INVERTSELECTION, tr("&Invert Selection"))
            .SetMetricsIdentifier("MainEditor", "InvertObjectSelection")
            .SetShortcut(tr("Ctrl+Shift+I"));
    }

    am->AddAction(ID_SELECT_OBJECT, tr("&Object(s)..."))
        .SetIcon(EditorProxyStyle::icon("Object_list"))
        .SetApplyHoverEffect()
        .SetMetricsIdentifier("MainEditor", "SelectObjectsDialog")
        .SetStatusTip(tr("Select object(s)"));

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_LOCK_SELECTION, tr("Lock Selection"))
            .SetShortcut(tr("Ctrl+Shift+Space"))
            .SetMetricsIdentifier("MainEditor", "LockObjectSelection")
            .SetStatusTip(tr("Lock Current Selection."));
    }

    am->AddAction(ID_EDIT_NEXTSELECTIONMASK, tr("Next Selection Mask"))
        .SetMetricsIdentifier("MainEditor", "NextObjectSelectionMask");

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        // implemented by EditorTransformComponentSelection when the new Viewport Interaction Model is enabled
        am->AddAction(ID_EDIT_HIDE, tr("Hide Selection"))
            .SetShortcut(tr("H"))
            .SetStatusTip(tr("Hide selected object(s)."))
            .SetMetricsIdentifier("MainEditor", "HideSelectedObjects")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditHide);
        am->AddAction(ID_EDIT_UNHIDEALL, tr("Unhide All"))
            .SetShortcut(tr("Ctrl+H"))
            .SetMetricsIdentifier("MainEditor", "UnhideAllObjects")
            .SetStatusTip(tr("Unhide all hidden objects."));
    }

    am->AddAction(ID_EDIT_SHOW_LAST_HIDDEN, tr("Show Last Hidden"))
        .SetShortcut(tr("Shift+H"))
        .SetMetricsIdentifier("MainEditor", "ShowLastHiddenObject")
        .SetStatusTip(tr("Show last hidden object."));

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        if (m_enableLegacyCryEntities)
        {
            am->AddAction(ID_MODIFY_LINK, tr("Link"))
                .SetMetricsIdentifier("MainEditor", "LinkSelectedObjects");
            am->AddAction(ID_MODIFY_UNLINK, tr("Unlink"))
                .SetMetricsIdentifier("MainEditor", "UnlinkSelectedObjects");
        }
        else
        {
            am->AddAction(ID_MODIFY_LINK, tr("Parent"))
                .SetMetricsIdentifier("MainEditor", "LinkSelectedObjects");
            am->AddAction(ID_MODIFY_UNLINK, tr("Un-Parent"))
                .SetMetricsIdentifier("MainEditor", "UnlinkSelectedObjects");
        }
    }

    am->AddAction(ID_GROUP_MAKE, tr("&Group"))
        .SetStatusTip(tr("Make Group from selected objects."))
        .SetMetricsIdentifier("MainEditor", "GroupSelectedObjects")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGroupMake);
    am->AddAction(ID_GROUP_UNGROUP, tr("&Ungroup"))
        .SetMetricsIdentifier("MainEditor", "UngroupSelectedObjects")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGroupUngroup);
    am->AddAction(ID_GROUP_OPEN, tr("&Open Group"))
        .SetStatusTip(tr("Open selected Group."))
        .SetMetricsIdentifier("MainEditor", "OpenSelectedObjectGroup")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGroupOpen);
    am->AddAction(ID_GROUP_CLOSE, tr("&Close Group"))
        .SetStatusTip(tr("Close selected Group."))
        .SetMetricsIdentifier("MainEditor", "CloseSelectedObjectGroup")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGroupClose);
    am->AddAction(ID_GROUP_ATTACH, tr("&Attach to Group"))
        .SetStatusTip(tr("Attach object to Group."))
        .SetMetricsIdentifier("MainEditor", "AttachSelectedObjectsToGroup")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGroupAttach);
    am->AddAction(ID_GROUP_DETACH, tr("&Detach From Group"))
        .SetMetricsIdentifier("MainEditor", "DetachSelectedFromGroup")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGroupDetach);

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        // implemented by EditorTransformComponentSelection when the new Viewport Interaction Model is enabled
        am->AddAction(ID_EDIT_FREEZE, tr("Lock selection"))
            .SetShortcut(tr("L"))
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditFreeze)
            .SetMetricsIdentifier("MainEditor", "FreezeSelectedObjects")
            .SetIcon(EditorProxyStyle::icon("Locked"))
            .SetApplyHoverEffect();
        am->AddAction(ID_EDIT_UNFREEZEALL, tr("Unlock all"))
            .SetShortcut(tr("Ctrl+L"))
            .SetIcon(EditorProxyStyle::icon("Unlocked"))
            .SetApplyHoverEffect()
            .SetMetricsIdentifier("MainEditor", "UnfreezeAllObjects");
    }

    am->AddAction(ID_EDIT_HOLD, tr("&Hold"))
        .SetShortcut(tr("Ctrl+Alt+H"))
        .SetMetricsIdentifier("MainEditor", "Hold")
        .SetStatusTip(tr("Save the current state(Hold)"));
    am->AddAction(ID_EDIT_FETCH, tr("&Fetch"))
        .SetShortcut(tr("Ctrl+Alt+F"))
        .SetMetricsIdentifier("MainEditor", "Fetch")
        .SetStatusTip(tr("Restore saved state (Fetch)"));

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        // implemented by EditorTransformComponentSelection when the new Viewport Interaction Model is enabled
        am->AddAction(ID_EDIT_DELETE, tr("&Delete"))
            .SetShortcut(QKeySequence::Delete)
            .SetMetricsIdentifier("MainEditor", "DeleteSelectedObjects")
            .SetStatusTip(tr("Delete selected objects."))
            ->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        am->AddAction(ID_EDIT_CLONE, tr("Duplicate"))
            .SetShortcut(tr("Ctrl+D"))
            .SetMetricsIdentifier("MainEditor", "DuplicateSelectedObjects")
            .SetStatusTip(tr("Duplicate selected objects."));
    }

    // Modify actions
    am->AddAction(ID_CONVERTSELECTION_TOBRUSHES, tr("Brush"))
        .SetStatusTip(tr("Convert to Brush"))
        .SetMetricsIdentifier("MainEditor", "ConvertToBrush")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_CONVERTSELECTION_TOSIMPLEENTITY, tr("Geom Entity"))
        .SetMetricsIdentifier("MainEditor", "ConvertToGeomEntity")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_CONVERTSELECTION_TODESIGNEROBJECT, tr("Designer Object"))
        .SetMetricsIdentifier("MainEditor", "ConvertToDesignerObject")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_CONVERTSELECTION_TOSTATICENTITY, tr("StaticEntity"))
        .SetMetricsIdentifier("MainEditor", "ConvertToStaticEntity");
    am->AddAction(ID_CONVERTSELECTION_TOGAMEVOLUME, tr("GameVolume"))
        .SetMetricsIdentifier("MainEditor", "ConvertToGameVolume")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_CONVERTSELECTION_TOCOMPONENTENTITY, tr("Component Entity"))
        .SetMetricsIdentifier("MainEditor", "ConvertToComponentEntity");
    am->AddAction(ID_SUBOBJECTMODE_VERTEX, tr("Vertex"))
        .SetMetricsIdentifier("MainEditor", "SelectionModeVertex");
    am->AddAction(ID_SUBOBJECTMODE_EDGE, tr("Edge"))
        .SetMetricsIdentifier("MainEditor", "SelectionModeEdge");
    am->AddAction(ID_SUBOBJECTMODE_FACE, tr("Face"))
        .SetMetricsIdentifier("MainEditor", "SelectionModeFace");
    am->AddAction(ID_SUBOBJECTMODE_PIVOT, tr("Pivot"))
        .SetMetricsIdentifier("MainEditor", "SelectionPivot");
    am->AddAction(ID_MODIFY_OBJECT_HEIGHT, tr("Set Object(s) Height..."))
        .SetMetricsIdentifier("MainEditor", "SetObjectsHeight");
    am->AddAction(ID_EDIT_RENAMEOBJECT, tr("Rename Object(s)..."))
        .SetMetricsIdentifier("MainEditor", "RenameObjects")
        .SetStatusTip(tr("Rename Object"));

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_EDITMODE_SELECT, tr("Select mode"))
            .SetIcon(EditorProxyStyle::icon("Select"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("1"))
            .SetCheckable(true)
            .SetStatusTip(tr("Select object(s)"))
            .SetMetricsIdentifier("MainEditor", "ToolSelect")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditmodeSelect);
    }

    am->AddAction(ID_EDITMODE_MOVE, tr("Move"))
        .SetIcon(EditorProxyStyle::icon("Move"))
        .SetApplyHoverEffect()
        .SetShortcut(GetIEditor()->IsNewViewportInteractionModelEnabled() ? tr("1") : tr("2"))
        .SetCheckable(true)
        .SetStatusTip(tr("Select and move selected object(s)"))
        .SetMetricsIdentifier("MainEditor", "ToolMove")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditmodeMove);
    am->AddAction(ID_EDITMODE_ROTATE, tr("Rotate"))
        .SetIcon(EditorProxyStyle::icon("Translate"))
        .SetApplyHoverEffect()
        .SetShortcut(GetIEditor()->IsNewViewportInteractionModelEnabled() ? tr("2") : tr("3"))
        .SetCheckable(true)
        .SetStatusTip(tr("Select and rotate selected object(s)"))
        .SetMetricsIdentifier("MainEditor", "ToolRotate")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditmodeRotate);
    am->AddAction(ID_EDITMODE_SCALE, tr("Scale"))
        .SetIcon(EditorProxyStyle::icon("Scale"))
        .SetApplyHoverEffect()
        .SetShortcut(GetIEditor()->IsNewViewportInteractionModelEnabled() ? tr("3") : tr("4"))
        .SetCheckable(true)
        .SetStatusTip(tr("Select and scale selected object(s)"))
        .SetMetricsIdentifier("MainEditor", "ToolScale")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditmodeScale);

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_EDITMODE_SELECTAREA, tr("Select terrain"))
            .SetIcon(EditorProxyStyle::icon("Select_terrain"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("5"))
            .SetCheckable(true)
            .SetStatusTip(tr("Switch to terrain selection mode"))
            .SetMetricsIdentifier("MainEditor", "ToolSelectTerrain")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditmodeSelectarea);
        am->AddAction(ID_SELECT_AXIS_X, tr("Constrain to X axis"))
            .SetIcon(EditorProxyStyle::icon("X_axis"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("Ctrl+1"))
            .SetCheckable(true)
            .SetStatusTip(tr("Lock movement on X axis"))
            .SetMetricsIdentifier("MainEditor", "ToggleXAxisConstraint")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelectAxisX);
        am->AddAction(ID_SELECT_AXIS_Y, tr("Constrain to Y axis"))
            .SetIcon(EditorProxyStyle::icon("Y_axis"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("Ctrl+2"))
            .SetCheckable(true)
            .SetStatusTip(tr("Lock movement on Y axis"))
            .SetMetricsIdentifier("MainEditor", "ToggleYAxisConstraint")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelectAxisY);
        am->AddAction(ID_SELECT_AXIS_Z, tr("Constrain to Z axis"))
            .SetIcon(EditorProxyStyle::icon("Z_axis"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("Ctrl+3"))
            .SetCheckable(true)
            .SetStatusTip(tr("Lock movement on Z axis"))
            .SetMetricsIdentifier("MainEditor", "ToggleZAxisConstraint")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelectAxisZ);
        am->AddAction(ID_SELECT_AXIS_XY, tr("Constrain to XY plane"))
            .SetIcon(EditorProxyStyle::icon("XY2_copy"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("Ctrl+4"))
            .SetCheckable(true)
            .SetStatusTip(tr("Lock movement on XY plane"))
            .SetMetricsIdentifier("MainEditor", "ToggleYYPlaneConstraint")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelectAxisXy);
        am->AddAction(ID_SELECT_AXIS_TERRAIN, tr("Constrain to terrain/geometry"))
            .SetIcon(EditorProxyStyle::icon("Object_follow_terrain"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("Ctrl+5"))
            .SetCheckable(true)
            .SetStatusTip(tr("Lock object movement to follow terrain"))
            .SetMetricsIdentifier("MainEditor", "ToggleFollowTerrainConstraint")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelectAxisTerrain);
        am->AddAction(ID_SELECT_AXIS_SNAPTOALL, tr("Follow terrain and snap to objects"))
            .SetIcon(EditorProxyStyle::icon("Follow_terrain"))
            .SetApplyHoverEffect()
            .SetShortcut(tr("Ctrl+6"))
            .SetCheckable(true)
            .SetMetricsIdentifier("MainEditor", "ToggleSnapToObjectsAndTerrain")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelectAxisSnapToAll);
        am->AddAction(ID_OBJECTMODIFY_ALIGNTOGRID, tr("Align to grid"))
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected)
            .SetMetricsIdentifier("MainEditor", "ToggleAlignToGrid")
            .SetIcon(EditorProxyStyle::icon("Align_to_grid"))
            .SetApplyHoverEffect();
        am->AddAction(ID_OBJECTMODIFY_ALIGN, tr("Align to object")).SetCheckable(true)
#if AZ_TRAIT_OS_PLATFORM_APPLE
            .SetStatusTip(tr("⌘: Align an object to a bounding box, ⌥ : Keep Rotation of the moved object, Shift : Keep Scale of the moved object"))
#else
            .SetStatusTip(tr("Ctrl: Align an object to a bounding box, Alt : Keep Rotation of the moved object, Shift : Keep Scale of the moved object"))
#endif
            .SetMetricsIdentifier("MainEditor", "ToggleAlignToObjects")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateAlignObject)
            .SetIcon(EditorProxyStyle::icon("Align_to_Object"))
            .SetApplyHoverEffect();
        am->AddAction(ID_MODIFY_ALIGNOBJTOSURF, tr("Align object to surface (Hold CTRL)")).SetCheckable(true)
            .SetMetricsIdentifier("MainEditor", "ToggleAlignToSurfaceVoxels")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateAlignToVoxel)
            .SetIcon(EditorProxyStyle::icon("Align_object_to_surface"))
            .SetApplyHoverEffect();
    }

    am->AddAction(ID_SNAP_TO_GRID, tr("Snap to grid"))
        .SetIcon(EditorProxyStyle::icon("Grid"))
        .SetApplyHoverEffect()
        .SetShortcut(tr("G"))
        .SetStatusTip(tr("Toggles snap to grid"))
        .SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "ToggleSnapToGrid")
        .RegisterUpdateCallback(this, &MainWindow::OnUpdateSnapToGrid);
    am->AddAction(ID_SNAPANGLE, tr("Snap angle"))
        .SetIcon(EditorProxyStyle::icon("Angle"))
        .SetApplyHoverEffect()
        .SetStatusTip(tr("Snap angle"))
        .SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "ToggleSnapToAngle")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSnapangle);

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_ROTATESELECTION_XAXIS, tr("Rotate X Axis"))
            .SetMetricsIdentifier("MainEditor", "FastRotateXAxis");
        am->AddAction(ID_ROTATESELECTION_YAXIS, tr("Rotate Y Axis"))
            .SetMetricsIdentifier("MainEditor", "FastRotateYAxis");
        am->AddAction(ID_ROTATESELECTION_ZAXIS, tr("Rotate Z Axis"))
            .SetMetricsIdentifier("MainEditor", "FastRotateYAxis");
        am->AddAction(ID_ROTATESELECTION_ROTATEANGLE, tr("Rotate Angle..."))
            .SetMetricsIdentifier("MainEditor", "EditFastRotateAngle");
    }

    // Display actions
    am->AddAction(ID_WIREFRAME, tr("&Wireframe"))
        .SetShortcut(tr("F3"))
        .SetCheckable(true)
        .SetStatusTip(tr("Render in Wireframe Mode."))
        .SetMetricsIdentifier("MainEditor", "ToggleWireframeRendering")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateWireframe);

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_RULER, tr("Ruler"))
            .SetCheckable(true)
            .SetIcon(EditorProxyStyle::icon("Measure"))
            .SetApplyHoverEffect()
            .SetStatusTip(tr("Create temporary ruler to measure distance"))
            .SetMetricsIdentifier("MainEditor", "CreateTemporaryRuler")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateRuler);
    }

    am->AddAction(ID_VIEW_GRIDSETTINGS, tr("Grid Settings..."))
        .SetMetricsIdentifier("MainEditor", "GridSettings");
    am->AddAction(ID_SWITCHCAMERA_DEFAULTCAMERA, tr("Default Camera")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SwitchToDefaultCamera")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSwitchToDefaultCamera);
    am->AddAction(ID_SWITCHCAMERA_SEQUENCECAMERA, tr("Sequence Camera")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SwitchToSequenceCamera")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSwitchToSequenceCamera);
    am->AddAction(ID_SWITCHCAMERA_SELECTEDCAMERA, tr("Selected Camera Object")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "SwitchToSelectedCameraObject")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSwitchToSelectedCamera);
    am->AddAction(ID_SWITCHCAMERA_NEXT, tr("Cycle Camera"))
        .SetShortcut(tr("Ctrl+`"))
        .SetMetricsIdentifier("MainEditor", "CycleCamera");
    am->AddAction(ID_CHANGEMOVESPEED_INCREASE, tr("Increase"))
        .SetMetricsIdentifier("MainEditor", "IncreaseFlycamMoveSpeed")
        .SetStatusTip(tr("Increase Flycam Movement Speed"));
    am->AddAction(ID_CHANGEMOVESPEED_DECREASE, tr("Decrease"))
        .SetMetricsIdentifier("MainEditor", "DecreateFlycamMoveSpeed")
        .SetStatusTip(tr("Decrease Flycam Movement Speed"));
    am->AddAction(ID_CHANGEMOVESPEED_CHANGESTEP, tr("Change Step"))
        .SetMetricsIdentifier("MainEditor", "ChangeFlycamMoveStep")
        .SetStatusTip(tr("Change Flycam Movement Step"));
    am->AddAction(ID_DISPLAY_GOTOPOSITION, tr("Goto Coordinates"))
        .SetMetricsIdentifier("MainEditor", "GotoCoordinates");
    am->AddAction(ID_DISPLAY_SETVECTOR, tr("Display Set Vector"))
        .SetMetricsIdentifier("MainEditor", "DisplaySetVector");
    am->AddAction(ID_MODIFY_GOTO_SELECTION, tr("Center on Selection"))
        .SetShortcut(tr("Z"))
        .SetMetricsIdentifier("MainEditor", "GotoSelection")
        .Connect(&QAction::triggered, this, &MainWindow::OnGotoSelected);
    am->AddAction(ID_GOTO_LOC1, tr("Location 1"))
        .SetShortcut(tr("Shift+F1"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation1");
    am->AddAction(ID_GOTO_LOC2, tr("Location 2"))
        .SetShortcut(tr("Shift+F2"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation2");
    am->AddAction(ID_GOTO_LOC3, tr("Location 3"))
        .SetShortcut(tr("Shift+F3"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation2");
    am->AddAction(ID_GOTO_LOC4, tr("Location 4"))
        .SetShortcut(tr("Shift+F4"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation4");
    am->AddAction(ID_GOTO_LOC5, tr("Location 5"))
        .SetShortcut(tr("Shift+F5"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation5");
    am->AddAction(ID_GOTO_LOC6, tr("Location 6"))
        .SetShortcut(tr("Shift+F6"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation6");
    am->AddAction(ID_GOTO_LOC7, tr("Location 7"))
        .SetShortcut(tr("Shift+F7"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation7");
    am->AddAction(ID_GOTO_LOC8, tr("Location 8"))
        .SetShortcut(tr("Shift+F8"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation8");
    am->AddAction(ID_GOTO_LOC9, tr("Location 9"))
        .SetShortcut(tr("Shift+F9"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation9");
    am->AddAction(ID_GOTO_LOC10, tr("Location 10"))
        .SetShortcut(tr("Shift+F10"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation10");
    am->AddAction(ID_GOTO_LOC11, tr("Location 11"))
        .SetShortcut(tr("Shift+F11"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation11");
    am->AddAction(ID_GOTO_LOC12, tr("Location 12"))
        .SetShortcut(tr("Shift+F12"))
        .SetMetricsIdentifier("MainEditor", "GotoSelectedLocation12");
    am->AddAction(ID_TAG_LOC1, tr("Location 1"))
        .SetShortcut(tr("Ctrl+F1"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation1");
    am->AddAction(ID_TAG_LOC2, tr("Location 2"))
        .SetShortcut(tr("Ctrl+F2"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation2");
    am->AddAction(ID_TAG_LOC3, tr("Location 3"))
        .SetShortcut(tr("Ctrl+F3"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation3");
    am->AddAction(ID_TAG_LOC4, tr("Location 4"))
        .SetShortcut(tr("Ctrl+F4"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation4");
    am->AddAction(ID_TAG_LOC5, tr("Location 5"))
        .SetShortcut(tr("Ctrl+F5"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation5");
    am->AddAction(ID_TAG_LOC6, tr("Location 6"))
        .SetShortcut(tr("Ctrl+F6"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation6");
    am->AddAction(ID_TAG_LOC7, tr("Location 7"))
        .SetShortcut(tr("Ctrl+F7"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation7");
    am->AddAction(ID_TAG_LOC8, tr("Location 8"))
        .SetShortcut(tr("Ctrl+F8"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation8");
    am->AddAction(ID_TAG_LOC9, tr("Location 9"))
        .SetShortcut(tr("Ctrl+F9"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation9");
    am->AddAction(ID_TAG_LOC10, tr("Location 10"))
        .SetShortcut(tr("Ctrl+F10"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation10");
    am->AddAction(ID_TAG_LOC11, tr("Location 11"))
        .SetShortcut(tr("Ctrl+F11"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation11");
    am->AddAction(ID_TAG_LOC12, tr("Location 12"))
        .SetShortcut(tr("Ctrl+F12"))
        .SetMetricsIdentifier("MainEditor", "TagSelectedLocation12");

#ifndef OTHER_ACTIVE
    am->AddAction(ID_VIEW_CONFIGURELAYOUT, tr("Configure Layout..."))
        .SetMetricsIdentifier("MainEditor", "ConfigureLayoutDialog");
#endif
#ifdef FEATURE_ORTHOGRAPHIC_VIEW
    am->AddAction(ID_VIEW_CYCLE2DVIEWPORT, tr("Cycle Viewports"))
        .SetShortcut(tr("Ctrl+Tab"))
        .SetMetricsIdentifier("MainEditor", "CycleViewports")
        .SetStatusTip(tr("Cycle 2D Viewport"))
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateNonGameMode);
#endif
    am->AddAction(ID_DISPLAY_SHOWHELPERS, tr("Show/Hide Helpers"))
        .SetShortcut(tr("Shift+Space"))
        .SetMetricsIdentifier("MainEditor", "ToggleHelpers");

    // AI actions
    am->AddAction(ID_AI_GENERATEALL, tr("Generate &All AI"))
        .SetShortcut(tr(""))
        .SetMetricsIdentifier("MainEditor", "GenerateAllAI");
    am->AddAction(ID_AI_GENERATETRIANGULATION, tr("Generate &Triangulation"))
        .SetMetricsIdentifier("MainEditor", "GenerateTriangulation");
    am->AddAction(ID_AI_GENERATE3DVOLUMES, tr("Generate &3D Navigation Volumes"))
        .SetMetricsIdentifier("MainEditor", "Generate3DNavigationVolumes");
    am->AddAction(ID_AI_GENERATEFLIGHTNAVIGATION, tr("Generate &Flight Navigation"))
        .SetMetricsIdentifier("MainEditor", "GenerateFlightNavigation");
    am->AddAction(ID_AI_GENERATEWAYPOINT, tr("Generate &Waypoints"))
        .SetMetricsIdentifier("MainEditor", "GenerateWaypoints");
    am->AddAction(ID_AI_VALIDATENAVIGATION, tr("&Validate Navigation"))
        .SetMetricsIdentifier("MainEditor", "ValidateNavigation");
    am->AddAction(ID_AI_CLEARALLNAVIGATION, tr("&Clear All Navigation"))
        .SetMetricsIdentifier("MainEditor", "ClearAllNavigation");
    am->AddAction(ID_AI_GENERATESPAWNERS, tr("Generate Spawner Entity Code"))
        .SetMetricsIdentifier("MainEditor", "GenerateSpawnerEntityCode");
    am->AddAction(ID_AI_GENERATE3DDEBUGVOXELS, tr("Generate 3D Debug Vo&xels"))
        .SetMetricsIdentifier("MainEditor", "Generate3DDebugVoxels");
    am->AddAction(ID_AI_NAVIGATION_NEW_AREA, tr("Create New Navigation Area"))
        .SetMetricsIdentifier("MainEditor", "CreateNewNaviationArea")
        .SetStatusTip(tr("Create a new navigation area"));
    am->AddAction(ID_AI_NAVIGATION_TRIGGER_FULL_REBUILD, tr("Request a full MNM rebuild"))
        .SetMetricsIdentifier("MainEditor", "NaviationTriggerFullRebuild");
    am->AddAction(ID_AI_NAVIGATION_SHOW_AREAS, tr("Show Navigation Areas")).SetCheckable(true)
        .SetStatusTip(tr("Turn on/off navigation area display"))
        .SetMetricsIdentifier("MainEditor", "ToggleNavigationAreaDisplay")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnAINavigationShowAreasUpdate);
    am->AddAction(ID_AI_NAVIGATION_ADD_SEED, tr("Add Navigation Seed"))
        .SetMetricsIdentifier("MainEditor", "AddNavigationSeed");
    am->AddAction(ID_AI_NAVIGATION_ENABLE_CONTINUOUS_UPDATE, tr("Continuous Update")).SetCheckable(true)
        .SetStatusTip(tr("Turn on/off background continuous navigation updates"))
        .SetMetricsIdentifier("MainEditor", "ToggleNavigationContinuousUpdate")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnAINavigationEnableContinuousUpdateUpdate);
    am->AddAction(ID_AI_NAVIGATION_VISUALIZE_ACCESSIBILITY, tr("Visualize Navigation Accessibility")).SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "ToggleNavigationVisualizeAccessibility")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnVisualizeNavigationAccessibilityUpdate);
    am->AddAction(ID_AI_NAVIGATION_DISPLAY_AGENT, tr("View Agent Type"))
        .SetStatusTip(tr("Toggle navigation debug display"))
        .SetMetricsIdentifier("MainEditor", "ToggleNavigationDebugDisplay")
        .SetMenu(new CNavigationAgentTypeMenu);
    am->AddAction(ID_AI_GENERATECOVERSURFACES, tr("Generate Cover Surfaces"))
        .SetMetricsIdentifier("MainEditor", "AIGenerateCoverSurfaces");
    am->AddAction(ID_MODIFY_AIPOINT_PICKLINK, tr("AIPoint Pick Link"))
        .SetMetricsIdentifier("MainEditor", "AIPointPickLink");
    am->AddAction(ID_MODIFY_AIPOINT_PICKIMPASSLINK, tr("AIPoint Pick Impass Link"))
        .SetMetricsIdentifier("MainEditor", "AIPointPickImpassLink");

    // Audio actions
    am->AddAction(ID_SOUND_STOPALLSOUNDS, tr("Stop All Sounds"))
        .SetMetricsIdentifier("MainEditor", "StopAllSounds")
        .Connect(&QAction::triggered, this, &MainWindow::OnStopAllSounds);
    am->AddAction(ID_AUDIO_REFRESH_AUDIO_SYSTEM, tr("Refresh Audio"))
        .SetMetricsIdentifier("MainEditor", "RefreshAudio")
        .Connect(&QAction::triggered, this, &MainWindow::OnRefreshAudioSystem);

    // Clouds actions
    am->AddAction(ID_CLOUDS_CREATE, tr("Create"))
        .SetMetricsIdentifier("MainEditor", "CloudCreate")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_CLOUDS_DESTROY, tr("Destroy"))
        .SetMetricsIdentifier("MainEditor", "CloudDestroy")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateCloudsDestroy);
    am->AddAction(ID_CLOUDS_OPEN, tr("Open"))
        .SetMetricsIdentifier("MainEditor", "CloudOpen")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateCloudsOpen);
    am->AddAction(ID_CLOUDS_CLOSE, tr("Close"))
        .SetMetricsIdentifier("MainEditor", "CloudClose")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateCloudsClose);

    // Fame actions
    am->AddAction(ID_VIEW_SWITCHTOGAME, tr("Play &Game"))
        .SetShortcut(tr("Ctrl+G"))
        .SetStatusTip(tr("Activate the game input mode"))
        .SetIcon(EditorProxyStyle::icon("Play"))
        .SetApplyHoverEffect()
        .SetMetricsIdentifier("MainEditor", "ToggleGameMode")
        .SetCheckable(true)
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdatePlayGame);
    am->AddAction(ID_SWITCH_PHYSICS, tr("Simulate"))
        .SetShortcut(tr("Ctrl+P"))
        .SetCheckable(true)
        .SetStatusTip(tr("Enable processing of Physics and AI."))
        .SetMetricsIdentifier("MainEditor", "TogglePhysicsAndAI")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnSwitchPhysicsUpdate);
#ifdef LY_TERRAIN_EDITOR
    am->AddAction(ID_TERRAIN_COLLISION, tr("Enable Camera Terrain Collision")).SetCheckable(true)
        .SetStatusTip(tr("Enable collision of camera with terrain."))
        .SetMetricsIdentifier("MainEditor", "ToggleTerrainCameraCollision")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnTerrainCollisionUpdate);
#endif //#ifdef LY_TERRAIN_EDITOR
    am->AddAction(ID_GAME_SYNCPLAYER, tr("Move Player and Camera Separately")).SetCheckable(true)
        .SetStatusTip(tr("Move Player and Camera Separately\nMove Player and Camera Separately"))
        .SetMetricsIdentifier("MainEditor", "SynchronizePlayerWithCamear")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnSyncPlayerUpdate);
    am->AddAction(ID_TOOLS_EQUIPPACKSEDIT, tr("&Edit Equipment-Packs..."))
        .SetMetricsIdentifier("MainEditor", "EditEquipmentPacksDialog");
    am->AddAction(ID_TOGGLE_MULTIPLAYER, tr("Toggle SP/MP GameRules")).SetCheckable(true)
        .SetStatusTip(tr("Switch SP/MP gamerules."))
        .SetMetricsIdentifier("MainEditor", "ToggleSP/MPGameRules")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnToggleMultiplayerUpdate);
    am->AddAction(ID_VIEW_DEPLOY, tr("Deploy"))
        .SetStatusTip(tr("Open the Deployment tool"))
        .SetIcon(EditorProxyStyle::icon("Deploy"))
        .SetMetricsIdentifier("MainEditor", "Deploy")
        .SetCheckable(false);

    // Physics actions
    am->AddAction(ID_PHYSICS_GETPHYSICSSTATE, tr("Get Physics State"))
        .SetMetricsIdentifier("MainEditor", "PhysicsGetState")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_PHYSICS_RESETPHYSICSSTATE, tr("Reset Physics State"))
        .SetMetricsIdentifier("MainEditor", "PhysicsResetState")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
    am->AddAction(ID_PHYSICS_SIMULATEOBJECTS, tr("Simulate Objects"))
        .SetMetricsIdentifier("MainEditor", "PhysicsSimulateObjects")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);

    // Prefabs actions
    am->AddAction(ID_PREFABS_MAKEFROMSELECTION, tr("Create Prefab from Selected Object(s)"))
        .SetStatusTip(tr("Make a new Prefab from selected objects."))
        .SetMetricsIdentifier("MainEditor", "PrefabCreateFromSelection")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdatePrefabsMakeFromSelection);
    am->AddAction(ID_PREFABS_ADDSELECTIONTOPREFAB, tr("Add Selected Object(s) to Prefab"))
        .SetStatusTip(tr("Add Selection to Prefab"))
        .SetMetricsIdentifier("MainEditor", "PrefabAddSelection")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateAddSelectionToPrefab);
    am->AddAction(ID_PREFABS_CLONESELECTIONFROMPREFAB, tr("Clone Selected Object(s)"))
        .SetMetricsIdentifier("MainEditor", "PrefabCloneSelection")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateCloneSelectionFromPrefab);
    am->AddAction(ID_PREFABS_EXTRACTSELECTIONFROMPREFAB, tr("Extract Selected Object(s)"))
        .SetMetricsIdentifier("MainEditor", "PrefabsExtractSelection")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateExtractSelectionFromPrefab);
    am->AddAction(ID_PREFABS_OPENALL, tr("Open All"))
        .SetMetricsIdentifier("MainEditor", "PrefabsOpenAll");
    am->AddAction(ID_PREFABS_CLOSEALL, tr("Close All"))
        .SetMetricsIdentifier("MainEditor", "PrefabsCloseAll");
    am->AddAction(ID_PREFABS_REFRESHALL, tr("Reload All"))
        .SetMetricsIdentifier("MainEditor", "PrefabsReloadAll")
        .SetStatusTip(tr("Recreate all objects in Prefabs."));

    // Terrain actions
    am->AddAction(ID_FILE_GENERATETERRAINTEXTURE, tr("&Generate Terrain Texture"))
        .SetStatusTip(tr("Generate terrain texture"))
        .SetMetricsIdentifier("MainEditor", "TerrainGenerateTexture")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGenerateTerrainTexture);
    am->AddAction(ID_FILE_GENERATETERRAIN, tr("&Generate Terrain"))
        .SetStatusTip(tr("Generate terrain"))
        .SetMetricsIdentifier("MainEditor", "TerrainGenerate")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateGenerateTerrain);

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_TERRAIN, tr("&Edit Terrain"))
            .SetMetricsIdentifier("MainEditor", "TerrainEditDialog")
            .SetStatusTip(tr("Open Terrain Editor"));
        am->AddAction(ID_GENERATORS_TEXTURE, tr("Terrain &Texture Layers"))
            .SetMetricsIdentifier("MainEditor", "TerrainTextureLayersDialog")
            .SetStatusTip(tr("Bring up the terrain texture generation dialog"));
        am->AddAction(ID_TERRAIN_TEXTURE_EXPORT, tr("Export/Import Megaterrain Texture"))
            .SetMetricsIdentifier("MainEditor", "TerrainExportOrImportMegaterrainTexture");
        am->AddAction(ID_GENERATORS_LIGHTING, tr("&Sun Trajectory Tool"))
            .SetIcon(EditorProxyStyle::icon("Lighting"))
            .SetApplyHoverEffect()
            .SetMetricsIdentifier("MainEditor", "SunTrajectoryToolDialog")
            .SetStatusTip(tr("Bring up the terrain lighting dialog"));
        am->AddAction(ID_TERRAIN_TIMEOFDAY, tr("Time Of Day"))
            .SetMetricsIdentifier("MainEditor", "TimeOfDayDialog")
            .SetStatusTip(tr("Open Time of Day Editor"));
        am->AddAction(ID_RELOAD_TERRAIN, tr("Reload Terrain"))
            .SetMetricsIdentifier("MainEditor", "TerrainReload")
            .SetStatusTip(tr("Reload Terrain in Game"));
        am->AddAction(ID_TERRAIN_EXPORTBLOCK, tr("Export Terrain Block"))
            .SetMetricsIdentifier("MainEditor", "TerrainExportBlock")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateTerrainExportblock);
        am->AddAction(ID_TERRAIN_IMPORTBLOCK, tr("Import Terrain Block"))
            .SetMetricsIdentifier("MainEditor", "TerrainImportBlock")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateTerrainImportblock);
        am->AddAction(ID_TERRAIN_RESIZE, tr("Resize Terrain"))
            .SetStatusTip(tr("Resize Terrain Heightmap"))
            .SetMetricsIdentifier("MainEditor", "TerrainResizeHeightmap")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateTerrainResizeterrain);
        am->AddAction(ID_TOOLTERRAINMODIFY_SMOOTH, tr("Flatten"))
            .SetMetricsIdentifier("MainEditor", "TerrainFlattenTool");
        am->AddAction(ID_TERRAINMODIFY_SMOOTH, tr("Smooth"))
            .SetMetricsIdentifier("MainEditor", "TerrainSmoothTool");
        am->AddAction(ID_TERRAIN_VEGETATION, tr("Edit Vegetation"))
            .SetMetricsIdentifier("MainEditor", "EditVegetation");
        am->AddAction(ID_TERRAIN_PAINTLAYERS, tr("Paint Layers"))
            .SetMetricsIdentifier("MainEditor", "PaintLayers");
        am->AddAction(ID_TERRAIN_REFINETERRAINTEXTURETILES, tr("Refine Terrain Texture Tiles"))
            .SetMetricsIdentifier("MainEditor", "TerrainRefineTextureTiles");
        am->AddAction(ID_FILE_EXPORT_TERRAINAREA, tr("Export Terrain Area"))
            .SetMetricsIdentifier("MainEditor", "TerrainExportArea")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateExportTerrainArea);
        am->AddAction(ID_FILE_EXPORT_TERRAINAREAWITHOBJECTS, tr("Export &Terrain Area with Objects"))
            .SetMetricsIdentifier("MainEditor", "TerrainExportAreaWithObjects")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateExportTerrainArea);
    }

    // Tools actions
    am->AddAction(ID_RELOAD_ALL_SCRIPTS, tr("Reload All Scripts"))
        .SetMetricsIdentifier("MainEditor", "ScriptsReloadAll")
        .SetStatusTip(tr("Reload all Scripts."));
    am->AddAction(ID_RELOAD_ENTITY_SCRIPTS, tr("Reload Entity Scripts"))
        .SetMetricsIdentifier("MainEditor", "ScriptsReloadEntity")
        .SetStatusTip(tr("Reload all Entity Scripts."));
    am->AddAction(ID_RELOAD_ACTOR_SCRIPTS, tr("Reload Actor Scripts"))
        .SetMetricsIdentifier("MainEditor", "ScriptsReloadActor")
        .SetStatusTip(tr("Reload all Game Scripts (Actor, Gamerules)."));
    am->AddAction(ID_RELOAD_ITEM_SCRIPTS, tr("Reload Item Scripts"))
        .SetMetricsIdentifier("MainEditor", "ScriptsReloadItem")
        .SetStatusTip(tr("Reload all Item Scripts."));
    am->AddAction(ID_RELOAD_AI_SCRIPTS, tr("Reload AI Scripts"))
        .SetMetricsIdentifier("MainEditor", "ScriptsReloadAI")
        .SetStatusTip(tr("Reload all AI Scripts."));
    am->AddAction(ID_RELOAD_UI_SCRIPTS, tr("Reload UI Scripts"))
        .SetMetricsIdentifier("MainEditor", "ScriptsReloadUI");
    am->AddAction(ID_RELOAD_TEXTURES, tr("Reload Textures/Shaders"))
        .SetMetricsIdentifier("MainEditor", "ReloadTexturesAndShaders")
        .SetStatusTip(tr("Reload all textures."));
    am->AddAction(ID_RELOAD_GEOMETRY, tr("Reload Geometry"))
        .SetMetricsIdentifier("MainEditor", "ReloadGeometry")
        .SetStatusTip(tr("Reload all geometries."));
    // This action is already in the terrain menu - no need to create twice
    // am->AddAction(ID_RELOAD_TERRAIN, tr("Reload Terrain"));
    am->AddAction(ID_TOOLS_RESOLVEMISSINGOBJECTS, tr("Resolve Missing Objects/Materials..."))
        .SetMetricsIdentifier("MainEditor", "MissingAssetResolverDialog");
    am->AddAction(ID_TOOLS_ENABLEFILECHANGEMONITORING, tr("Enable File Change Monitoring"))
        .SetMetricsIdentifier("MainEditor", "ToggleFileChangeMonitoring");
    am->AddAction(ID_CLEAR_REGISTRY, tr("Clear Registry Data"))
        .SetMetricsIdentifier("MainEditor", "ClearRegistryData")
        .SetStatusTip(tr("Clear Registry Data"));
    am->AddAction(ID_VALIDATELEVEL, tr("&Check Level for Errors"))
        .SetMetricsIdentifier("MainEditor", "CheckLevelForErrors")
        .SetStatusTip(tr("Validate Level"));
    am->AddAction(ID_TOOLS_VALIDATEOBJECTPOSITIONS, tr("Check Object Positions"))
        .SetMetricsIdentifier("MainEditor", "CheckObjectPositions");
#ifndef OTHER_ACTIVE
    am->AddAction(ID_TOOLS_LOGMEMORYUSAGE, tr("Save Level Statistics"))
        .SetMetricsIdentifier("MainEditor", "SaveLevelStatistics")
        .SetStatusTip(tr("Logs Editor memory usage."));
#else
    am->AddAction(ID_TOOLS_LOGMEMORYUSAGE, tr("Save Level Statistics (Disabled when Other is active)"))
        .SetMetricsIdentifier("MainEditor", "SaveLevelStatistics")
        .SetStatusTip(tr("Logs Editor memory usage."))
        ->setEnabled(false);
#endif
    am->AddAction(ID_SCRIPT_COMPILESCRIPT, tr("Compile Cry Lua &Script (LEGACY)"))
        .SetMetricsIdentifier("MainEditor", "CompileScript");
    am->AddAction(ID_RESOURCES_REDUCEWORKINGSET, tr("Reduce Working Set"))
        .SetMetricsIdentifier("MainEditor", "ReduceWorkingSet")
        .SetStatusTip(tr("Reduce Physical RAM Working Set."));
    am->AddAction(ID_TOOLS_UPDATEPROCEDURALVEGETATION, tr("Update Procedural Vegetation"))
        .SetMetricsIdentifier("MainEditor", "UpdateProceduralVegetation");
    am->AddAction(ID_TOOLS_CONFIGURETOOLS, tr("Configure ToolBox Macros..."))
        .SetMetricsIdentifier("MainEditor", "ConfigureToolboxMacros");
    am->AddAction(ID_TOOLS_SCRIPTHELP, tr("Script Help"))
        .SetMetricsIdentifier("MainEditor", "ScriptHelp");
    am->AddAction(ID_TOOLS_LUA_EDITOR, tr("Lua Editor"))
        .SetMetricsIdentifier("MainEditor", "Lua Editor");

    // View actions
    am->AddAction(ID_VIEW_OPENVIEWPANE, tr("Open View Pane"))
        .SetMetricsIdentifier("MainEditor", "OpenViewPane");
    am->AddAction(ID_VIEW_ROLLUPBAR, tr(LyViewPane::LegacyRollupBarMenuName))
        .SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "ToggleRollupBar")
        .Connect(&QAction::triggered, this, &MainWindow::ToggleRollupBar);
    am->AddAction(ID_VIEW_CONSOLEWINDOW, tr(LyViewPane::ConsoleMenuName))
        .SetShortcut(tr("^"))
        .SetReserved()
        .SetStatusTip(tr("Show or hide the console window"))
        .SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "ToggleConsoleWindow")
        .Connect(&QAction::triggered, this, &MainWindow::ToggleConsole);
    am->AddAction(ID_OPEN_QUICK_ACCESS_BAR, tr("Show &Quick Access Bar"))
        .SetShortcut(tr("Ctrl+Alt+Space"))
        .SetMetricsIdentifier("MainEditor", "ToggleQuickAccessBar");

    // Disable layouts menu if other is enabled
#ifndef OTHER_ACTIVE
    am->AddAction(ID_VIEW_LAYOUTS, tr("Layouts"))
        .SetMetricsIdentifier("MainEditor", "Layouts");

    am->AddAction(ID_VIEW_SAVELAYOUT, tr("Save Layout..."))
        .SetMetricsIdentifier("MainEditor", "SaveLayout")
        .Connect(&QAction::triggered, this, &MainWindow::SaveLayout);
    am->AddAction(ID_VIEW_LAYOUT_LOAD_DEFAULT, tr("Restore Default Layout"))
        .SetMetricsIdentifier("MainEditor", "RestoreDefaultLayout")
        .Connect(&QAction::triggered, [this]() { m_viewPaneManager->RestoreDefaultLayout(true); });
#endif

    am->AddAction(ID_SKINS_REFRESH, tr("Refresh Style"))
        .SetMetricsIdentifier("MainEditor", "RefreshStyle")
        .SetToolTip(tr("Refreshes the editor stylesheet"))
        .Connect(&QAction::triggered, this, &MainWindow::RefreshStyle);

    // AWS actions
    am->AddAction(ID_AWS_LAUNCH, tr("Main AWS Console")).RegisterUpdateCallback(cryEdit, &CCryEditApp::OnAWSLaunchUpdate)
        .SetMetricsIdentifier("MainEditor", "OpenAWSConsole");
    am->AddAction(ID_AWS_GAMELIFT_LEARN, tr("Learn more")).SetToolTip(tr("Learn more about Amazon GameLift"))
        .SetMetricsIdentifier("MainEditor", "GameLiftLearnMore");
    am->AddAction(ID_AWS_GAMELIFT_CONSOLE, tr("Console")).SetToolTip(tr("Show the Amazon GameLift Console"))
        .SetMetricsIdentifier("MainEditor", "GameLiftConsole");
    am->AddAction(ID_AWS_GAMELIFT_GETSTARTED, tr("Getting Started"))
        .SetMetricsIdentifier("MainEditor", "GameLiftGettingStarted");
    am->AddAction(ID_AWS_GAMELIFT_TRIALWIZARD, tr("Trial Wizard"))
        .SetMetricsIdentifier("MainEditor", "GameLiftTrialWizard");
    am->AddAction(ID_AWS_COGNITO_CONSOLE, tr("Cognito"))
        .SetMetricsIdentifier("MainEditor", "CognitoConsole");
    am->AddAction(ID_AWS_DEVICEFARM_CONSOLE, tr("Device Farm"))
        .SetMetricsIdentifier("MainEditor", "DeviceFarmConsole");
    am->AddAction(ID_AWS_DYNAMODB_CONSOLE, tr("DynamoDB"))
        .SetMetricsIdentifier("MainEditor", "DynamoDBConsole");
    am->AddAction(ID_AWS_LAMBDA_CONSOLE, tr("Lambda"))
        .SetMetricsIdentifier("MainEditor", "LambdaConsole");
    am->AddAction(ID_AWS_S3_CONSOLE, tr("S3"))
        .SetMetricsIdentifier("MainEditor", "S3Console");
    am->AddAction(ID_AWS_ACTIVE_DEPLOYMENT, tr("Select a Deployment"))
        .SetMetricsIdentifier("MainEditor", "AWSSelectADeployment");
    am->AddAction(ID_AWS_CREDENTIAL_MGR, tr("Credentials manager"))
        .SetMetricsIdentifier("MainEditor", "AWSCredentialsManager");
    am->AddAction(ID_AWS_RESOURCE_MANAGEMENT, tr("Resource Manager")).SetToolTip(tr("Show the Cloud Canvas Resource Manager"))
        .SetMetricsIdentifier("MainEditor", "AWSResourceManager");

    // Commerce actions
    am->AddAction(ID_COMMERCE_MERCH, tr("Merch by Amazon"))
        .SetMetricsIdentifier("MainEditor", "AmazonMerch");
    am->AddAction(ID_COMMERCE_PUBLISH, tr("Publishing on Amazon"))
        .SetMetricsIdentifier("MainEditor", "PublishingOnAmazon")
        .SetStatusTip(tr("https://developer.amazon.com/appsandservices/solutions/platforms/mac-pc"));

    // Help actions
    am->AddAction(ID_DOCUMENTATION_GETTINGSTARTEDGUIDE, tr("Getting Started"))
        .SetMetricsIdentifier("MainEditor", "DocsGettingStarted")
        .SetReserved();
    am->AddAction(ID_DOCUMENTATION_TUTORIALS, tr("Tutorials"))
        .SetMetricsIdentifier("MainEditor", "DocsTutorials")
        .SetReserved();

    am->AddAction(ID_DOCUMENTATION_GLOSSARY, tr("Glossary"))
        .SetMetricsIdentifier("MainEditor", "DocsGlossary")
        .SetReserved();
    am->AddAction(ID_DOCUMENTATION_LUMBERYARD, tr("Lumberyard Documentation"))
        .SetMetricsIdentifier("MainEditor", "DocsLumberyardDocumentation")
        .SetReserved();
    am->AddAction(ID_DOCUMENTATION_GAMELIFT, tr("GameLift Documentation"))
        .SetMetricsIdentifier("MainEditor", "DocsGameLift")
        .SetReserved();
    am->AddAction(ID_DOCUMENTATION_RELEASENOTES, tr("Release Notes"))
        .SetMetricsIdentifier("MainEditor", "DocsReleaseNotes")
        .SetReserved();

    am->AddAction(ID_DOCUMENTATION_GAMEDEVBLOG, tr("GameDev Blog"))
        .SetMetricsIdentifier("MainEditor", "DocsGameDevBlog")
        .SetReserved();
    am->AddAction(ID_DOCUMENTATION_TWITCHCHANNEL, tr("GameDev Twitch Channel"))
        .SetMetricsIdentifier("MainEditor", "DocsGameDevTwitchChannel")
        .SetReserved();
    am->AddAction(ID_DOCUMENTATION_FORUMS, tr("Forums"))
        .SetMetricsIdentifier("MainEditor", "DocsForums")
        .SetReserved();
    am->AddAction(ID_DOCUMENTATION_AWSSUPPORT, tr("AWS Support"))
        .SetMetricsIdentifier("MainEditor", "DocsAWSSupport")
        .SetReserved();

    am->AddAction(ID_DOCUMENTATION_FEEDBACK, tr("Give Us Feedback"))
        .SetMetricsIdentifier("MainEditor", "DocsFeedback")
        .SetReserved();
    am->AddAction(ID_APP_ABOUT, tr("&About Lumberyard"))
        .SetMetricsIdentifier("MainEditor", "AboutLumberyard")
        .SetStatusTip(tr("Display program information, version number and copyright"))
        .SetReserved();

    // Editors Toolbar actions
    am->AddAction(ID_OPEN_ASSET_BROWSER, tr("Asset browser"))
        .SetToolTip(tr("Open Asset Browser"))
        .SetApplyHoverEffect();

    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_OPEN_LAYER_EDITOR, tr(LyViewPane::LegacyLayerEditor))
            .SetToolTip(tr("Open Layer Editor"))
        .SetApplyHoverEffect();
    }

#ifndef OTHER_ACTIVE
    am->AddAction(ID_OPEN_MATERIAL_EDITOR, tr(LyViewPane::MaterialEditor))
        .SetToolTip(tr("Open Material Editor"))
        .SetIcon(EditorProxyStyle::icon("Material"))
        .SetApplyHoverEffect();
#endif

#ifdef ENABLE_LEGACY_ANIMATION
    am->AddAction(ID_OPEN_CHARACTER_TOOL, tr(LyViewPane::LegacyGeppetto))
        .SetToolTip(tr("Open Geppetto"))
        .SetIcon(EditorProxyStyle::icon("Gepetto"))
        .SetApplyHoverEffect();
    am->AddAction(ID_OPEN_MANNEQUIN_EDITOR, tr("Mannequin"))
        .SetToolTip(tr("Open Mannequin (LEGACY)"))
        .SetIcon(EditorProxyStyle::icon("Mannequin"));
#endif // ENABLE_LEGACY_ANIMATION
    AZ::EBusReduceResult<bool, AZStd::logical_or<bool>> emfxEnabled(false);
    using AnimationRequestBus = AzToolsFramework::EditorAnimationSystemRequestsBus;
    using AnimationSystemType = AzToolsFramework::EditorAnimationSystemRequests::AnimationSystem;
    AnimationRequestBus::BroadcastResult(emfxEnabled, &AnimationRequestBus::Events::IsSystemActive, AnimationSystemType::EMotionFX);
    if (emfxEnabled.value)
    {
        QAction* action = am->AddAction(ID_OPEN_EMOTIONFX_EDITOR, tr("Animation Editor"))
            .SetToolTip(tr("Open Animation Editor (PREVIEW)"))
            .SetIcon(QIcon(":/EMotionFX/EMFX_icon_32x32.png"))
            .SetApplyHoverEffect();
        QObject::connect(action, &QAction::triggered, this, []() {
            QtViewPaneManager::instance()->OpenPane(LyViewPane::AnimationEditor);
        });
    }

    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_OPEN_AIDEBUGGER, tr(LyViewPane::AIDebugger))
            .SetToolTip(tr("Open AI Debugger"))
        .SetIcon(QIcon(":/MainWindow/toolbars/standard_views_toolbar-08.png"))
        .SetApplyHoverEffect();
    }

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_OPEN_TRACKVIEW, tr("TrackView"))
            .SetToolTip(tr("Open Track View"))
            .SetApplyHoverEffect();
    }

    am->AddAction(ID_OPEN_AUDIO_CONTROLS_BROWSER, tr("Audio Controls Editor"))
        .SetToolTip(tr("Open Audio Controls Editor"))
        .SetIcon(EditorProxyStyle::icon("Audio"))
        .SetApplyHoverEffect();

#if defined(LY_TERRAIN_EDITOR) && !defined(OTHER_ACTIVE)
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_OPEN_TERRAIN_EDITOR, tr(LyViewPane::TerrainEditor))
            .SetToolTip(tr("Open Terrain Editor"))
            .SetIcon(EditorProxyStyle::icon("Terrain"))
            .SetApplyHoverEffect();
        am->AddAction(ID_OPEN_TERRAINTEXTURE_EDITOR, tr("Terrain Texture Layers Editor"))
            .SetToolTip(tr("Open Terrain Texture Layers"))
            .SetIcon(EditorProxyStyle::icon("Terrain_Texture"))
            .SetApplyHoverEffect();
    }
#endif // #if defined(LY_TERRAIN_EDITOR) && !defined(OTHER_ACTIVE)

#ifndef OTHER_ACTIVE
        am->AddAction(ID_PARTICLE_EDITOR, tr("Particle Editor"))
            .SetToolTip(tr("Open Particle Editor"))
            .SetIcon(EditorProxyStyle::icon("particle"))
            .SetApplyHoverEffect();

        am->AddAction(ID_TERRAIN_TIMEOFDAYBUTTON, tr("Time of Day Editor"))
            .SetToolTip(tr("Open Time of Day"))
            .SetApplyHoverEffect();
#endif // OTHER_ACTIVE

    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_OPEN_DATABASE, tr(LyViewPane::DatabaseView))
            .SetToolTip(tr("Open Database View"))
        .SetIcon(EditorProxyStyle::icon("Database_view"))
        .SetApplyHoverEffect();
    }
    am->AddAction(ID_OPEN_UICANVASEDITOR, tr("UI Editor"))
        .SetToolTip(tr("Open UI Editor"))
        .SetApplyHoverEffect();

    // Edit Mode Toolbar Actions
    am->AddAction(ID_EDITTOOL_LINK, tr("Link an object to parent"))
        .SetIcon(EditorProxyStyle::icon("add_link"))
        .SetApplyHoverEffect()
        .SetCheckable(true)
        .SetMetricsIdentifier("MainEditor", "ToolLinkObjectToParent")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditToolLink);
    am->AddAction(ID_EDITTOOL_UNLINK, tr("Unlink all selected objects"))
        .SetIcon(EditorProxyStyle::icon("remove_link"))
        .SetApplyHoverEffect()
        .SetMetricsIdentifier("MainEditor", "ToolUnlinkSelection")
        .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateEditToolUnlink);
    am->AddAction(IDC_SELECTION_MASK, tr("Selected Object Types"))
        .SetMetricsIdentifier("MainEditor", "SelectedObjectTypes");
    am->AddAction(ID_REF_COORDS_SYS, tr("Reference coordinate system"))
        .SetShortcut(tr("Ctrl+W"))
        .SetMetricsIdentifier("MainEditor", "ToggleReferenceCoordinateSystem")
        .Connect(&QAction::triggered, this, &MainWindow::ToggleRefCoordSys);
    am->AddAction(IDC_SELECTION, tr("Named Selections"))
        .SetMetricsIdentifier("MainEditor", "NamedSelections");

    if (m_enableLegacyCryEntities)
    {
        am->AddAction(ID_SELECTION_DELETE, tr("Delete named selection"))
            .SetIcon(EditorProxyStyle::icon("Select"))
        .SetApplyHoverEffect()
            .SetMetricsIdentifier("MainEditor", "DeleteNamedSelection")
            .Connect(&QAction::triggered, this, &MainWindow::DeleteSelection);

        am->AddAction(ID_LAYER_SELECT, tr(""))
            .SetToolTip(tr("Select current layer"))
            .SetApplyHoverEffect()
            .SetMetricsIdentifier("MainEditor", "LayerSelect")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateCurrentLayer);
    }

    // Object Toolbar Actions
    am->AddAction(ID_GOTO_SELECTED, tr("Go to selected object"))
        .SetIcon(EditorProxyStyle::icon("select_object"))
        .SetApplyHoverEffect()
        .SetMetricsIdentifier("MainEditor", "GotoSelection")
        .Connect(&QAction::triggered, this, &MainWindow::OnGotoSelected);

    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        am->AddAction(ID_OBJECTMODIFY_SETHEIGHT, tr("Set object(s) height"))
            .SetIcon(QIcon(":/MainWindow/toolbars/object_toolbar-03.svg"))
            .SetApplyHoverEffect()
            .SetMetricsIdentifier("MainEditor", "SetObjectHeight")
            .RegisterUpdateCallback(cryEdit, &CCryEditApp::OnUpdateSelected);
        // vertex snapping not yet supported when the new Viewport Interaction Model is enabled
        am->AddAction(ID_OBJECTMODIFY_VERTEXSNAPPING, tr("Vertex snapping"))
            .SetMetricsIdentifier("MainEditor", "ToggleVertexSnapping")
            .SetIcon(EditorProxyStyle::icon("Vertex_snapping"))
            .SetApplyHoverEffect();
    }

    // Misc Toolbar Actions
    am->AddAction(ID_OPEN_SUBSTANCE_EDITOR, tr("Open Substance Editor"))
        .SetMetricsIdentifier("MainEditor", "OpenSubstanceEditor")
        .SetApplyHoverEffect();
}

void MainWindow::InitToolActionHandlers()
{
    ActionManager* am = GetActionManager();
    CToolBoxManager* tbm = GetIEditor()->GetToolBoxManager();
    am->RegisterActionHandler(ID_APP_EXIT, [=]() { window()->close(); });

    for (int id = ID_TOOL_FIRST; id <= ID_TOOL_LAST; ++id)
    {
        am->RegisterActionHandler(id, [tbm, id] {
            tbm->ExecuteMacro(id - ID_TOOL_FIRST, true);
        });
    }

    for (int id = ID_TOOL_SHELVE_FIRST; id <= ID_TOOL_SHELVE_LAST; ++id)
    {
        am->RegisterActionHandler(id, [tbm, id] {
            tbm->ExecuteMacro(id - ID_TOOL_SHELVE_FIRST, false);
        });
    }

    for (int id = CEditorCommandManager::CUSTOM_COMMAND_ID_FIRST; id <= CEditorCommandManager::CUSTOM_COMMAND_ID_LAST; ++id)
    {
        am->RegisterActionHandler(id, [id] {
            GetIEditor()->GetCommandManager()->Execute(id);
        });
    }
}

void MainWindow::OnEscapeAction()
{
    if (GetIEditor()->IsInGameMode())
    {
        GetIEditor()->SetInGameMode(false);
    }
    else
    {
        AzToolsFramework::EditorEvents::Bus::Broadcast(
            &AzToolsFramework::EditorEvents::OnEscape);

        CCryEditApp::instance()->OnEditEscape();
    }
}

void MainWindow::InitToolBars()
{
    m_toolbarManager->LoadToolbars();
    AdjustToolBarIconSize(static_cast<AzQtComponents::ToolBar::ToolBarIconSize>(gSettings.gui.nToolbarIconSize));
}

QComboBox* MainWindow::CreateSelectionMaskComboBox()
{
    //IDC_SELECTION_MASK
    struct Mask
    {
        QString text;
        uint32 mask;
    };
    static Mask s_selectionMasks[] =
    {
        { tr("Select All"), static_cast<uint32>(OBJTYPE_ANY) },
        { tr("Brushes"), OBJTYPE_BRUSH },
        { tr("No Brushes"), static_cast<uint32>(~OBJTYPE_BRUSH) },
        { tr("Entities"), OBJTYPE_ENTITY },
        { tr("Prefabs"), OBJTYPE_PREFAB },
        { tr("Areas, Shapes"), OBJTYPE_VOLUME | OBJTYPE_SHAPE },
        { tr("AI Points"), OBJTYPE_AIPOINT },
        { tr("Decals"), OBJTYPE_DECAL },
        { tr("Solids"), OBJTYPE_SOLID },
        { tr("No Solids"), static_cast<uint32>(~OBJTYPE_SOLID) },
    };

    QComboBox* cb = new QComboBox(this);
    for (const Mask& m : s_selectionMasks)
    {
        cb->addItem(m.text, m.mask);
    }
    cb->setCurrentIndex(0);

    connect(cb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [](int index)
    {
        if (index >= 0 && index < sizeof(s_selectionMasks))
        {
            gSettings.objectSelectMask = s_selectionMasks[index].mask;
        }
    });

    QAction* ac = m_actionManager->GetAction(ID_EDIT_NEXTSELECTIONMASK);
    connect(ac, &QAction::triggered, cb, [cb]
    {
        // cycle the combo-box
        const int currentIndex = qMax(0, cb->currentIndex()); // if -1 assume 0
        const int nextIndex = (currentIndex + 1) % cb->count();
        cb->setCurrentIndex(nextIndex);
    });

    // KDAB_TODO, we should monitor when gSettings.objectSelectMask changes, and update the combo-box.
    // I don't think this normally can happen, but was something the MFC code did.

    return cb;
}

QComboBox* MainWindow::CreateRefCoordComboBox()
{
    // ID_REF_COORDS_SYS;
    auto coordSysCombo = new RefCoordComboBox(this);

    connect(this, &MainWindow::ToggleRefCoordSys, coordSysCombo, &RefCoordComboBox::ToggleRefCoordSys);
    connect(this, &MainWindow::UpdateRefCoordSys, coordSysCombo, &RefCoordComboBox::UpdateRefCoordSys);

    return coordSysCombo;
}

RefCoordComboBox::RefCoordComboBox(QWidget* parent)
    : QComboBox(parent)
{
    addItems(coordSysList());
    setCurrentIndex(0);

    connect(this, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [](int index)
    {
        if (index >= 0 && index < LAST_COORD_SYSTEM)
        {
            RefCoordSys coordSys = (RefCoordSys)index;
            if (GetIEditor()->GetReferenceCoordSys() != index)
            {
                GetIEditor()->SetReferenceCoordSys(coordSys);
            }
        }
    });

    UpdateRefCoordSys();
}

QStringList RefCoordComboBox::coordSysList() const
{
    static QStringList list = { tr("View"), tr("Local"), tr("Parent"), tr("World"), tr("Custom") };
    return list;
}

void RefCoordComboBox::UpdateRefCoordSys()
{
    RefCoordSys coordSys = GetIEditor()->GetReferenceCoordSys();
    if (coordSys >= 0 && coordSys < LAST_COORD_SYSTEM)
    {
        setCurrentIndex(coordSys);
    }
}

void RefCoordComboBox::ToggleRefCoordSys()
{
    QStringList coordSys = coordSysList();
    const int localIndex = coordSys.indexOf(tr("Local"));
    const int worldIndex = coordSys.indexOf(tr("World"));
    const int newIndex = currentIndex() == localIndex ? worldIndex : localIndex;
    setCurrentIndex(newIndex);
}

QWidget* MainWindow::CreateSelectObjectComboBox()
{
    // IDC_SELECTION
    auto selectionCombo = new SelectionComboBox(m_actionManager->GetAction(ID_SELECT_OBJECT), this);
    selectionCombo->setObjectName("SelectionComboBox");
    connect(this, &MainWindow::DeleteSelection, selectionCombo, &SelectionComboBox::DeleteSelection);
    return selectionCombo;
}

QToolButton* MainWindow::CreateUndoRedoButton(int command)
{
    // We do either undo or redo below, sort that out here
    UndoRedoDirection direction = UndoRedoDirection::Undo;
    auto stateSignal = &UndoStackStateAdapter::UndoAvailable;
    if (ID_REDO == command)
    {
        direction = UndoRedoDirection::Redo;
        stateSignal = &UndoStackStateAdapter::RedoAvailable;
    }

    auto button = new UndoRedoToolButton(this);
    button->setAutoRaise(true);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setDefaultAction(m_actionManager->GetAction(command));

    QMenu* menu = new QMenu(button);
    auto action = new QWidgetAction(button);
    auto undoRedo = new CUndoDropDown(direction, button);
    action->setDefaultWidget(undoRedo);
    menu->addAction(action);
    button->setMenu(menu);

    connect(menu, &QMenu::aboutToShow, undoRedo, &CUndoDropDown::Prepare);
    connect(undoRedo, &CUndoDropDown::accepted, menu, &QMenu::hide);
    connect(m_undoStateAdapter, stateSignal, button, &UndoRedoToolButton::Update);

    button->setEnabled(false);

    return button;
}

QToolButton* MainWindow::CreateEnvironmentModeButton()
{
    QToolButton* environmentModeButton = new QToolButton(this);
    environmentModeButton->setAutoRaise(true);
    environmentModeButton->setPopupMode(QToolButton::InstantPopup);
    environmentModeButton->setIcon(EditorProxyStyle::icon("Environment"));
    environmentModeButton->setStatusTip(tr("Select from a variety of environment mode options"));
    environmentModeButton->setToolTip(tr("Environment modes"));

    CVarMenu* environmentModeMenu = new CVarMenu(this);
    connect(environmentModeMenu, &QMenu::aboutToShow, [this, environmentModeMenu]()
        {
            InitEnvironmentModeMenu(environmentModeMenu);
        });
    environmentModeButton->setMenu(environmentModeMenu);

    return environmentModeButton;
}

QToolButton* MainWindow::CreateDebugModeButton()
{
    QToolButton* debugModeButton = new QToolButton(this);
    debugModeButton->setAutoRaise(true);
    debugModeButton->setPopupMode(QToolButton::InstantPopup);
    debugModeButton->setIcon(EditorProxyStyle::icon("Debugging"));
    debugModeButton->setStatusTip(tr("Select from a variety of debug/view mode options"));
    debugModeButton->setToolTip(tr("Debug modes"));

    CVarMenu* debugModeMenu = new CVarMenu(this);
    connect(debugModeMenu, &QMenu::aboutToShow, [this, debugModeMenu]()
        {
            InitDebugModeMenu(debugModeMenu);
        });
    debugModeButton->setMenu(debugModeMenu);

    return debugModeButton;
}

void MainWindow::InitEnvironmentModeMenu(CVarMenu* environmentModeMenu)
{
    environmentModeMenu->clear();
    environmentModeMenu->AddCVarToggleItem({ "e_Fog", tr("Hide Global Fog"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "r_FogVolumes", tr("Hide Fog Volumes"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_Clouds", tr("Hide Clouds"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_Wind", tr("Hide Wind"), 0, 1 });
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddCVarToggleItem({ "e_Sun", tr("Hide Sun"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_Skybox", tr("Hide Skybox"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "r_SSReflections", tr("Hide Screen Space Reflection"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_Shadows", tr("Hide Shadows"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "r_TransparentPasses", tr("Hide Transparent Objects"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "r_ssdo", tr("Hide Screen Space Directional Occlusion"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_DynamicLights", tr("Hide All Dynamic Lights"), 0, 1 });
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddCVarValuesItem("e_TimeOfDay", tr("Time of Day"),
        {
            {tr("Day (1:00 pm)"), 13},
            {tr("Night (9:00 pm)"), 21}
        }, 9);
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddCVarToggleItem({ "e_Entities", tr("Hide Entities"), 0, 1 });
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddCVarToggleItem({ "e_Vegetation", tr("Hide Vegetation"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_Terrain", tr("Hide Terrain"), 0, 1 });
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddCVarToggleItem({ "e_Particles", tr("Hide Particles"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_Flares", tr("Hide Flares"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_Decals", tr("Hide Decals"), 0, 1 });
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddCVarToggleItem({ "e_WaterOcean", tr("Hide Ocean Water (for legacy)"), 0, 1 });
    environmentModeMenu->AddCVarToggleItem({ "e_WaterVolumes", tr("Hide Water Volumes"), 0, 1 });
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddCVarToggleItem({ "e_BBoxes", tr("Hide BBoxes"), 0, 1 });
    environmentModeMenu->AddSeparator();
    environmentModeMenu->AddResetCVarsItem();
}

void MainWindow::InitDebugModeMenu(CVarMenu* debugModeMenu)
{
    debugModeMenu->clear();
    debugModeMenu->AddCVarValuesItem("r_DebugGBuffer", tr("GBuffers"),
        {
            {tr("Full Shading Mode (Default)"), 0},
            {tr("Normal Visualization"), 1},
            {tr("Smoothness"), 2},
            {tr("Reflectance"), 3},
            {tr("Albedo"), 4},
            {tr("Lighting Model"), 5},
            {tr("Translucency"), 6},
            {tr("Sun Self Shadowing"), 7},
            {tr("Subsurface Scattering"), 8},
            {tr("Specular Validation Overlay"), 9}
        }, 0);
    debugModeMenu->AddSeparator();
    debugModeMenu->AddCVarValuesItem("r_Stats", tr("Profiling"),
        {
            {tr("Frame Timing"), 1},
            {tr("Object Timing"), 3},
            {tr("Instance Draw Calls"), 6},
        }, 0);
    debugModeMenu->AddSeparator();
    debugModeMenu->AddUniqueCVarsItem(tr("Wireframe"),
        {
            {"r_wireframe", tr("Wireframe Rendering Mode"), 1, 0},
            {"r_showlines", tr("Wireframe Overlay"), 1, 0}
        }),
    debugModeMenu->AddCVarValuesItem("e_debugdraw", tr("Art Info"),
        {
            {tr("Texture Memory Usage"), 4},
            {tr("Renderable Material Count"), 5},
            {tr("LOD Vertex Count"), 22}
        }, 0);

    debugModeMenu->AddSeparator();
    debugModeMenu->AddCVarValuesItem("e_defaultmaterial", tr("Default Material on all Objects"),
        {
            {tr("Gray Material with Normal Maps"), 1},
        }, 0);

    debugModeMenu->AddCVarValuesItem("r_DeferredShadingTiledDebugAlbedo", tr("Debug Visualization of Deferred Lighting"),
        {
            {tr("White Albedo"), 1},
        }, 0);

    debugModeMenu->AddCVarToggleItem({ "r_ShowTangents", tr("Show Tangents"), 1, 0 });
    debugModeMenu->AddCVarToggleItem({ "p_draw_helpers", tr("Show Collision Shapes (Proxy)"), 1, 0 });

    debugModeMenu->AddSeparator();
    debugModeMenu->AddResetCVarsItem();
}

UndoRedoToolButton::UndoRedoToolButton(QWidget* parent)
    : QToolButton(parent)
{
}

void UndoRedoToolButton::Update(int count)
{
    setEnabled(count > 0);
}

QWidget* MainWindow::CreateSnapToGridWidget()
{
    SnapToWidget::SetValueCallback setCallback = [](double snapStep)
    {
        GetIEditor()->GetViewManager()->GetGrid()->size = snapStep;
    };

    SnapToWidget::GetValueCallback getCallback = []()
    {
        return GetIEditor()->GetViewManager()->GetGrid()->size;
    };

    return new SnapToWidget(m_actionManager->GetAction(ID_SNAP_TO_GRID), setCallback, getCallback);
}

QWidget* MainWindow::CreateSnapToAngleWidget()
{
    SnapToWidget::SetValueCallback setCallback = [](double snapAngle)
    {
        GetIEditor()->GetViewManager()->GetGrid()->angleSnap = snapAngle;
    };

    SnapToWidget::GetValueCallback getCallback = []()
    {
        return GetIEditor()->GetViewManager()->GetGrid()->angleSnap;
    };

    return new SnapToWidget(m_actionManager->GetAction(ID_SNAPANGLE), setCallback, getCallback);
}

bool MainWindow::IsPreview() const
{
    return GetIEditor()->IsInPreviewMode();
}

int MainWindow::SelectRollUpBar(int rollupBarId)
    {
    // If we are in legacy UI mode, just grab the rollup bar directly
    const QtViewPane* pane = nullptr;
    if (m_enableLegacyCryEntities)
    {
        pane = m_viewPaneManager->OpenPane(LyViewPane::LegacyRollupBar);
    }
    // Otherwise, we only have the terrain tool
    else if (rollupBarId == ROLLUP_TERRAIN)
    {
        pane = m_viewPaneManager->OpenPane(LyViewPane::TerrainTool);
    }

    if (!pane)

    {
        // TODO: This needs to be replaced with an equivalent workflow when the
        // rollupbar functionality has been replaced
        return -1;
    }

    // We only need to set the proper index when in legacy mode, since in
    // Cry-Free mode there's only the terrain tool, whereas in legacy
    // we actually need to open the proper index in the rollupbar
    if (m_enableLegacyCryEntities)
    {
        CRollupBar* rollup = qobject_cast<CRollupBar*>(pane->Widget());
        if (rollup)
        {
            rollup->setCurrentIndex(rollupBarId);
        }
    }

    return rollupBarId;
        }

QRollupCtrl* MainWindow::GetRollUpControl(int rollupBarId)
{
    // If we are in legacy UI mode, just grab the rollup bar directly
    QtViewPane* pane = nullptr;
    if (m_enableLegacyCryEntities)
    {
        pane = m_viewPaneManager->GetPane(LyViewPane::LegacyRollupBar);
    }
    // Otherwise, we only have the terrain tool
    else if (rollupBarId == ROLLUP_TERRAIN)
    {
        pane = m_viewPaneManager->GetPane(LyViewPane::TerrainTool);
    }

    if (!pane)
    {
        // TODO: This needs to be replaced with an equivalent workflow when the
        // rollupbar functionality has been replaced
        return nullptr;
    }

    // In legacy UI mode, we need to find the proper control from the rollupbar
    QRollupCtrl* ctrl = nullptr;
    if (m_enableLegacyCryEntities)
    {
        CRollupBar* rollup = qobject_cast<CRollupBar*>(pane->Widget());
        if (rollup)
        {
            ctrl = rollup->GetRollUpControl(rollupBarId);
        }
    }
    // Otherwise, our terrain tool is the actual rollup control
    else
    {
        ctrl = qobject_cast<CTerrainTool*>(pane->Widget());
    }

    return ctrl;
}

MainStatusBar* MainWindow::StatusBar() const
{
    assert(statusBar()->inherits("MainStatusBar"));
    return static_cast<MainStatusBar*>(statusBar());
}

void MainWindow::OnUpdateSnapToGrid(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    bool bEnabled = gSettings.pGrid->IsEnabled();
    action->setChecked(bEnabled);

    float gridSize = gSettings.pGrid->size;
    action->setText(QObject::tr("Snap To Grid"));
}

KeyboardCustomizationSettings* MainWindow::GetShortcutManager() const
{
    return m_keyboardCustomization;
}

ActionManager* MainWindow::GetActionManager() const
{
    return m_actionManager;
}

void MainWindow::OpenViewPane(int paneId)
{
    OpenViewPane(QtViewPaneManager::instance()->GetPane(paneId));
}

void MainWindow::OpenViewPane(QtViewPane* pane)
{
    if (pane && pane->IsValid())
    {
        QtViewPaneManager::instance()->OpenPane(pane->m_name);
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "Invalid pane" << pane->m_id << pane->m_category << pane->m_name;
    }
}

void MainWindow::AdjustToolBarIconSize(AzQtComponents::ToolBar::ToolBarIconSize size)
{
    const QList<QToolBar*> toolbars = findChildren<QToolBar*>();

    // make sure to set this back, so that the general settings page matches up with what the size is too
    if (gSettings.gui.nToolbarIconSize != static_cast<int>(size))
    {
        gSettings.gui.nToolbarIconSize = static_cast<int>(size);
    }

    for (auto toolbar : toolbars)
    {
        AzQtComponents::ToolBar::setToolBarIconSize(toolbar, size);
    }
}

void MainWindow::OnGameModeChanged(bool inGameMode)
{
    auto setRollUpBarDisabled = [this](bool disabled)
    {
        auto rollUpPane = m_viewPaneManager->GetPane(LyViewPane::LegacyRollupBar);
        if (rollUpPane && rollUpPane->Widget())
        {
            rollUpPane->Widget()->setDisabled(disabled);
        }
    };

    menuBar()->setDisabled(inGameMode);
    m_toolbarManager->SetEnabled(!inGameMode);
    setRollUpBarDisabled(inGameMode);
    QAction* action = m_actionManager->GetAction(ID_VIEW_SWITCHTOGAME);
    action->blockSignals(true); // avoid a loop
    action->setChecked(inGameMode);
    action->blockSignals(false);
}

void MainWindow::OnEditorNotifyEvent(EEditorNotifyEvent ev)
{
    switch (ev)
    {
    case eNotify_OnEndSceneOpen:
    case eNotify_OnEndSceneSave:
    {
        auto cryEdit = CCryEditApp::instance();
        if (cryEdit)
        {
            cryEdit->SetEditorWindowTitle(0, 0, GetIEditor()->GetGameEngine()->GetLevelName());
        }
    }
    break;
    case eNotify_OnCloseScene:
    {
        auto cryEdit = CCryEditApp::instance();
        if (cryEdit)
        {
            cryEdit->SetEditorWindowTitle();
        }
    }
    break;
    case eNotify_OnRefCoordSysChange:
        emit UpdateRefCoordSys();
        break;
    case eNotify_OnInvalidateControls:
        InvalidateControls();
        break;
    case eNotify_OnBeginGameMode:
        OnGameModeChanged(true);
        break;
    case eNotify_OnEndGameMode:
        OnGameModeChanged(false);
        break;
    // Remove track view option to avoid starting in bad state
    case eNotify_OnBeginSimulationMode:
        if (m_actionManager->HasAction(ID_OPEN_TRACKVIEW))
        {
            QAction* tvAction = m_actionManager->GetAction(ID_OPEN_TRACKVIEW);
            if (tvAction)
            {
                tvAction->setVisible(false);
            }
        }
        break;
    case eNotify_OnEndSimulationMode:
        if (m_actionManager->HasAction(ID_OPEN_TRACKVIEW))
        {
            QAction* tvAction = m_actionManager->GetAction(ID_OPEN_TRACKVIEW);
            if (tvAction)
            {
                tvAction->setVisible(true);
            }
        }
        break;
    }

    switch (ev)
    {
    case eNotify_OnBeginSceneOpen:
    case eNotify_OnBeginNewScene:
    case eNotify_OnCloseScene:
        ResetAutoSaveTimers();
        break;
    case eNotify_OnEndSceneOpen:
    case eNotify_OnEndNewScene:
        ResetAutoSaveTimers(true);
        break;
    }
}

SelectionComboBox::SelectionComboBox(QAction* action, QWidget* parent)
    : AzQtComponents::ToolButtonComboBox(parent)
{
    // We don't do fit to content, otherwise it would jump
    setFixedWidth(85);
    setIcon(EditorProxyStyle::icon("Object_list"));
    button()->setDefaultAction(action);
    QStringList names;
    GetIEditor()->GetObjectManager()->GetNameSelectionStrings(names);
    for (const QString& name : names)
    {
        comboBox()->addItem(name);
    }
}

void SelectionComboBox::DeleteSelection()
{
    QString selString = comboBox()->currentText();
    if (selString.isEmpty())
    {
        return;
    }

    CUndo undo("Del Selection Group");
    GetIEditor()->BeginUndo();
    GetIEditor()->GetObjectManager()->RemoveSelection(selString);
    GetIEditor()->SetModifiedFlag();
    GetIEditor()->SetModifiedModule(eModifiedBrushes);
    GetIEditor()->Notify(eNotify_OnInvalidateControls);

    const int numItems = comboBox()->count();
    for (int i = 0; i < numItems; ++i)
    {
        if (comboBox()->itemText(i) == selString)
        {
            comboBox()->setCurrentText(QString());
            comboBox()->removeItem(i);
            break;
        }
    }
}

void MainWindow::InvalidateControls()
{
    emit UpdateRefCoordSys();
}

void MainWindow::RegisterStdViewClasses()
{
    AzAssetBrowserWindow::createListenerForShowAssetEditorEvent(this);

    CRollupBar::RegisterViewClass();
    CTrackViewDialog::RegisterViewClass();
    CDataBaseDialog::RegisterViewClass();
    CSmartObjectsEditorDialog::RegisterViewClass();
    CAIDebugger::RegisterViewClass();
    CSelectObjectDlg::RegisterViewClass();
    CDialogEditorDialog::RegisterViewClass();
    CVisualLogWnd::RegisterViewClass();
    CAssetBrowserDialog::RegisterViewClass();
    CErrorReportDialog::RegisterViewClass();
    CPanelDisplayLayer::RegisterViewClass();
    CPythonScriptsDialog::RegisterViewClass();
    CMissingAssetDialog::RegisterViewClass();

    CScriptTermDialog::RegisterViewClass();
    CMeasurementSystemDialog::RegisterViewClass();
    CConsoleSCB::RegisterViewClass();
    ConsoleVariableEditor::RegisterViewClass();
    CSettingsManagerDialog::RegisterViewClass();
    AzAssetBrowserWindow::RegisterViewClass();
    AssetEditorWindow::RegisterViewClass();
    CVegetationDataBasePage::RegisterViewClass();

#ifndef OTHER_ACTIVE
    CMaterialDialog::RegisterViewClass();
    CLensFlareEditor::RegisterViewClass();
    CTimeOfDayDialog::RegisterViewClass();
    CTerrainTool::RegisterViewClass();
#ifdef LY_TERRAIN_EDITOR
    CTerrainDialog::RegisterViewClass();
    CTerrainTextureDialog::RegisterViewClass();
#endif //#ifdef LY_TERRAIN_EDITOR
    CTerrainLighting::RegisterViewClass();
#endif
#ifdef ThumbnailDemo
    ThumbnailsSampleWidget::RegisterViewClass();
#endif

    if (gEnv->pGame && gEnv->pGame->GetIGameFramework())
    {
        CMannequinDialog::RegisterViewClass();
    }

    //These view dialogs aren't used anymore so they became disabled.
    //CLightmapCompilerDialog::RegisterViewClass();
    //CLightmapCompilerDialog::RegisterViewClass();

    // Notify that views can now be registered
    EBUS_EVENT(AzToolsFramework::EditorEvents::Bus, NotifyRegisterViews);
}

void MainWindow::OnCustomizeToolbar()
{
    /* TODO_KDAB, rest of CMainFrm::OnCustomize() goes here*/
    SaveConfig();
}

void MainWindow::RefreshStyle()
{
    GetIEditor()->Notify(eNotify_OnStyleChanged);
}

void MainWindow::ResetAutoSaveTimers(bool bForceInit)
{
    if (m_autoSaveTimer)
    {
        delete m_autoSaveTimer;
    }
    if (m_autoRemindTimer)
    {
        delete m_autoRemindTimer;
    }
    m_autoSaveTimer = 0;
    m_autoRemindTimer = 0;

    if (bForceInit)
    {
        if (gSettings.autoBackupTime > 0 && gSettings.autoBackupEnabled)
        {
            m_autoSaveTimer = new QTimer(this);
            m_autoSaveTimer->start(gSettings.autoBackupTime * 1000 * 60);
            connect(m_autoSaveTimer, &QTimer::timeout, this, [&]() {
                if (gSettings.autoBackupEnabled)
                {
                    // Call autosave function of CryEditApp
                    GetIEditor()->GetDocument()->SaveAutoBackup();
                }
            });
        }
        if (gSettings.autoRemindTime > 0)
        {
            m_autoRemindTimer = new QTimer(this);
            m_autoRemindTimer->start(gSettings.autoRemindTime * 1000 * 60);
            connect(m_autoRemindTimer, &QTimer::timeout, this, [&]() {
                if (gSettings.autoRemindTime > 0)
                {
                    // Remind to save.
                    CCryEditApp::instance()->SaveAutoRemind();
                }
            });
        }
    }

}

void MainWindow::ResetBackgroundUpdateTimer()
{
    if (m_backgroundUpdateTimer)
    {
        delete m_backgroundUpdateTimer;
        m_backgroundUpdateTimer = 0;
    }

    ICVar* pBackgroundUpdatePeriod = gEnv->pConsole->GetCVar("ed_backgroundUpdatePeriod");
    if (pBackgroundUpdatePeriod && pBackgroundUpdatePeriod->GetIVal() > 0)
    {
        m_backgroundUpdateTimer = new QTimer(this);
        m_backgroundUpdateTimer->start(pBackgroundUpdatePeriod->GetIVal());
        connect(m_backgroundUpdateTimer, &QTimer::timeout, this, [&]() {
            // Make sure that visible editor window get low-fps updates while in the background

            CCryEditApp* pApp = CCryEditApp::instance();
            if (!isMinimized() && !pApp->IsWindowInForeground())
            {
                pApp->IdleProcessing(true);
            }
        });
    }
}

void MainWindow::UpdateToolsMenu()
{
     m_levelEditorMenuHandler->UpdateMacrosMenu();
}

int MainWindow::ViewPaneVersion() const
{
    return m_levelEditorMenuHandler->GetViewPaneVersion();
}

void MainWindow::OnStopAllSounds()
{
    Audio::SAudioRequest oStopAllSoundsRequest;
    Audio::SAudioManagerRequestData<Audio::eAMRT_STOP_ALL_SOUNDS>   oStopAllSoundsRequestData;
    oStopAllSoundsRequest.pData = &oStopAllSoundsRequestData;

    CryLogAlways("<Audio> Executed \"Stop All Sounds\" command.");
    Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequest, oStopAllSoundsRequest);
}

void MainWindow::OnRefreshAudioSystem()
{
    QString sLevelName = GetIEditor()->GetGameEngine()->GetLevelName();

    if (QString::compare(sLevelName, "Untitled", Qt::CaseInsensitive) == 0)
    {
        // Rather pass NULL to indicate that no level is loaded!
        sLevelName = QString();
    }

    Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::RefreshAudioSystem, sLevelName.toUtf8().data());
}

void MainWindow::SaveLayout()
{
    const int MAX_LAYOUTS = ID_VIEW_LAYOUT_LAST - ID_VIEW_LAYOUT_FIRST + 1;

    if (m_viewPaneManager->LayoutNames(true).count() >= MAX_LAYOUTS)
    {
        QMessageBox::critical(this, tr("Maximum number of layouts reached"), tr("Please delete a saved layout before creating another."));
        return;
    }

    QString layoutName = QInputDialog::getText(this, tr("Layout Name"), QString()).toLower();
    if (layoutName.isEmpty())
    {
        return;
    }

    if (m_viewPaneManager->HasLayout(layoutName))
    {
        QMessageBox box(this); // Not static so we can remove help button
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setText(tr("Overwrite Layout?"));
        box.setIcon(QMessageBox::Warning);
        box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
        box.setInformativeText(tr("The chosen layout name already exists. Do you want to overwrite it?"));
        if (box.exec() != QMessageBox::Yes)
        {
            SaveLayout();
            return;
        }
    }

    m_viewPaneManager->SaveLayout(layoutName);
}

void MainWindow::ViewDeletePaneLayout(const QString& layoutName)
{
    if (layoutName.isEmpty())
    {
        return;
    }

    QMessageBox box(this); // Not static so we can remove help button
    box.setText(tr("Delete Layout?"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setIcon(QMessageBox::Warning);
    box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    box.setInformativeText(tr("Are you sure you want to delete the layout '%1'?").arg(layoutName));
    if (box.exec() == QMessageBox::Yes)
    {
        m_viewPaneManager->RemoveLayout(layoutName);
    }
}

void MainWindow::ViewRenamePaneLayout(const QString& layoutName)
{
    if (layoutName.isEmpty())
    {
        return;
    }

    QString newLayoutName;
    bool validName = false;
    while (!validName)
    {
        newLayoutName = QInputDialog::getText(this, tr("Rename layout '%1'").arg(layoutName), QString());
        if (newLayoutName.isEmpty())
        {
            return;
        }

        if (m_viewPaneManager->HasLayout(newLayoutName))
        {
            QMessageBox box(this); // Not static so we can remove help button
            box.setText(tr("Layout name already exists"));
            box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            box.setIcon(QMessageBox::Warning);
            box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
            box.setInformativeText(tr("The layout name '%1' already exists, please choose a different name").arg(newLayoutName));
            if (box.exec() == QMessageBox::No)
            {
                return;
            }
        }
        else
        {
            validName = true;
        }
    }

    m_viewPaneManager->RenameLayout(layoutName, newLayoutName);
}

void MainWindow::ViewLoadPaneLayout(const QString& layoutName)
{
    if (!layoutName.isEmpty())
    {
        m_viewPaneManager->RestoreLayout(layoutName);
    }
}

void MainWindow::ViewSavePaneLayout(const QString& layoutName)
{
    if (layoutName.isEmpty())
    {
        return;
    }

    QMessageBox box(this); // Not static so we can remove help button
    box.setText(tr("Overwrite Layout?"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setIcon(QMessageBox::Warning);
    box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    box.setInformativeText(tr("Do you want to overwrite the layout '%1' with the current one?").arg(layoutName));
    if (box.exec() == QMessageBox::Yes)
    {
        m_viewPaneManager->SaveLayout(layoutName);
    }
}

void MainWindow::OnUpdateConnectionStatus()
{
    auto* statusBar = StatusBar();

    if (!m_connectionListener)
    {
        statusBar->SetItem("connection", tr("Disconnected"), tr("Disconnected"), IDI_BALL_DISABLED);
        //TODO: disable clicking
    }
    else
    {
        using EConnectionState = EngineConnectionListener::EConnectionState;
        int icon = IDI_BALL_OFFLINE;
        QString tooltip, status;
        switch (m_connectionListener->GetState())
        {
        case EConnectionState::Connecting:
            // Checking whether we are not connected here instead of disconnect state because this function is called on a timer
            // and therefore we may not receive the disconnect state.
            if (m_connectedToAssetProcessor)
            {
                m_connectedToAssetProcessor = false;
                m_showAPDisconnectDialog = true;
            }
            tooltip = tr("Connecting to Asset Processor");
            icon = IDI_BALL_PENDING;
            break;
        case EConnectionState::Disconnecting:
            tooltip = tr("Disconnecting from Asset Processor");
            icon = IDI_BALL_PENDING;
            break;
        case EConnectionState::Listening:
            if (m_connectedToAssetProcessor)
            {
                m_connectedToAssetProcessor = false;
                m_showAPDisconnectDialog = true;
            }
            tooltip = tr("Listening for incoming connections");
            icon = IDI_BALL_PENDING;
            break;
        case EConnectionState::Connected:
            m_connectedToAssetProcessor = true;
            tooltip = tr("Connected to Asset Processor");
            icon = IDI_BALL_ONLINE;
            break;
        case EConnectionState::Disconnected:
            icon = IDI_BALL_OFFLINE;
            tooltip = tr("Disconnected from Asset Processor");
            break;
        }

        if (m_connectedToAssetProcessor)
        {
            m_connectionLostTimer->stop();
        }

        tooltip += "\n Last Asset Processor Task: ";
        tooltip += m_connectionListener->LastAssetProcessorTask().c_str();
        tooltip += "\n";
        AZStd::set<AZStd::string> failedJobs = m_connectionListener->FailedJobsList();
        int failureCount = failedJobs.size();
        if (failureCount)
        {
            tooltip += "\n Failed Jobs\n";
            for (auto failedJob : failedJobs)
            {
                tooltip += failedJob.c_str();
                tooltip += "\n";
            }
        }

        status = tr("Pending Jobs : %1  Failed Jobs : %2").arg(m_connectionListener->GetJobsCount()).arg(failureCount);

        statusBar->SetItem(QtUtil::ToQString("connection"), status, tooltip, icon);

        if (m_showAPDisconnectDialog && m_connectionListener->GetState() != EConnectionState::Connected)
        {
            m_showAPDisconnectDialog = false;// Just show the dialog only once if connection is lost
            m_connectionLostTimer->setSingleShot(true);
            m_connectionLostTimer->start(15000);
        }
    }
}

void MainWindow::ShowConnectionDisconnectedDialog()
{
    // when REMOTE_ASSET_PROCESSOR is undef'd it means behave as if there is no such thing as the remote asset processor.
#ifdef REMOTE_ASSET_PROCESSOR
    if (gEnv && gEnv->pSystem)
    {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Asset Processor has disconnected."));
        messageBox.setText(
            tr("Asset Processor is not connected. Please try (re)starting the Asset Processor or restarting the Editor.<br><br>"
                "Data may be lost while the Asset Processor is not running!<br>"
                "The status of the Asset Processor can be monitored from the editor in the bottom-right corner of the status bar.<br><br>"
                "Would you like to start the asset processor?<br>"));
        messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Ignore);
        messageBox.setDefaultButton(QMessageBox::Yes);
        messageBox.setIcon(QMessageBox::Critical);
        if (messageBox.exec() == QMessageBox::Yes)
        {
            gEnv->pSystem->LaunchAssetProcessor();
        }
    }
    else
    {
        QMessageBox::critical(this, tr("Asset Processor has disconnected."),
            tr("Asset Processor is not connected. Please try (re)starting the asset processor or restarting the Editor.<br><br>"
                "Data may be lost while the asset processor is not running!<br>"
                "The status of the asset processor can be monitored from the editor in the bottom-right corner of the status bar."));
    }
#endif
}

void MainWindow::OnConnectionStatusClicked()
{
    using namespace AzFramework;
    AssetSystemRequestBus::Broadcast(&AssetSystemRequestBus::Events::ShowAssetProcessor);
}

static bool paneLessThan(const QtViewPane& v1, const QtViewPane& v2)
{
    return v1.m_name.compare(v2.m_name, Qt::CaseInsensitive) < 0;
}

void MainWindow::RegisterOpenWndCommands()
{
    s_openViewCmds.clear();

    auto panes = m_viewPaneManager->GetRegisteredPanes(/* viewPaneMenuOnly=*/ false);
    std::sort(panes.begin(), panes.end(), paneLessThan);

    for (auto viewPane : panes)
    {
        if (viewPane.m_category.isEmpty())
        {
            continue;
        }

        const QString className = viewPane.m_name;

        // Make a open-view command for the class.
        QString classNameLowered = viewPane.m_name.toLower();
        classNameLowered.replace(' ', '_');
        QString openCommandName = "open_";
        openCommandName += classNameLowered;

        CEditorOpenViewCommand* pCmd = new CEditorOpenViewCommand(GetIEditor(), viewPane.m_name);
        s_openViewCmds.push_back(pCmd);

        CCommand0::SUIInfo cmdUI;
        cmdUI.caption = className.toUtf8().data();
        cmdUI.tooltip = (QString("Open ") + className).toUtf8().data();
        cmdUI.iconFilename = className.toUtf8().data();
        GetIEditor()->GetCommandManager()->RegisterUICommand("editor", openCommandName.toUtf8().data(),
            "", "", functor(*pCmd, &CEditorOpenViewCommand::Execute), cmdUI);
        GetIEditor()->GetCommandManager()->GetUIInfo("editor", openCommandName.toUtf8().data(), cmdUI);
    }
}

void MainWindow::MatEditSend(int param)
{
    if (param == eMSM_Init || GetIEditor()->IsInMatEditMode())
    {
        // In MatEditMode this message is handled by CMatEditMainDlg, which doesn't have
        // any view panes and opens MaterialDialog directly.
        return;
    }

    if (QtViewPaneManager::instance()->OpenPane(LyViewPane::MaterialEditor))
    {
        GetIEditor()->GetMaterialManager()->SyncMaterialEditor();
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEventFilter(const QByteArray &eventType, void *message, long *)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_MATEDITSEND) // For supporting 3ds Max Exporter, Windows Only
    {
        MatEditSend(msg->wParam);
        return true;
    }

    return false;
}
#endif

bool MainWindow::event(QEvent* event)
{
#ifdef Q_OS_MAC
    if (event->type() == QEvent::HoverMove)
    {
        // this fixes a problem on macOS where the mouse cursor was not
        // set when hovering over the splitter handles between dock widgets
        // it might be fixed in future Qt versions
        auto mouse = static_cast<QHoverEvent*>(event);
        bool result = QMainWindow::event(event);
        void setCocoaMouseCursor(QWidget*);
        setCocoaMouseCursor(childAt(mouse->pos()));
        return result;
    }
#endif
    return QMainWindow::event(event);
}

void MainWindow::ToggleConsole()
{
    m_viewPaneManager->TogglePane(LyViewPane::Console);

    QtViewPane* pane = m_viewPaneManager->GetPane(LyViewPane::Console);
    if (!pane)
    {
        return;
    }

    // If we toggled the console on, we want to focus its input text field
    if (pane->IsVisible())
    {
        CConsoleSCB* console = qobject_cast<CConsoleSCB*>(pane->Widget());
        if (!console)
        {
            return;
        }

        console->SetInputFocus();
    }
}

void MainWindow::ToggleRollupBar()
{
    m_viewPaneManager->TogglePane(LyViewPane::LegacyRollupBar);
}


void MainWindow::OnViewPaneCreated(const QtViewPane* pane)
{
    QAction* action = nullptr;
    int id = pane->m_id;

    // Use built-in action id if available
    if (pane->m_options.builtInActionId != -1)
    {
        id = pane->m_options.builtInActionId;
    }

    if (m_actionManager->HasAction(id))
    {
        action = m_actionManager->GetAction(id);
        action->setChecked(true);

        connect(pane->m_dockWidget->toggleViewAction(), &QAction::toggled,
            action, &QAction::setChecked, Qt::UniqueConnection);
    }
}

void MainWindow::ConnectivityStateChanged(const AzToolsFramework::SourceControlState state)
{
    bool connected = false;
    ISourceControl* pSourceControl = GetIEditor() ? GetIEditor()->GetSourceControl() : nullptr;
    if (pSourceControl)
    {
        pSourceControl->SetSourceControlState(state);
        if (state == AzToolsFramework::SourceControlState::Active || state == AzToolsFramework::SourceControlState::ConfigurationInvalid)
        {
            connected = true;
        }
    }

    CEngineSettingsManager settingsManager;
    settingsManager.SetModuleSpecificBoolEntry("RC_EnableSourceControl", connected);
    settingsManager.StoreData();

    gSettings.enableSourceControl = connected;
    gSettings.SaveEnableSourceControlFlag(false);
}

void MainWindow::OnGotoSelected()
{
    AzToolsFramework::EditorRequestBus::Broadcast(&AzToolsFramework::EditorRequestBus::Events::GoToSelectedEntitiesInViewports);
}

void MainWindow::OnGotoSliceRoot()
{
    int numViews = GetIEditor()->GetViewManager()->GetViewCount();
    for (int i = 0; i < numViews; ++i)
    {
        CViewport* viewport = GetIEditor()->GetViewManager()->GetView(i);
        if (viewport)
        {
            viewport->CenterOnSliceInstance();
        }
    }
}

void MainWindow::ShowCustomizeToolbarDialog()
{
    if (m_toolbarCustomizationDialog)
    {
        return;
    }

    m_toolbarCustomizationDialog = new ToolbarCustomizationDialog(this);
    m_toolbarCustomizationDialog->show();
}

QMenu* MainWindow::createPopupMenu()
{
    QMenu* menu = QMainWindow::createPopupMenu();
    menu->addSeparator();
    QAction* action = menu->addAction(QStringLiteral("Customize..."));
    connect(action, &QAction::triggered, this, &MainWindow::ShowCustomizeToolbarDialog);
    return menu;
}

ToolbarManager* MainWindow::GetToolbarManager() const
{
    return m_toolbarManager;
}

bool MainWindow::IsCustomizingToolbars() const
{
    return m_toolbarCustomizationDialog != nullptr;
}

QWidget* MainWindow::CreateToolbarWidget(int actionId)
{
    QWidgetAction* action = qobject_cast<QWidgetAction*>(m_actionManager->GetAction(actionId));
    if (!action)
    {
        qWarning() << Q_FUNC_INFO << "No QWidgetAction for actionId = " << actionId;
        return nullptr;
    }

    QWidget* w = nullptr;
    switch (actionId)
    {
    case ID_TOOLBAR_WIDGET_UNDO:
        w = CreateUndoRedoButton(ID_UNDO);
        break;
    case ID_TOOLBAR_WIDGET_REDO:
        w = CreateUndoRedoButton(ID_REDO);
        break;
    case ID_TOOLBAR_WIDGET_SELECTION_MASK:
        w = CreateSelectionMaskComboBox();
        break;
    case ID_TOOLBAR_WIDGET_REF_COORD:
        w = CreateRefCoordComboBox();
        break;
    case ID_TOOLBAR_WIDGET_SNAP_GRID:
        w = CreateSnapToGridWidget();
        break;
    case ID_TOOLBAR_WIDGET_SNAP_ANGLE:
        w = CreateSnapToAngleWidget();
        break;
    case ID_TOOLBAR_WIDGET_SELECT_OBJECT:
        w = CreateSelectObjectComboBox();
        break;
    case ID_TOOLBAR_WIDGET_ENVIRONMENT_MODE:
        w = CreateEnvironmentModeButton();
        break;
    case ID_TOOLBAR_WIDGET_DEBUG_MODE:
        w = CreateDebugModeButton();
        break;
    default:
        qWarning() << Q_FUNC_INFO << "Unknown id " << actionId;
        return nullptr;
    }

    return w;
}


// don't want to eat escape as if it were a shortcut, as it would eat it for other windows that also care about escape
// and are reading it as an event rather.
void MainWindow::keyPressEvent(QKeyEvent* e)
{
    // We shouldn't need to do this, as there's already an escape key shortcut set on an action
    // attached to the MainWindow. We need to explicitly trap the escape key here because when in
    // Game Mode, all of the actions attached to the MainWindow are disabled.
    if (e->key() == Qt::Key_Escape)
    {
       MainWindow::OnEscapeAction();
       return;
    }
    return QMainWindow::keyPressEvent(e);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    using namespace AzQtComponents;
    DragAndDropContextBase context;
    DragAndDropEventsBus::Event(DragAndDropContexts::EditorMainWindow, &DragAndDropEvents::DragEnter, event, context);
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event)
{
    using namespace AzQtComponents;
    DragAndDropContextBase context;
    DragAndDropEventsBus::Event(DragAndDropContexts::EditorMainWindow, &DragAndDropEvents::DragMove, event, context);
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent* event)
{
    using namespace AzQtComponents;
    DragAndDropEventsBus::Event(DragAndDropContexts::EditorMainWindow, &DragAndDropEvents::DragLeave, event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    using namespace AzQtComponents;
    DragAndDropContextBase context;
    DragAndDropEventsBus::Event(DragAndDropContexts::EditorMainWindow, &DragAndDropEvents::Drop, event, context);
}

bool MainWindow::focusNextPrevChild(bool next)
{
    // Don't change the focus when we're in game mode or else the viewport could
    // stop receiving input events
    if (GetIEditor()->IsInGameMode())
    {
        return false;
    }

    return QMainWindow::focusNextPrevChild(next);
}

namespace AzToolsFramework
{
    void MainWindowEditorFuncsHandler::Reflect(AZ::ReflectContext* context)
    {
        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            // this will put these methods into the 'azlmbr.legacy.general' module
            auto addLegacyGeneral = [](AZ::BehaviorContext::GlobalMethodBuilder methodBuilder)
            {
                methodBuilder->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Category, "Legacy/Editor")
                    ->Attribute(AZ::Script::Attributes::Module, "legacy.general");
            };
            addLegacyGeneral(behaviorContext->Method("open_pane", PyOpenViewPane, nullptr, "Opens a view pane specified by the pane class name."));
            addLegacyGeneral(behaviorContext->Method("close_pane", PyCloseViewPane, nullptr, "Closes a view pane specified by the pane class name."));
            addLegacyGeneral(behaviorContext->Method("is_pane_visible", PyIsViewPaneVisible, nullptr, "Returns true if pane specified by the pane class name is visible."));
            addLegacyGeneral(behaviorContext->Method("get_pane_class_names", PyGetViewPaneNames, nullptr, "Get all available class names for use with open_pane & close_pane."));
            addLegacyGeneral(behaviorContext->Method("exit", PyExit, nullptr, "Exits the editor."));
            addLegacyGeneral(behaviorContext->Method("exit_no_prompt", PyExitNoPrompt, nullptr, "Exits the editor without prompting to save first."));
            addLegacyGeneral(behaviorContext->Method("get_status_text", PyGetStatusText, nullptr, "Gets the status text from the Editor's current edit tool"));
        }
    }
}

#include <MainWindow.moc>
