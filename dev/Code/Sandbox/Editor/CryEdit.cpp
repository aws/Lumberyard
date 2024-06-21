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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"

#ifdef WIN32
#include <gdiplus.h>
#pragma comment (lib, "Gdiplus.lib")

#include <WinUser.h> // needed for MessageBoxW in the assert handler
#endif

#include "CryEdit.h"

#include "Include/SandboxAPI.h"
#include "QtUtilWin.h"
#include "GameExporter.h"
#include "GameResourcesExporter.h"
#include "MainWindow.h"
#include "Core/QtEditorApplication.h"
#include "CryEditDoc.h"
#include "ViewPane.h"
#include "StringDlg.h"
#include "SelectObjectDlg.h"
#include "LinkTool.h"
#include "AlignTool.h"
#include "VoxelAligningTool.h"
#include "MissionScript.h"
#include "NewLevelDialog.h"

#include "VegetationMap.h"
#include "GridSettingsDialog.h"
#include "LayoutConfigDialog.h"
#include "IEditorGame.h"
#include "KeyboardCustomizationSettings.h"
#include <AzQtComponents/Components/WindowDecorationWrapper.h>
#include <AzQtComponents/Utilities/QtPluginPaths.h>
#include "ProcessInfo.h"
#include "CheckOutDialog.h"

#include "ViewManager.h"
#include "ModelViewport.h"
#include "RenderViewport.h"
#include "FileTypeUtils.h"

#include "PluginManager.h"

#include "Objects/Group.h"
#include "Objects/AiPoint.h"
#include "Objects/CloudGroup.h"
#include "Objects/CameraObject.h"
#include "Objects/EntityScript.h"
#include "Objects/BrushObject.h"
#include "Objects/GeomEntity.h"

#include "Objects/PrefabObject.h"
#include "Prefabs/PrefabManager.h"
#include "Material/MaterialManager.h"

#include "IEditorImpl.h"
#include "StartupLogoDialog.h"
#include "DisplaySettings.h"
#include "EquipPackDialog.h"
#include "GameEngine.h"
#include "LayersSelectDialog.h"
#include "Mission.h"
#include "MissionSelectDialog.h"
#include "ObjectCloneTool.h"
#include "StartupTraceHandler.h"
#include "ThumbnailGenerator.h"
#include "ToolsConfigPage.h"
#include "TrackView/TrackViewDialog.h"
#include "Undo/Undo.h"

#include "AI/AIManager.h"
#include "AI/GenerateSpawners.h"

#ifdef LY_TERRAIN_EDITOR
#include "NewTerrainDialog.h"
#include "TerrainDialog.h"
#include "TerrainMoveTool.h"
#include "TerrainModifyTool.h"
#include <AzToolsFramework/Component/EditorComponentAPIBus.h>
#include <AzToolsFramework/Component/EditorLevelComponentAPIBus.h>
#endif //#ifdef LY_TERRAIN_EDITOR

// This is the "Sun Trajectory Tool", so it's not directly related to the rest of the Terrain Editor code above.
#include "TerrainLighting.h"

#include "ToolBox.h"
#include "Geometry/EdMesh.h"

#include "LevelInfo.h"
#include "EditorPreferencesDialog.h"
#include "GraphicsSettingsDialog.h"
#include "FeedbackDialog/FeedbackDialog.h"

#include "MatEditMainDlg.h"

#include "Objects/ObjectPhysicsManager.h"

#include <WinWidget/WinWidgetId.h>

#include <IScriptSystem.h>
#include <IEntitySystem.h>
#include <I3DEngine.h>
#include <ITimer.h>
#include <IGame.h>
#include <IGameFramework.h>
#include <IItemSystem.h>
#include <ICryAnimation.h>
#include <IPhysics.h>
#include <IGameRulesSystem.h>
#include <IBackgroundScheduleManager.h>
#include "IEditorGame.h"

#include "TimeOfDayDialog.h"
#include "CryEdit.h"
#include "ShaderCache.h"
#include "GotoPositionDlg.h"
#include "SetVectorDlg.h"

#include "ConsoleDialog.h"
#include "Controls/ConsoleSCB.h"

#include "StringUtils.h"

#include "ScopedVariableSetter.h"

#include "Util/3DConnexionDriver.h"

#include "DimensionsDialog.h"

#ifdef LY_TERRAIN_EDITOR
#include "TerrainTextureExport.h"
#include "Terrain/TerrainManager.h"
#endif //#ifdef LY_TERRAIN_EDITOR

#include "MeasurementSystem/MeasurementSystem.h"

#include "Util/AutoDirectoryRestoreFileDialog.h"
#include "Util/EditorAutoLevelLoadTest.h"
#include "Util/Ruler.h"
#include "Util/IndexedFiles.h"
#include <Util/PathUtil.h>  // for getting the game (editing) folder

#include "AssetBrowser/AssetBrowserManager.h"
#include "IAssetItemDatabase.h"

#include "Mannequin/MannequinChangeMonitor.h"

#include "AboutDialog.h"
#include "ScriptHelpDialog.h"

#include "QuickAccessBar.h"
#include "QtViewPaneManager.h"

#include "Export/ExportManager.h"

#include "LevelFileDialog.h"
#include "LevelIndependentFileMan.h"
#include "WelcomeScreen/WelcomeScreenDialog.h"
#include "Objects/ObjectLayerManager.h"
#include "Dialogs/DuplicatedObjectsHandlerDlg.h"
#include "IconManager.h"
#include "Dialogs/DuplicatedObjectsHandlerDlg.h"
#include "Include/IEventLoopHook.h"
#include "EditMode/VertexSnappingModeTool.h"

#include "UndoViewPosition.h"
#include "UndoViewRotation.h"
#include "UndoConfigSpec.h"

#include "Core/LevelEditorMenuHandler.h"

#include "ParseEngineConfig.h"

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Module/ModuleManagerBus.h>
#include <AzCore/std/containers/map.h>
#include <AzFramework/AzFramework_Traits_Platform.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/Network/AssetProcessorConnection.h>
#include <AzToolsFramework/API/ComponentEntityObjectBus.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/AssetBrowser/AssetSelectionModel.h>
#include <AzToolsFramework/AssetBrowser/Entries/AssetBrowserEntry.h>
#include <AzToolsFramework/UI/Logging/TracePrintFLogPanel.h>
#include <AzToolsFramework/UI/UICore/ProgressShield.hxx>
#include <AzToolsFramework/UI/UICore/WidgetHelpers.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Thumbnails/ThumbnailerComponent.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserComponent.h>
#include <AzToolsFramework/MaterialBrowser/MaterialBrowserComponent.h>
#include <AzToolsFramework/Slice/SliceUtilities.h>
#include <AzToolsFramework/ViewportSelection/EditorTransformComponentSelectionRequestBus.h>
#include <AzToolsFramework/API/EditorPythonConsoleBus.h>
#include <AzToolsFramework/API/EditorPythonRunnerRequestsBus.h>
#include <AzQtComponents/Components/StyleManager.h>
#include <AzQtComponents/Utilities/HandleDpiAwareness.h>

#include <LmbrCentral/Rendering/MeshComponentBus.h>

#include <Controls/ReflectedPropertyControl/PropertyCtrl.h>
#include <Controls/ReflectedPropertyControl/ReflectedVar.h>
#include <LegacyEntityConversion/LegacyEntityConversionBus.h>

// Confetti Begin: Jurecka
#include "Include/IParticleEditor.h"
// Confetti End: Jurecka

#include <QtUtil.h>
#include <QtUtilWin.h>

#include <QAction>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QDialogButtonBox>
#include <QProcess>
#include <QFile>
#include <QString>
#include <QMenuBar>
#include <QUrl>
#include <QUrlQuery>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QApplication>
#include <QDesktopServices>
#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QSurfaceFormat>
#include <QLocale>
#include <QProgressBar>
#include <QStandardItemModel>
#include <QTreeView>
#include <QMessageBox>
#include <QClipboard>
#include <QElapsedTimer>
#include <QScopedValueRollback>
#include <ILevelSystem.h>

#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Module/Environment.h>
#include <AzToolsFramework/Undo/UndoSystem.h>

#include <Amazon/Login.h>
#include <CloudCanvas/ICloudCanvasEditor.h>
#include <AWSNativeSDKInit/AWSNativeSDKInit.h>

#include <aws/sts/STSClient.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/sts/model/GetFederationTokenRequest.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <array>
#include <string>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Metrics/LyEditorMetricsBus.h>

#include "Core/EditorMetricsPlainTextNameRegistration.h"
#include "EditorToolsApplication.h"

#include "../Plugins/EditorUI_QT/EditorUI_QTAPI.h"
#include "../ComponentEntityEditorPlugin/Objects/ComponentEntityObject.h"

#include <Engines/EnginesAPI.h> // For LyzardSDK

#if defined(AZ_PLATFORM_WINDOWS)
#include <AzFramework/API/ApplicationAPI_Platform.h>
#endif

#if AZ_TRAIT_OS_PLATFORM_APPLE
#include "WindowObserver_mac.h"
#endif

#include <AzCore/RTTI/BehaviorContext.h>
#if !ENABLE_CRY_PHYSICS
#include <AzFramework/Render/Intersector.h>
#endif

static const char defaultFileExtension[] = ".ly";
static const char oldFileExtension[] = ".cry";

static const char lumberyardEditorClassName[] = "LumberyardEditorClass";
static const char lumberyardApplicationName[] = "LumberyardApplication";

static AZ::EnvironmentVariable<bool> inEditorBatchMode = nullptr;

RecentFileList::RecentFileList()
{
    m_settings.beginGroup(QStringLiteral("Application"));
    m_settings.beginGroup(QStringLiteral("Recent File List"));

    ReadList();
}

void RecentFileList::Remove(int index)
{
    m_arrNames.removeAt(index);
}

void RecentFileList::Add(const QString& f)
{
    QString filename = QDir::toNativeSeparators(f);
    m_arrNames.removeAll(filename);
    m_arrNames.push_front(filename);
    while (m_arrNames.count() > Max)
    {
        m_arrNames.removeAt(Max);
    }
}

int RecentFileList::GetSize()
{
    return m_arrNames.count();
}

void RecentFileList::GetDisplayName(QString& name, int index, const QString& curDir)
{
    name = m_arrNames[index];

    const QDir cur(curDir);
    QDir fileDir(name); // actually pointing at file, first cdUp() gets us the parent dir
    while (fileDir.cdUp())
    {
        if (fileDir == cur)
        {
            name = cur.relativeFilePath(name);
            break;
        }
    }

    name = QDir::toNativeSeparators(name);
}

QString& RecentFileList::operator[](int index)
{
    return m_arrNames[index];
}

void RecentFileList::ReadList()
{
    m_arrNames.clear();

    for (int i = 1; i <= Max; ++i)
    {
        QString f = m_settings.value(QStringLiteral("File%1").arg(i)).toString();
        if (!f.isEmpty())
        {
            m_arrNames.push_back(f);
        }
    }
}

void RecentFileList::WriteList()
{
    m_settings.remove(QString());

    int i = 1;
    for (auto f : m_arrNames)
    {
        m_settings.setValue(QStringLiteral("File%1").arg(i++), f);
    }
}


//////////////////////////////////////////////////////////////////////////
namespace
{
    static const char* s_CryEditAppInstanceName = "CryEditAppInstanceName";

    void PyRunFile(const AZ::StringSet& args)
    {
        if (args.empty())
        {
            // We expect at least "pyRunFile" and the filename
            AZ_Warning("editor", false, "The pyRunFile requires a file script name.");
            return;
        }
        else if (args.size() == 1)
        {
            // If we only have "pyRunFile filename", there are no args to pass through.
            AzToolsFramework::EditorPythonRunnerRequestBus::Broadcast(
                &AzToolsFramework::EditorPythonRunnerRequestBus::Events::ExecuteByFilename,
                args.front().c_str());
        }
        else
        {
            // We have "pyRunFile filename x y z", so copy everything past filename into a new vector
            AZStd::vector<AZStd::string_view> pythonArgs;
            AZStd::transform(args.begin() + 1, args.end(), std::back_inserter(pythonArgs), [](auto&& value) { return value.c_str(); });
            AzToolsFramework::EditorPythonRunnerRequestBus::Broadcast(
                &AzToolsFramework::EditorPythonRunnerRequestBus::Events::ExecuteByFilenameWithArgs,
                args[0],
                pythonArgs);
        }
    }
    AZ_CONSOLEFREEFUNC("pyRunFile", PyRunFile, AZ::ConsoleFunctorFlags::Null, "Runs the Python script from the console.");

    //! We have explicitly not exposed this CloseCurrentLevel API to Python Scripting since the Editor
    //! doesn't officially support it (it doesn't exist in the File menu). It is used for cases
    //! where a level with legacy entities are unable to be converted and so the level that has
    //! been opened needs to be closed, but it hasn't been fully tested for a normal workflow.
    void CloseCurrentLevel()
    {
        CCryEditDoc* currentLevel = GetIEditor()->GetDocument();
        if (currentLevel && currentLevel->IsDocumentReady())
        {
            // This closes the current document (level)
            currentLevel->OnNewDocument();

            // Then we freeze the viewport's input
            AzToolsFramework::ViewportInteraction::ViewportFreezeRequestBus::Broadcast(
                &AzToolsFramework::ViewportInteraction::ViewportFreezeRequestBus::Events::FreezeViewportInput, true);

            // Then we need to tell the game engine there is no level to render anymore
            if (GetIEditor()->GetGameEngine())
            {
                GetIEditor()->GetGameEngine()->SetLevelPath("");
                GetIEditor()->GetGameEngine()->SetLevelLoaded(false);

                CViewManager* pViewManager = GetIEditor()->GetViewManager();
                CViewport* pGameViewport = pViewManager ? pViewManager->GetGameViewport() : nullptr;
                if (pGameViewport)
                {
                    pGameViewport->SetViewTM(Matrix34::CreateIdentity());
                }
            }
        }
    }

    const char* PyGetGameFolder()
    {
        return Path::GetEditingGameDataFolder().c_str();
    }

    AZStd::string PyGetGameFolderAsString()
    {
        return Path::GetEditingGameDataFolder();
    }

    bool PyOpenLevel(const char* pLevelName)
    {
        QString levelPath = pLevelName;

        if (!QFile::exists(levelPath))
        {
            // if the input path can't be found, let's automatically add on the game folder and the levels
            QString levelsDir = QString("%1/%2").arg(Path::GetEditingGameDataFolder().c_str()).arg("Levels");

            // now let's check if they pre-pended directories (Samples/SomeLevelName)
            QString levelFileName = levelPath;
            QStringList splitLevelPath = levelPath.contains('/') ? levelPath.split('/') : levelPath.split('\\');
            if (splitLevelPath.length() > 1)
            {
                // take the last one as the level directory name and the level file name in that directory
                levelFileName = splitLevelPath.last();
            }

            levelPath = levelsDir / levelPath / levelFileName;

            // make sure the level path includes the cry extension, if needed
            if (!levelFileName.endsWith(oldFileExtension) && !levelFileName.endsWith(defaultFileExtension))
            {
                QString newLevelPath = levelPath + defaultFileExtension;
                QString oldLevelPath = levelPath + oldFileExtension;

                // Check if there is a .cry file, otherwise assume it is a new .ly file
                if (QFileInfo(oldLevelPath).exists())
                {
                    levelPath = oldLevelPath;
                }
                else
                {
                    levelPath = newLevelPath;
                }
            }

            if (!QFile::exists(levelPath))
            {
                return false;
            }
        }

        auto previousDocument = GetIEditor()->GetDocument();
        QString previousPathName = (previousDocument != nullptr) ? previousDocument->GetLevelPathName() : "";
        auto newDocument = CCryEditApp::instance()->OpenDocumentFile(levelPath.toUtf8().data());

        // the underlying document pointer doesn't change, so we can't check that; use the path name's instead

        bool result = true;
        if (newDocument == nullptr || newDocument->IsLevelLoadFailed() || (newDocument->GetLevelPathName() == previousPathName))
        {
            result = false;
        }

        return result;
    }

    bool PyOpenLevelNoPrompt(const char* pLevelName)
    {
        GetIEditor()->GetDocument()->SetModifiedFlag(false);
        return PyOpenLevel(pLevelName);
    }

    bool PyReloadCurrentLevel()
    {
        if (GetIEditor()->IsLevelLoaded())
        {
            // Close the current level so that the subsequent call to open the same level will be allowed
            QString currentLevelPath = GetIEditor()->GetDocument()->GetLevelPathName();
            CloseCurrentLevel();

            return PyOpenLevel(currentLevelPath.toUtf8().constData());
        }

        return false;
    }

    int PyCreateLevel(const char* levelName, int resolution, int unitSize, bool bUseTerrain)
    {
        QString qualifiedName;
        return CCryEditApp::instance()->CreateLevel(levelName, resolution, unitSize, bUseTerrain, qualifiedName,
                                                    CCryEditApp::TerrainTextureExportSettings(CCryEditApp::TerrainTextureExportTechnique::PromptUser));
    }

    int PyCreateLevelNoPrompt(const char* levelName, int heightmapResolution, int heightmapUnitSize, int terrainExportTextureSize, bool useTerrain)
    {
        // If a level was open, ignore any unsaved changes if it had been modified
        if (GetIEditor()->IsLevelLoaded())
        {
            GetIEditor()->GetDocument()->SetModifiedFlag(false);
        }

        QString qualifiedName;
        CCryEditApp::TerrainTextureExportSettings exportSettings(CCryEditApp::TerrainTextureExportTechnique::UseDefault, terrainExportTextureSize);
        return CCryEditApp::instance()->CreateLevel(levelName, heightmapResolution, heightmapUnitSize, useTerrain, qualifiedName, exportSettings);
    }

    const char* PyGetCurrentLevelName()
    {
        return GetIEditor()->GetGameEngine()->GetLevelName().toUtf8().data();
    }

    const char* PyGetCurrentLevelPath()
    {
        return GetIEditor()->GetGameEngine()->GetLevelPath().toUtf8().data();
    }

    void Command_LoadPlugins()
    {
        GetIEditor()->LoadPlugins();
    }

    AZ::Vector3 PyGetCurrentViewPosition()
    {
        Vec3 pos = GetIEditor()->GetSystem()->GetViewCamera().GetPosition();
        return AZ::Vector3(pos.x, pos.y, pos.z);
    }

    AZ::Vector3 PyGetCurrentViewRotation()
    {
        Ang3 ang = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(GetIEditor()->GetSystem()->GetViewCamera().GetMatrix())));
        return AZ::Vector3(ang.x, ang.y, ang.z);
    }

    void PySetCurrentViewPosition(float x, float y, float z)
    {
        AzToolsFramework::IEditorCameraController* editorCameraController = AZ::Interface<AzToolsFramework::IEditorCameraController>::Get();
        AZ_Error("editor", editorCameraController, "IEditorCameraController is not registered.");
        if (editorCameraController)
        {
            editorCameraController->SetCurrentViewPosition(AZ::Vector3{ x, y, z });
        }
    }

    void PySetCurrentViewRotation(float x, float y, float z)
    {
        AzToolsFramework::IEditorCameraController* editorCameraController = AZ::Interface<AzToolsFramework::IEditorCameraController>::Get();
        AZ_Error("editor", editorCameraController, "IEditorCameraController is not registered.");
        if (editorCameraController)
        {
            editorCameraController->SetCurrentViewRotation(AZ::Vector3{ x, y, z });
        }
    }
}


#define ERROR_LEN 256


CCryDocManager::CCryDocManager()
{
}

CCrySingleDocTemplate* CCryDocManager::SetDefaultTemplate(CCrySingleDocTemplate* pNew)
{
    CCrySingleDocTemplate* pOld = m_pDefTemplate;
    m_pDefTemplate = pNew;
    m_templateList.clear();
    m_templateList.push_back(m_pDefTemplate);
    return pOld;
}
// Copied from MFC to get rid of the silly ugly unoverridable doc-type pick dialog
void CCryDocManager::OnFileNew()
{
    assert(m_pDefTemplate != NULL);

    m_pDefTemplate->OpenDocumentFile(NULL);
    // if returns NULL, the user has already been alerted
}
BOOL CCryDocManager::DoPromptFileName(QString& fileName, UINT nIDSTitle,
    DWORD lFlags, BOOL bOpenFileDialog, CDocTemplate* pTemplate)
{
    CLevelFileDialog levelFileDialog(bOpenFileDialog);

    levelFileDialog.show();
    levelFileDialog.adjustSize();

    if (levelFileDialog.exec() == QDialog::Accepted)
    {
        fileName = levelFileDialog.GetFileName();
        return true;
    }

    return false;
}
CCryEditDoc* CCryDocManager::OpenDocumentFile(LPCTSTR lpszFileName, BOOL bAddToMRU)
{
    assert(lpszFileName != NULL);

    // find the highest confidence
    auto pos = m_templateList.begin();
    CCrySingleDocTemplate::Confidence bestMatch = CCrySingleDocTemplate::noAttempt;
    CCrySingleDocTemplate* pBestTemplate = NULL;
    CCryEditDoc* pOpenDocument = NULL;

    if (lpszFileName[0] == '\"')
    {
        ++lpszFileName;
    }
    QString szPath = QString::fromUtf8(lpszFileName);
    if (szPath.endsWith('"'))
    {
        szPath.remove(szPath.length() - 1, 1);
    }

    while (pos != m_templateList.end())
    {
        auto pTemplate = *(pos++);

        CCrySingleDocTemplate::Confidence match;
        assert(pOpenDocument == NULL);
        match = pTemplate->MatchDocType(szPath.toUtf8().data(), pOpenDocument);
        if (match > bestMatch)
        {
            bestMatch = match;
            pBestTemplate = pTemplate;
        }
        if (match == CCrySingleDocTemplate::yesAlreadyOpen)
        {
            break;      // stop here
        }
    }

    if (pOpenDocument != NULL)
    {
        return pOpenDocument;
    }

    if (pBestTemplate == NULL)
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Failed to open document."));
        return NULL;
    }

    return pBestTemplate->OpenDocumentFile(szPath.toUtf8().data(), bAddToMRU, FALSE);
}

//////////////////////////////////////////////////////////////////////////////
// CCryEditApp

#undef ON_COMMAND
#define ON_COMMAND(id, method) \
    MainWindow::instance()->GetActionManager()->RegisterActionHandler(id, this, &CCryEditApp::method);

#undef ON_COMMAND_RANGE
#define ON_COMMAND_RANGE(idStart, idEnd, method) \
    for (int i = idStart; i <= idEnd; ++i) \
        ON_COMMAND(i, method);

void CCryEditApp::RegisterActionHandlers()
{
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_DOCUMENTATION_GETTINGSTARTEDGUIDE, OnDocumentationGettingStartedGuide)
    ON_COMMAND(ID_DOCUMENTATION_TUTORIALS, OnDocumentationTutorials)
    ON_COMMAND(ID_DOCUMENTATION_GLOSSARY, OnDocumentationGlossary)
    ON_COMMAND(ID_DOCUMENTATION_LUMBERYARD, OnDocumentationLumberyard)
    ON_COMMAND(ID_DOCUMENTATION_GAMELIFT, OnDocumentationGamelift)
    ON_COMMAND(ID_DOCUMENTATION_RELEASENOTES, OnDocumentationReleaseNotes)
    ON_COMMAND(ID_DOCUMENTATION_GAMEDEVBLOG, OnDocumentationGameDevBlog)
    ON_COMMAND(ID_DOCUMENTATION_TWITCHCHANNEL, OnDocumentationTwitchChannel)
    ON_COMMAND(ID_DOCUMENTATION_FORUMS, OnDocumentationForums)
    ON_COMMAND(ID_DOCUMENTATION_AWSSUPPORT, OnDocumentationAWSSupport)
    ON_COMMAND(ID_DOCUMENTATION_FEEDBACK, OnDocumentationFeedback)
    ON_COMMAND(ID_AWS_CREDENTIAL_MGR, OnAWSCredentialEditor)
    ON_COMMAND(ID_AWS_RESOURCE_MANAGEMENT, OnAWSResourceManagement)
    ON_COMMAND(ID_AWS_ACTIVE_DEPLOYMENT, OnAWSActiveDeployment)
    ON_COMMAND(ID_AWS_GAMELIFT_LEARN, OnAWSGameliftLearn)
    ON_COMMAND(ID_AWS_GAMELIFT_CONSOLE, OnAWSGameliftConsole)
    ON_COMMAND(ID_AWS_GAMELIFT_GETSTARTED, OnAWSGameliftGetStarted)
    ON_COMMAND(ID_AWS_GAMELIFT_TRIALWIZARD, OnAWSGameliftTrialWizard)
    ON_COMMAND(ID_AWS_COGNITO_CONSOLE, OnAWSCognitoConsole)
    ON_COMMAND(ID_AWS_DEVICEFARM_CONSOLE, OnAWSDeviceFarmConsole)        
    ON_COMMAND(ID_AWS_DYNAMODB_CONSOLE, OnAWSDynamoDBConsole)
    ON_COMMAND(ID_AWS_S3_CONSOLE, OnAWSS3Console)
    ON_COMMAND(ID_AWS_LAMBDA_CONSOLE, OnAWSLambdaConsole)
    ON_COMMAND(ID_AWS_LAUNCH, OnAWSLaunch)
    ON_COMMAND(ID_AWS_HOW_TO_SET_UP_CLOUD_CANVAS, OnHowToSetUpCloudCanvas)
    ON_COMMAND(ID_AWS_LEARN_ABOUT_GAME_LIFT, OnLearnAboutGameLift)
    ON_COMMAND(ID_COMMERCE_PUBLISH, OnCommercePublish)
    ON_COMMAND(ID_COMMERCE_MERCH, OnCommerceMerch)
    ON_COMMAND(ID_TERRAIN, ToolTerrain)
    // Confetti Begin: Jurecka
    ON_COMMAND(ID_PARTICLE_EDITOR, OnOpenParticleEditor)
    // Confetti End: Jurecka
    ON_COMMAND(ID_GENERATORS_LIGHTING, ToolLighting)
    ON_COMMAND(ID_MEASUREMENT_SYSTEM_TOOL, MeasurementSystemTool)
    ON_COMMAND(ID_TERRAIN_TEXTURE_EXPORT, TerrainTextureExport)
    ON_COMMAND(ID_TERRAIN_REFINETERRAINTEXTURETILES, RefineTerrainTextureTiles)
    ON_COMMAND(ID_GENERATORS_TEXTURE, ToolTexture)
    ON_COMMAND(ID_FILE_GENERATETERRAINTEXTURE, GenerateTerrainTextureWithPrompts)
    ON_COMMAND(ID_FILE_GENERATETERRAIN, GenerateTerrain)
    ON_COMMAND(ID_FILE_EXPORT_SELECTEDOBJECTS, OnExportSelectedObjects)
    ON_COMMAND(ID_FILE_EXPORT_TERRAINAREA, OnExportTerrainArea)
    ON_COMMAND(ID_FILE_EXPORT_TERRAINAREAWITHOBJECTS, OnExportTerrainAreaWithObjects)
    ON_COMMAND(ID_EDIT_HOLD, OnEditHold)
    ON_COMMAND(ID_EDIT_FETCH, OnEditFetch)
    ON_COMMAND(ID_GENERATORS_STATICOBJECTS, OnGeneratorsStaticobjects)
    ON_COMMAND(ID_FILE_EXPORTTOGAMENOSURFACETEXTURE, OnFileExportToGameNoSurfaceTexture)
    ON_COMMAND(ID_VIEW_SWITCHTOGAME, OnViewSwitchToGame)
    ON_COMMAND(ID_VIEW_DEPLOY, OnViewDeploy)
    ON_COMMAND(ID_EDIT_SELECTALL, OnEditSelectAll)
    ON_COMMAND(ID_EDIT_SELECTNONE, OnEditSelectNone)
    ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
    ON_COMMAND(ID_MOVE_OBJECT, OnMoveObject)
    ON_COMMAND(ID_SELECT_OBJECT, OnSelectObject)
    ON_COMMAND(ID_RENAME_OBJ, OnRenameObj)
    ON_COMMAND(ID_SET_HEIGHT, OnSetHeight)
    ON_COMMAND(ID_SCRIPT_COMPILESCRIPT, OnScriptCompileScript)
    ON_COMMAND(ID_SCRIPT_EDITSCRIPT, OnScriptEditScript)
    ON_COMMAND(ID_EDITMODE_MOVE, OnEditmodeMove)
    ON_COMMAND(ID_EDITMODE_ROTATE, OnEditmodeRotate)
    ON_COMMAND(ID_EDITMODE_SCALE, OnEditmodeScale)
    ON_COMMAND(ID_EDITTOOL_LINK, OnEditToolLink)
    ON_COMMAND(ID_EDITTOOL_UNLINK, OnEditToolUnlink)
    ON_COMMAND(ID_EDITMODE_SELECT, OnEditmodeSelect)
    ON_COMMAND(ID_EDIT_ESCAPE, OnEditEscape)
    ON_COMMAND(ID_OBJECTMODIFY_SETAREA, OnObjectSetArea)
    ON_COMMAND(ID_OBJECTMODIFY_SETHEIGHT, OnObjectSetHeight)
    ON_COMMAND(ID_OBJECTMODIFY_VERTEXSNAPPING, OnObjectVertexSnapping)
    ON_COMMAND(ID_MODIFY_ALIGNOBJTOSURF, OnAlignToVoxel)
    ON_COMMAND(ID_OBJECTMODIFY_FREEZE, OnObjectmodifyFreeze)
    ON_COMMAND(ID_OBJECTMODIFY_UNFREEZE, OnObjectmodifyUnfreeze)
    ON_COMMAND(ID_EDITMODE_SELECTAREA, OnEditmodeSelectarea)
    ON_COMMAND(ID_SELECT_AXIS_X, OnSelectAxisX)
    ON_COMMAND(ID_SELECT_AXIS_Y, OnSelectAxisY)
    ON_COMMAND(ID_SELECT_AXIS_Z, OnSelectAxisZ)
    ON_COMMAND(ID_SELECT_AXIS_XY, OnSelectAxisXy)
    ON_COMMAND(ID_UNDO, OnUndo)
    ON_COMMAND(ID_TOOLBAR_WIDGET_REDO, OnUndo)     // Can't use the same ID, because for the menu we can't have a QWidgetAction, while for the toolbar we want one
    ON_COMMAND(ID_EDIT_CLONE, OnEditClone)
    ON_COMMAND(ID_SELECTION_SAVE, OnSelectionSave)
    ON_COMMAND(ID_IMPORT_ASSET, OnOpenAssetImporter)
    ON_COMMAND(ID_SELECTION_LOAD, OnSelectionLoad)
    ON_COMMAND(ID_OBJECTMODIFY_ALIGN, OnAlignObject)
    ON_COMMAND(ID_MODIFY_ALIGNOBJTOSURF, OnAlignToVoxel)
    ON_COMMAND(ID_OBJECTMODIFY_ALIGNTOGRID, OnAlignToGrid)
    ON_COMMAND(ID_GROUP_ATTACH, OnGroupAttach)
    ON_COMMAND(ID_GROUP_CLOSE, OnGroupClose)
    ON_COMMAND(ID_GROUP_DETACH, OnGroupDetach)
    ON_COMMAND(ID_GROUP_MAKE, OnGroupMake)
    ON_COMMAND(ID_GROUP_OPEN, OnGroupOpen)
    ON_COMMAND(ID_GROUP_UNGROUP, OnGroupUngroup)
    ON_COMMAND(ID_CLOUDS_CREATE, OnCloudsCreate)
    ON_COMMAND(ID_CLOUDS_DESTROY, OnCloudsDestroy)
    ON_COMMAND(ID_CLOUDS_OPEN, OnCloudsOpen)
    ON_COMMAND(ID_CLOUDS_CLOSE, OnCloudsClose)
    ON_COMMAND(ID_MISSION_RENAME, OnMissionRename)
    ON_COMMAND(ID_MISSION_SELECT, OnMissionSelect)
    ON_COMMAND(ID_LOCK_SELECTION, OnLockSelection)
    ON_COMMAND(ID_EDIT_LEVELDATA, OnEditLevelData)
    ON_COMMAND(ID_FILE_EDITLOGFILE, OnFileEditLogFile)
    ON_COMMAND(ID_FILE_RESAVESLICES, OnFileResaveSlices)
    ON_COMMAND(ID_FILE_EDITEDITORINI, OnFileEditEditorini)
    ON_COMMAND(ID_SELECT_AXIS_TERRAIN, OnSelectAxisTerrain)
    ON_COMMAND(ID_SELECT_AXIS_SNAPTOALL, OnSelectAxisSnapToAll)
    ON_COMMAND(ID_PREFERENCES, OnPreferences)
    ON_COMMAND(ID_RELOAD_ALL_SCRIPTS, OnReloadAllScripts)
    ON_COMMAND(ID_RELOAD_ENTITY_SCRIPTS, OnReloadEntityScripts)
    ON_COMMAND(ID_RELOAD_ACTOR_SCRIPTS, OnReloadActorScripts)
    ON_COMMAND(ID_RELOAD_ITEM_SCRIPTS, OnReloadItemScripts)
    ON_COMMAND(ID_RELOAD_AI_SCRIPTS, OnReloadAIScripts)
    ON_COMMAND(ID_RELOAD_GEOMETRY, OnReloadGeometry)
    ON_COMMAND(ID_RELOAD_TERRAIN, OnReloadTerrain)
    ON_COMMAND(ID_TOGGLE_MULTIPLAYER, OnToggleMultiplayer)
    ON_COMMAND(ID_REDO, OnRedo)
    ON_COMMAND(ID_TOOLBAR_WIDGET_REDO, OnRedo)
    ON_COMMAND(ID_RELOAD_TEXTURES, OnReloadTextures)
    ON_COMMAND(ID_FILE_OPEN_LEVEL, OnOpenLevel)
    ON_COMMAND(ID_FILE_RELOAD_LEVEL, OnReloadLevel)
#ifdef ENABLE_SLICE_EDITOR
    ON_COMMAND(ID_FILE_NEW_SLICE, OnCreateSlice)
    ON_COMMAND(ID_FILE_OPEN_SLICE, OnOpenSlice)
#endif
#ifdef LY_TERRAIN_EDITOR
    ON_COMMAND(ID_TERRAIN_COLLISION, OnTerrainCollision)
#endif //#ifdef LY_TERRAIN_EDITOR
    ON_COMMAND(ID_RESOURCES_GENERATECGFTHUMBNAILS, OnGenerateCgfThumbnails)
    ON_COMMAND(ID_AI_GENERATEALL, OnAIGenerateAll)
    ON_COMMAND(ID_AI_GENERATETRIANGULATION, OnAIGenerateTriangulation)
    ON_COMMAND(ID_AI_GENERATEWAYPOINT, OnAIGenerateWaypoint)
    ON_COMMAND(ID_AI_GENERATEFLIGHTNAVIGATION, OnAIGenerateFlightNavigation)
    ON_COMMAND(ID_AI_GENERATE3DVOLUMES, OnAIGenerate3dvolumes)
    ON_COMMAND(ID_AI_VALIDATENAVIGATION, OnAIValidateNavigation)
    ON_COMMAND(ID_AI_GENERATE3DDEBUGVOXELS, OnAIGenerate3DDebugVoxels)
    ON_COMMAND(ID_AI_CLEARALLNAVIGATION, OnAIClearAllNavigation)
    ON_COMMAND(ID_AI_GENERATESPAWNERS, OnAIGenerateSpawners)
    ON_COMMAND(ID_AI_GENERATECOVERSURFACES, OnAIGenerateCoverSurfaces)
    ON_COMMAND(ID_AI_NAVIGATION_SHOW_AREAS, OnAINavigationShowAreas)
    ON_COMMAND(ID_AI_NAVIGATION_ENABLE_CONTINUOUS_UPDATE, OnAINavigationEnableContinuousUpdate)
    ON_COMMAND(ID_AI_NAVIGATION_NEW_AREA, OnAINavigationNewArea)
    ON_COMMAND(ID_AI_NAVIGATION_TRIGGER_FULL_REBUILD, OnAINavigationFullRebuild)
    ON_COMMAND(ID_AI_NAVIGATION_ADD_SEED, OnAINavigationAddSeed)
    ON_COMMAND(ID_AI_NAVIGATION_VISUALIZE_ACCESSIBILITY, OnVisualizeNavigationAccessibility)
    ON_COMMAND(ID_LAYER_SELECT, OnLayerSelect)
    ON_COMMAND(ID_SWITCH_PHYSICS, OnSwitchPhysics)
    ON_COMMAND(ID_GAME_SYNCPLAYER, OnSyncPlayer)
    ON_COMMAND(ID_RESOURCES_REDUCEWORKINGSET, OnResourcesReduceworkingset)
    ON_COMMAND(ID_TOOLS_EQUIPPACKSEDIT, OnToolsEquipPacksEdit)
    ON_COMMAND(ID_TOOLS_UPDATEPROCEDURALVEGETATION, OnToolsUpdateProcVegetation)

    // Standard file based document commands
    ON_COMMAND(ID_EDIT_HIDE, OnEditHide)
    ON_COMMAND(ID_EDIT_SHOW_LAST_HIDDEN, OnEditShowLastHidden)
    ON_COMMAND(ID_EDIT_UNHIDEALL, OnEditUnhideall)
    ON_COMMAND(ID_EDIT_FREEZE, OnEditFreeze)
    ON_COMMAND(ID_EDIT_UNFREEZEALL, OnEditUnfreezeall)

    ON_COMMAND(ID_SNAP_TO_GRID, OnSnap)

    ON_COMMAND(ID_WIREFRAME, OnWireframe)

    ON_COMMAND(ID_VIEW_GRIDSETTINGS, OnViewGridsettings)
    ON_COMMAND(ID_VIEW_CONFIGURELAYOUT, OnViewConfigureLayout)

    ON_COMMAND(IDC_SELECTION, OnDummyCommand)
    //////////////////////////////////////////////////////////////////////////
    ON_COMMAND(ID_TAG_LOC1, OnTagLocation1)
    ON_COMMAND(ID_TAG_LOC2, OnTagLocation2)
    ON_COMMAND(ID_TAG_LOC3, OnTagLocation3)
    ON_COMMAND(ID_TAG_LOC4, OnTagLocation4)
    ON_COMMAND(ID_TAG_LOC5, OnTagLocation5)
    ON_COMMAND(ID_TAG_LOC6, OnTagLocation6)
    ON_COMMAND(ID_TAG_LOC7, OnTagLocation7)
    ON_COMMAND(ID_TAG_LOC8, OnTagLocation8)
    ON_COMMAND(ID_TAG_LOC9, OnTagLocation9)
    ON_COMMAND(ID_TAG_LOC10, OnTagLocation10)
    ON_COMMAND(ID_TAG_LOC11, OnTagLocation11)
    ON_COMMAND(ID_TAG_LOC12, OnTagLocation12)
    //////////////////////////////////////////////////////////////////////////
    ON_COMMAND(ID_GOTO_LOC1, OnGotoLocation1)
    ON_COMMAND(ID_GOTO_LOC2, OnGotoLocation2)
    ON_COMMAND(ID_GOTO_LOC3, OnGotoLocation3)
    ON_COMMAND(ID_GOTO_LOC4, OnGotoLocation4)
    ON_COMMAND(ID_GOTO_LOC5, OnGotoLocation5)
    ON_COMMAND(ID_GOTO_LOC6, OnGotoLocation6)
    ON_COMMAND(ID_GOTO_LOC7, OnGotoLocation7)
    ON_COMMAND(ID_GOTO_LOC8, OnGotoLocation8)
    ON_COMMAND(ID_GOTO_LOC9, OnGotoLocation9)
    ON_COMMAND(ID_GOTO_LOC10, OnGotoLocation10)
    ON_COMMAND(ID_GOTO_LOC11, OnGotoLocation11)
    ON_COMMAND(ID_GOTO_LOC12, OnGotoLocation12)
    //////////////////////////////////////////////////////////////////////////

    ON_COMMAND(ID_TOOLS_LOGMEMORYUSAGE, OnToolsLogMemoryUsage)
    ON_COMMAND(ID_TERRAIN_EXPORTBLOCK, OnTerrainExportblock)
    ON_COMMAND(ID_TERRAIN_IMPORTBLOCK, OnTerrainImportblock)
    ON_COMMAND(ID_TOOLS_CUSTOMIZEKEYBOARD, OnCustomizeKeyboard)
    ON_COMMAND(ID_TOOLS_CONFIGURETOOLS, OnToolsConfiguretools)
    ON_COMMAND(ID_TOOLS_SCRIPTHELP, OnToolsScriptHelp)
    ON_COMMAND(ID_EXPORT_INDOORS, OnExportIndoors)
#ifdef FEATURE_ORTHOGRAPHIC_VIEW
    ON_COMMAND(ID_VIEW_CYCLE2DVIEWPORT, OnViewCycle2dviewport)
#endif
    ON_COMMAND(ID_DISPLAY_GOTOPOSITION, OnDisplayGotoPosition)
    ON_COMMAND(ID_DISPLAY_SETVECTOR, OnDisplaySetVector)
    ON_COMMAND(ID_SNAPANGLE, OnSnapangle)
    ON_COMMAND(ID_RULER, OnRuler)
    ON_COMMAND(ID_ROTATESELECTION_XAXIS, OnRotateselectionXaxis)
    ON_COMMAND(ID_ROTATESELECTION_YAXIS, OnRotateselectionYaxis)
    ON_COMMAND(ID_ROTATESELECTION_ZAXIS, OnRotateselectionZaxis)
    ON_COMMAND(ID_ROTATESELECTION_ROTATEANGLE, OnRotateselectionRotateangle)
    ON_COMMAND(ID_CONVERTSELECTION_TOBRUSHES, OnConvertselectionTobrushes)
    ON_COMMAND(ID_CONVERTSELECTION_TOSIMPLEENTITY, OnConvertselectionTosimpleentity)
    ON_COMMAND(ID_CONVERTSELECTION_TODESIGNEROBJECT, OnConvertSelectionToDesignerObject)
    ON_COMMAND(ID_CONVERTSELECTION_TOGAMEVOLUME, OnConvertselectionToGameVolume)
    ON_COMMAND(ID_CONVERTSELECTION_TOCOMPONENTENTITY, OnConvertSelectionToComponentEntity)
    ON_COMMAND(ID_MODIFY_OBJECT_HEIGHT, OnObjectSetHeight)
    ON_COMMAND(ID_EDIT_RENAMEOBJECT, OnEditRenameobject)
    ON_COMMAND(ID_CHANGEMOVESPEED_INCREASE, OnChangemovespeedIncrease)
    ON_COMMAND(ID_CHANGEMOVESPEED_DECREASE, OnChangemovespeedDecrease)
    ON_COMMAND(ID_CHANGEMOVESPEED_CHANGESTEP, OnChangemovespeedChangestep)
    ON_COMMAND(ID_MODIFY_AIPOINT_PICKLINK, OnModifyAipointPicklink)
    ON_COMMAND(ID_MODIFY_AIPOINT_PICKIMPASSLINK, OnModifyAipointPickImpasslink)
    ON_COMMAND(ID_MATERIAL_ASSIGNCURRENT, OnMaterialAssigncurrent)
    ON_COMMAND(ID_MATERIAL_RESETTODEFAULT, OnMaterialResettodefault)
    ON_COMMAND(ID_MATERIAL_GETMATERIAL, OnMaterialGetmaterial)
    ON_COMMAND(ID_FILE_SAVELEVELRESOURCES, OnFileSavelevelresources)
    ON_COMMAND(ID_CLEAR_REGISTRY, OnClearRegistryData)
    ON_COMMAND(ID_VALIDATELEVEL, OnValidatelevel)
    ON_COMMAND(ID_TOOLS_VALIDATEOBJECTPOSITIONS, OnValidateObjectPositions)
    ON_COMMAND(ID_TERRAIN_RESIZE, OnTerrainResizeterrain)
    ON_COMMAND(ID_TOOLS_PREFERENCES, OnToolsPreferences)
    ON_COMMAND(ID_GRAPHICS_SETTINGS, OnGraphicsSettings)
    ON_COMMAND(ID_EDIT_INVERTSELECTION, OnEditInvertselection)
    ON_COMMAND(ID_PREFABS_MAKEFROMSELECTION, OnPrefabsMakeFromSelection)
    ON_COMMAND(ID_PREFABS_ADDSELECTIONTOPREFAB, OnAddSelectionToPrefab)
    ON_COMMAND(ID_PREFABS_CLONESELECTIONFROMPREFAB, OnCloneSelectionFromPrefab)
    ON_COMMAND(ID_PREFABS_EXTRACTSELECTIONFROMPREFAB, OnExtractSelectionFromPrefab)
    ON_COMMAND(ID_PREFABS_OPENALL, OnPrefabsOpenAll)
    ON_COMMAND(ID_PREFABS_CLOSEALL, OnPrefabsCloseAll)
    ON_COMMAND(ID_PREFABS_REFRESHALL, OnPrefabsRefreshAll)
    ON_COMMAND(ID_TOOLTERRAINMODIFY_SMOOTH, OnToolterrainmodifySmooth)
    ON_COMMAND(ID_TERRAINMODIFY_SMOOTH, OnTerrainmodifySmooth)
    ON_COMMAND(ID_TERRAIN_VEGETATION, OnTerrainVegetation)
    ON_COMMAND(ID_TERRAIN_PAINTLAYERS, OnTerrainPaintlayers)
    ON_COMMAND(ID_SWITCHCAMERA_DEFAULTCAMERA, OnSwitchToDefaultCamera)
    ON_COMMAND(ID_SWITCHCAMERA_SEQUENCECAMERA, OnSwitchToSequenceCamera)
    ON_COMMAND(ID_SWITCHCAMERA_SELECTEDCAMERA, OnSwitchToSelectedcamera)
    ON_COMMAND(ID_SWITCHCAMERA_NEXT, OnSwitchcameraNext)
    ON_COMMAND(ID_OPEN_SUBSTANCE_EDITOR, OnOpenProceduralMaterialEditor)
    ON_COMMAND(ID_OPEN_MANNEQUIN_EDITOR, OnOpenMannequinEditor)
    ON_COMMAND(ID_OPEN_CHARACTER_TOOL, OnOpenCharacterTool)
    ON_COMMAND(ID_OPEN_DATABASE, OnOpenDataBaseView)
    ON_COMMAND(ID_OPEN_ASSET_BROWSER, OnOpenAssetBrowserView)
    ON_COMMAND(ID_OPEN_AUDIO_CONTROLS_BROWSER, OnOpenAudioControlsEditor)

    ON_COMMAND(ID_OPEN_MATERIAL_EDITOR, OnOpenMaterialEditor)
    ON_COMMAND(ID_GOTO_VIEWPORTSEARCH, OnGotoViewportSearch)
    ON_COMMAND(ID_SUBOBJECTMODE_VERTEX, OnSubobjectmodeVertex)
    ON_COMMAND(ID_SUBOBJECTMODE_EDGE, OnSubobjectmodeEdge)
    ON_COMMAND(ID_SUBOBJECTMODE_FACE, OnSubobjectmodeFace)
    ON_COMMAND(ID_MATERIAL_PICKTOOL, OnMaterialPicktool)
    ON_COMMAND(ID_DISPLAY_SHOWHELPERS, OnShowHelpers)
    ON_COMMAND(ID_OPEN_AIDEBUGGER, OnOpenAIDebugger)
    ON_COMMAND(ID_OPEN_TERRAINTEXTURE_EDITOR, ToolTexture)
    ON_COMMAND(ID_OPEN_LAYER_EDITOR, OnOpenLayerEditor)
    ON_COMMAND(ID_OPEN_TERRAIN_EDITOR, OnOpenTerrainEditor)
    ON_COMMAND(ID_OPEN_TRACKVIEW, OnOpenTrackView)
    ON_COMMAND(ID_OPEN_UICANVASEDITOR, OnOpenUICanvasEditor)
    ON_COMMAND(ID_GOTO_VIEWPORTSEARCH, OnGotoViewportSearch)
    ON_COMMAND(ID_MATERIAL_PICKTOOL, OnMaterialPicktool)
    ON_COMMAND(ID_TERRAIN_TIMEOFDAY, OnTimeOfDay)
    ON_COMMAND(ID_TERRAIN_TIMEOFDAYBUTTON, OnTimeOfDay)

    ON_COMMAND(ID_TOOLS_RESOLVEMISSINGOBJECTS, OnResolveMissingObjects)

    ON_COMMAND_RANGE(ID_GAME_PC_ENABLELOWSPEC, ID_GAME_PC_ENABLEVERYHIGHSPEC, OnChangeGameSpec)

    ON_COMMAND_RANGE(ID_GAME_OSXMETAL_ENABLELOWSPEC, ID_GAME_OSXMETAL_ENABLEVERYHIGHSPEC, OnChangeGameSpec)

    ON_COMMAND_RANGE(ID_GAME_APPLETV_ENABLESPEC, ID_GAME_APPLETV_ENABLESPEC, OnChangeGameSpec)

    ON_COMMAND_RANGE(ID_GAME_ANDROID_ENABLELOWSPEC, ID_GAME_ANDROID_ENABLEVERYHIGHSPEC, OnChangeGameSpec)

    ON_COMMAND_RANGE(ID_GAME_IOS_ENABLELOWSPEC, ID_GAME_IOS_ENABLEVERYHIGHSPEC, OnChangeGameSpec)

#if defined(AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS)
#define AZ_RESTRICTED_PLATFORM_EXPANSION(CodeName, CODENAME, codename, PrivateName, PRIVATENAME, privatename, PublicName, PUBLICNAME, publicname, PublicAuxName1, PublicAuxName2, PublicAuxName3)\
    ON_COMMAND_RANGE(ID_GAME_##CODENAME##_ENABLELOWSPEC, ID_GAME_##CODENAME##_ENABLEHIGHSPEC, OnChangeGameSpec)
    AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS
#undef AZ_RESTRICTED_PLATFORM_EXPANSION
#endif

    ON_COMMAND(ID_OPEN_QUICK_ACCESS_BAR, OnOpenQuickAccessBar)

    ON_COMMAND(ID_FILE_SAVE_LEVEL, OnFileSave)
    ON_COMMAND(ID_PANEL_LAYERS_SAVE_EXTERNAL_LAYERS, OnFileSaveExternalLayers)
    ON_COMMAND(ID_CONVERT_LEGACY_ENTITIES, OnFileConvertLegacyEntities)
    ON_COMMAND(ID_FILE_EXPORTOCCLUSIONMESH, OnFileExportOcclusionMesh)

    // Project Configurator
    ON_COMMAND(ID_PROJECT_CONFIGURATOR_PROJECTSELECTION, OnOpenProjectConfiguratorSwitchProject)
    ON_COMMAND(ID_PROJECT_CONFIGURATOR_GEMS, OnOpenProjectConfiguratorGems)
    // ~Project Configurator
}

CCryEditApp* CCryEditApp::s_currentInstance = nullptr;
/////////////////////////////////////////////////////////////////////////////
// CCryEditApp construction
CCryEditApp::CCryEditApp()
{
    s_currentInstance = this;

    m_sPreviewFile[0] = 0;

    // Place all significant initialization in InitInstance
    ZeroStruct(m_tagLocations);
    ZeroStruct(m_tagAngles);

    AzFramework::AssetSystemInfoBus::Handler::BusConnect();

    m_disableIdleProcessingCounter = 0;
    EditorIdleProcessingBus::Handler::BusConnect();
}

//////////////////////////////////////////////////////////////////////////
CCryEditApp::~CCryEditApp()
{
    EditorIdleProcessingBus::Handler::BusDisconnect();
    AzFramework::AssetSystemInfoBus::Handler::BusDisconnect();
    s_currentInstance = nullptr;
}

CCryEditApp* CCryEditApp::instance()
{
    return s_currentInstance;
}

class CEditCommandLineInfo
{
public:
    bool m_bTest = false;
    bool m_bAutoLoadLevel = false;
    bool m_bExport = false;
    bool m_bExportTexture = false;
    bool m_bExportAI = false;

    bool m_bMatEditMode = false;
    bool m_bPrecacheShaders = false;
    bool m_bPrecacheShadersLevels = false;
    bool m_bPrecacheShaderList = false;
    bool m_bStatsShaders = false;
    bool m_bStatsShaderList = false;
    bool m_bMergeShaders = false;

    bool m_bConsoleMode = false;
    bool m_bNullRenderer = false;
    bool m_bDeveloperMode = false;
    bool m_bRunPythonScript = false;
    bool m_bRunPythonTestScript = false;
    bool m_bShowVersionInfo = false;
    QString m_exportFile;
    QString m_strFileName;
    QString m_appRoot;
    QString m_logFile;
    QString m_pythonArgs;
    QString m_execFile;
    QString m_execLineCmd;

    bool m_bSkipWelcomeScreenDialog = false;
    bool m_bAutotestMode = false;

#if AZ_TESTS_ENABLED
    bool m_bBootstrapPluginTests = false;
    std::vector<std::string> m_testArgs;
#endif

    struct CommandLineStringOption
    {
        QString name;
        QString description;
        QString valueName;
    };

    CEditCommandLineInfo()
    {
        bool dummy;
        QCommandLineParser parser;
        QString appRootOverride;
        parser.addHelpOption();
        parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
        parser.setApplicationDescription(QObject::tr("Amazon Lumberyard"));
        // nsDocumentRevisionDebugMode is an argument that the macOS system passed into an App bundle that is being debugged.
        // Need to include it here so that Qt argument parser does not error out.
        bool nsDocumentRevisionsDebugMode = false;
        const std::vector<std::pair<QString, bool&> > options = {
            { "export", m_bExport },
            { "exportTexture", m_bExportTexture },
            { "exportAI", m_bExportAI },
            { "test", m_bTest },
            { "auto_level_load", m_bAutoLoadLevel },
            { "PrecacheShaders", m_bPrecacheShaders },
            { "PrecacheShadersLevels", m_bPrecacheShadersLevels },
            { "PrecacheShaderList", m_bPrecacheShaderList },
            { "StatsShaders", m_bStatsShaders },
            { "StatsShaderList", m_bStatsShaderList },
            { "MergeShaders", m_bMergeShaders },
            { "MatEdit", m_bMatEditMode },
            { "BatchMode", m_bConsoleMode },
            { "NullRenderer", m_bNullRenderer },
            { "devmode", m_bDeveloperMode },
            { "VTUNE", dummy },
            { "runpython", m_bRunPythonScript },
            { "runpythontest", m_bRunPythonTestScript },
            { "version", m_bShowVersionInfo },
            { "NSDocumentRevisionsDebugMode", nsDocumentRevisionsDebugMode},
            { "skipWelcomeScreenDialog", m_bSkipWelcomeScreenDialog},
            { "autotest_mode", m_bAutotestMode}
        };

        QString dummyString;
        const std::vector<std::pair<CommandLineStringOption, QString&> > stringOptions = {
            {{"app-root", "Application Root path override", "app-root"}, m_appRoot},
            {{"logfile", "File name of the log file to write out to.", "logfile"}, m_logFile},
            {{"runpythonargs", "Command-line argument string to pass to the python script if --runpython or --runpythontest was used.", "runpythonargs"}, m_pythonArgs},
            {{"exec", "cfg file to run on startup, used for systems like automation", "exec"}, m_execFile},
            {{"rhi", "Command-line argument to force which rhi to use", "dummyString"}, dummyString },
            {{"rhi-device-validation", "Command-line argument to configure rhi validation", "dummyString"}, dummyString },
            {{"exec_line", "command to run on startup, used for systems like automation", "exec_line"}, m_execLineCmd}
            // add dummy entries here to prevent QCommandLineParser error-ing out on cmd line args that will be parsed later
        };


        parser.addPositionalArgument("file", QCoreApplication::translate("main", "file to open"));
        for (const auto& option : options)
        {
            parser.addOption(QCommandLineOption(option.first));
        }

        for (const auto& option : stringOptions)
        {
            parser.addOption(QCommandLineOption(option.first.name, option.first.description, option.first.valueName));
        }

        QStringList args = qApp->arguments();

#if AZ_TESTS_ENABLED
        // have to get all parameters after the bootstrap parameters to pass on
        QStringList filteredArgs;
        for (QString& arg : args)
        {
            if (m_bBootstrapPluginTests)
            {
                m_testArgs.push_back(arg.toStdString());
            }
            else if (arg.contains("bootstrap_plugin_tests"))
            {
                m_bBootstrapPluginTests = true;
            }
            else
            {
                filteredArgs.push_back(arg);
            }
        }
        args = filteredArgs;
#endif

#ifdef Q_OS_WIN32
        for (QString& arg : args)
        {
            if (!arg.isEmpty() && arg[0] == '/')
            {
                arg[0] = '-'; // QCommandLineParser only supports - and -- prefixes
            }
        }
#endif

        parser.process(args);

        // Get boolean options
        const int numOptions = options.size();
        for (int i = 0; i < numOptions; ++i)
        {
            options[i].second = parser.isSet(options[i].first);
        }

        // Get string options
        for (auto& option : stringOptions)
        {
            option.second = parser.value(option.first.valueName);
        }

        m_bExport = m_bExport | m_bExportTexture | m_bExportAI;

        const QStringList positionalArgs = parser.positionalArguments();

        if (!positionalArgs.isEmpty())
        {
            m_strFileName = positionalArgs.first();

            if (positionalArgs.first().at(0) != '[')
            {
                m_exportFile = positionalArgs.first();
            }
        }
    }
};

struct SharedData
{
    bool raise = false;
    char text[_MAX_PATH];
};
/////////////////////////////////////////////////////////////////////////////
// CTheApp::FirstInstance
//      FirstInstance checks for an existing instance of the application.
//      If one is found, it is activated.
//
//      This function uses a technique similar to that described in KB
//      article Q141752 to locate the previous instance of the application. .
BOOL CCryEditApp::FirstInstance(bool bForceNewInstance)
{
    QSystemSemaphore sem(QString(lumberyardApplicationName) + "_sem", 1);
    sem.acquire();
    {
        FixDanglingSharedMemory(lumberyardEditorClassName);
    }
    sem.release();
    m_mutexApplication = new QSharedMemory(lumberyardEditorClassName);
    if (!m_mutexApplication->create(sizeof(SharedData)) && !bForceNewInstance)
    {
        m_mutexApplication->attach();
        // another instance is already running - activate it
        sem.acquire();
        SharedData* data = reinterpret_cast<SharedData*>(m_mutexApplication->data());
        data->raise = true;

        if (m_bPreviewMode)
        {
            // IF in preview mode send this window copy data message to load new preview file.
            azstrcpy(data->text, MAX_PATH, m_sPreviewFile);
        }
        return false;
    }
    else
    {
        m_mutexApplication->attach();
        // this is the first instance
        sem.acquire();
        ::memset(m_mutexApplication->data(), 0, m_mutexApplication->size());
        sem.release();
        QTimer* t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this]() {
            QSystemSemaphore sem(QString(lumberyardApplicationName) + "_sem", 1);
            sem.acquire();
            SharedData* data = reinterpret_cast<SharedData*>(m_mutexApplication->data());
            QString preview = QString::fromLatin1(data->text);
            if (data->raise)
            {
                QWidget* w = MainWindow::instance();
                w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
                w->raise();
                w->activateWindow();
                data->raise = false;
            }
            if (!preview.isEmpty())
            {
                // Load this file
                LoadFile(preview);
                data->text[0] = 0;
            }
            sem.release();
        });
        t->start(1000);

        return true;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileSave()
{
    if (m_savingLevel)
    {
        return;
    }

    const QScopedValueRollback<bool> rollback(m_savingLevel, true);

    GetIEditor()->GetDocument()->DoFileSave();
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateDocumentReady(QAction* action)
{
    action->setEnabled(GetIEditor()
        && GetIEditor()->GetDocument()
        && GetIEditor()->GetDocument()->IsDocumentReady()
        && !m_bIsExportingLegacyData
        && !m_creatingNewLevel
        && !m_openingLevel
        && !m_savingLevel);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateFileOpen(QAction* action)
{
    action->setEnabled(!m_bIsExportingLegacyData && !m_creatingNewLevel && !m_openingLevel && !m_savingLevel);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateReloadLevel(QAction* action)
{
    action->setEnabled(GetIEditor() && GetIEditor()->GetDocument() && GetIEditor()->GetDocument()->IsDocumentReady() && !m_creatingNewLevel && !m_openingLevel && !m_savingLevel);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileSaveExternalLayers()
{
    GetIEditor()->GetDocument()->BackupBeforeSave();
    GetIEditor()->GetObjectManager()->GetLayersManager()->SaveExternalLayers();
}

/**
 * Return the number of legacy entities in the level (if one is loaded)
 */
static int FindNumLegacyEntities()
{
    // Check if we have a level loaded
    if (!GetIEditor()->GetDocument() || !GetIEditor()->GetDocument()->IsDocumentReady())
    {
        return -1;
    }

    // Gather the number of legacy entities (any object that isn't a ComponentEntity)
    CBaseObjectsArray objects;
    GetIEditor()->GetObjectManager()->GetObjects(objects);
    int numLegacyEntities = AZStd::count_if(objects.cbegin(), objects.cend(), [](const CBaseObject* object) {
        QString className = object->GetClassDesc()->ClassName();
        return className != "ComponentEntity";
    });

    return numLegacyEntities;
}

/**
 * Export our conversion log file to a CSV file
 */
static void ExportLegacyEntityConversionLog(QStandardItemModel* model)
{
    if (!model)
    {
        return;
    }

    QString outputFile = "ConversionLog.csv";
    QString saveFilter = "CSV Files (*.csv)";
    if (!CFileUtil::SelectSaveFile(saveFilter, "csv", QtUtil::ToQString(Path::GetEditingGameDataFolder().c_str()), outputFile))
    {
        return;
    }

    QFile file(outputFile);
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }

    // First, write out the column names, ignoring the first column since it is
    // a status icon
    QTextStream stream(&file);
    QStringList columnNames;
    const int numColumns = model->columnCount();
    for (int i = 1; i < numColumns; ++i)
    {
        columnNames.append(model->headerData(i, Qt::Horizontal).toString());
    }
    stream << columnNames.join(",") << Qt::endl;

    // Then, write out all the rows, again ignoring the first column since
    // it's just a status icon
    const int numRows = model->rowCount();
    for (int i = 0; i < numRows; ++i)
    {
        QStringList items;
        for (int j = 1; j < numColumns; ++j)
        {
            QStandardItem* item = model->item(i, j);
            if (!item)
            {
                continue;
            }

            // Wrap our items in quotes since they could contain commas
            items.append("\"" + item->text() + "\"");
        }

        stream << items.join(",") << Qt::endl;
    }

    // Write out the file
    file.close();

    // Open the log file in whichever application is registered to open .csv
    // files by default
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputFile));
}

/**
 * Display the log dialog for the legacy entity conversion process
 */
static int ShowLegacyEntityConversionLogDialog(int objectsConverted, int objectsUnconverted, QStandardItemModel* model)
{
    if (!model)
    {
        return -1;
    }

    QDialog logDialog(AzToolsFramework::GetActiveWindow());
    logDialog.setMinimumSize(800, 400);
    logDialog.setWindowTitle(QObject::tr("Conversion Log"));

    bool conversionSuccess = (objectsUnconverted == 0);
    QString singularOrPluralConverted = (objectsConverted == 1) ? QObject::tr("Entity") : QObject::tr("Entities");
    QString message = QObject::tr("%1 %2 converted.").arg(objectsConverted).arg(singularOrPluralConverted);
    if (conversionSuccess)
    {
        message += " " + QObject::tr("No warnings or errors.");
    }
    else
    {
        QString singularOrPluralUnconverted = (objectsUnconverted == 1) ? QObject::tr("Entity has") : QObject::tr("Entities have");
        message += " " + QObject::tr("%1 %2 warnings or errors.").arg(objectsUnconverted).arg(singularOrPluralUnconverted);
    }
    QLabel* messageLabel = new QLabel(&logDialog);
    messageLabel->setText(message);

    QTreeView* treeView = new QTreeView(&logDialog);
    treeView->setSelectionMode(QAbstractItemView::NoSelection);
    treeView->setFocusPolicy(Qt::NoFocus);
    treeView->setModel(model);
    treeView->setAlternatingRowColors(true);
    treeView->resizeColumnToContents(0);

    QVBoxLayout* mainLayout = new QVBoxLayout(&logDialog);
    logDialog.setLayout(mainLayout);

    // Show the appropriate message/icon if the conversion had errors/warnings
    if (conversionSuccess)
    {
        mainLayout->addWidget(messageLabel);
    }
    else
    {
        QWidget* header = new QWidget(&logDialog);
        QHBoxLayout* headerLayout = new QHBoxLayout(&logDialog);
        header->setLayout(headerLayout);
        QIcon warningIcon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
        QLabel* iconLabel = new QLabel(&logDialog);
        int iconSize = QApplication::style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
        iconLabel->setPixmap(warningIcon.pixmap(QSize(iconSize, iconSize)));
        headerLayout->addWidget(iconLabel);
        headerLayout->addWidget(messageLabel, 1);
        mainLayout->addWidget(header);
    }

    // Show the conversion log entries below the message header and let
    // it stretch to fill the available vertical space
    mainLayout->addWidget(treeView, 1);

    // Create back/ok buttons that will be used to reject/accept our dialog, respectively
    QPushButton* backButton = new QPushButton(QObject::tr("Back"), &logDialog);
    QObject::connect(backButton, &QPushButton::clicked, &logDialog, &QDialog::reject);

    // Add button for exporting our log to a CSV file
    QPushButton* exportLogButton = new QPushButton(QObject::tr("Export Log"), &logDialog);
    QObject::connect(exportLogButton, &QPushButton::clicked, model, [model]
    {
        ExportLegacyEntityConversionLog(model);
    });

    // Add our buttons to the bottom of our layout
    QWidget* footer = new QWidget(&logDialog);
    QHBoxLayout* buttonLayout = new QHBoxLayout(&logDialog);
    footer->setLayout(buttonLayout);
    buttonLayout->addWidget(backButton, 0, Qt::AlignLeft);
    buttonLayout->addWidget(exportLogButton, 0, Qt::AlignRight);
    mainLayout->addWidget(footer);

    return logDialog.exec();
}

/**
 * Show dialog for enabling or disabling the CryEntityRemoval gem that will
 * open the Project Configurator for the user if chosen
 */
bool CCryEditApp::ShowEnableDisableCryEntityRemovalDialog()
{
    bool isLegacyUIEnabled = GetIEditor()->IsLegacyUIEnabled();
    const QString title = (isLegacyUIEnabled) ? QObject::tr("Enable CryEntityRemoval Gem") : QObject::tr("Disable CryEntityRemoval Gem");
    const QString message = QObject::tr("You must use the Project Configurator to enable/disable the <b>CryEntityRemoval</b> gem for your project, which requires the Editor to be closed.");
    return ShowEnableDisableGemDialog(title, message);
}

bool CCryEditApp::ShowEnableDisableGemDialog(const QString& title, const QString& message, bool restartEditor)
{
    const QString informativeMessage = QObject::tr("Please follow the instructions <a href=\"http://docs.aws.amazon.com/console/lumberyard/gems\">here</a>, after which the Editor will be re-launched automatically.");

    QMessageBox box(AzToolsFramework::GetActiveWindow());
    box.addButton(QObject::tr("Continue"), QMessageBox::AcceptRole);
    box.addButton(QObject::tr("Back"), QMessageBox::RejectRole);
    box.setWindowTitle(title);
    box.setText(message);
    box.setInformativeText(informativeMessage);
    box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    if (box.exec() == QMessageBox::AcceptRole)
    {
        OpenProjectConfigurator(PROJECT_CONFIGURATOR_GEM_PAGE, restartEditor);
        // Called from a modal dialog with the main window as its parent. Best not to close the main window while the dialog is still active.
        QTimer::singleShot(0, MainWindow::instance(), &MainWindow::close);
        return true;
    }

    return false;
}

void CCryEditApp::AddLegacyTerrainLevelComponentIfNecessary(bool& editorWillClose)
{
    editorWillClose = false;

#ifdef LY_TERRAIN_EDITOR

    if (AzFramework::Terrain::TerrainDataRequestBus::HasHandlers())
    {
        //No further questions
        return;
    }

    bool levelWasCreatedWithLegacyTerrain = false;
    XmlNodeRef node = GetIEditor()->GetDocument()->GetEnvironmentTemplate();
    if (node)
    {
        XmlNodeRef envState = node->findChild("EnvState");
        if (envState)
        {
            XmlNodeRef showTerrainSurface = envState->findChild("ShowTerrainSurface");
            const char* attrValue = showTerrainSurface->getAttr("value");
            if (!attrValue || (attrValue[0] == '\0'))
            {
                //The required data not found. Not enough information to make a decision.
                //quietly do nothing.
                return;
            }
            //@attrValue is normally one of the following values: "false" or "1".
            if ((attrValue[0] == '1') || (attrValue[0] == 't') || (attrValue[0] == 'T'))
            {
                levelWasCreatedWithLegacyTerrain = true;
            }
        }
    }
    if (!levelWasCreatedWithLegacyTerrain)
    {
        //Nothing to do.
        return;
    }

    //Need to add the "Legacy Terrain" level component.
    if (!AddLegacyTerrainLevelComponent())
    {
        //Most likely the Legacy Terrain Gem is not enabled.
        //Notify the user.
        const QString title = QObject::tr("Enable Legacy Terrain Gem");
        const QString message = QObject::tr("This level was originally created with Legacy Terrain data. "
            "You must use the Project Configurator to enable the <b>Legacy Terrain</b> gem for your project, which requires the Editor to be closed.");
        const bool restartEditor = false;
        editorWillClose = ShowEnableDisableGemDialog(title, message, restartEditor);
        return;
    }

    {
        //Let the user know they should save and possibly re-export to engine.
        QMessageBox box(AzToolsFramework::GetActiveWindow());
        box.addButton(QObject::tr("Ok"), QMessageBox::AcceptRole);

        const QString title = QObject::tr("Added Legacy Terrain Level Component");
        box.setWindowTitle(title);

        const QString message = QObject::tr("This level was originally created with Legacy Terrain data. "
            "For your convenience, the <b>Legacy Terrain</b> level component has been added to this level (See Level Inspector). "
            "It is recommended to save this level. To make this level functional in the Game Client re-exporting it is also necessary.");
        box.setText(message);
        box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
        box.exec();
    }
    

#endif //#ifdef LY_TERRAIN_EDITOR
}

void CCryEditApp::OnFileConvertLegacyEntities()
{
    // Don't close the level if the conversion dialog is cancelled if the user
    // triggered the dialog by the file menu
    ConvertLegacyEntities(false);
}

void CCryEditApp::ConvertLegacyEntities(bool closeOnCancel)
{
    using namespace AZ::LegacyConversion;
    using namespace AzToolsFramework;
    AZ::EBusLogicalResult<bool, AZStd::logical_and<bool> > okToContinue(true);

    LegacyConversionEventBus::BroadcastResult(okToContinue, &LegacyConversionEvents::BeforeConversionBegins);

    if (!okToContinue.value)
    {
        AZ_TracePrintf("Legacy Conversion", "Legacy Conversion was cancelled by a system within the editor.\n");
        return;
    }

    // actual conversion process here:
    DynArray<CBaseObject*> objects;
    GetIEditor()->GetObjectManager()->GetObjects(objects);
    int objectsConvertedCount = 0;
    int unconvertedTotal = 0;

    // sort all the objects such that objects with parents are first and the deeper in the tree they are the farther back in the list they are
    qsort(objects.data(), objects.size(), sizeof(CBaseObject*), [](const void* first, const void* second)
    {
        CBaseObject* a = *(CBaseObject**)first;
        CBaseObject* b = *(CBaseObject**)second;

        if (!a)
        {
            return 1;
        }
        if (!b)
        {
            return -1;
        }

        int numParentsA = 0;
        CBaseObject* parent = a;
        do
        {
            parent = parent->GetParent();
            ++numParentsA;
        } while (parent);

        int numParentsB = 0;
        parent = b;
        do
        {
            parent = parent->GetParent();
            ++numParentsB;
        } while (parent);

        // whoever has the least parents should go first.
        // so if A has 10 parents and b has 20 parents, then A must go first, thus
        // we need to return a-b (10 - 20)
        return numParentsA - numParentsB;
    });

    // If we have no legacy entities then bail out
    bool isLegacyUIEnabled = GetIEditor()->IsLegacyUIEnabled();
    QString componentEntityInformativeText = QObject::tr("For more information on getting started with Component-Entities, visit <a href=\"http://docs.aws.amazon.com/lumberyard/latest/userguide/component-working.html\">Documentation.</a>");
    if (isLegacyUIEnabled)
    {
        componentEntityInformativeText = QObject::tr("If you want to work with the new Component-Entity tools, you will need to enable the \"CryEntityRemoval\" Gem.<br>") + componentEntityInformativeText;
    }
    int numLegacyEntities = FindNumLegacyEntities();
    if (numLegacyEntities == 0)
    {
        bool done = false;
        do
        {
            QMessageBox box(AzToolsFramework::GetActiveWindow());
            box.setWindowTitle(QObject::tr("Convert Entities"));
            box.setText(QObject::tr("This level has no Legacy Entities to be converted."));

            // If we are in legacy mode, give them the option to enable the
            // CryEntityRemoval gem since there are no legacy entities in
            // the level
            QPushButton* enableGemButton = nullptr;
            if (isLegacyUIEnabled)
            {
                box.setInformativeText(componentEntityInformativeText);
                box.addButton(QObject::tr("OK"), QMessageBox::RejectRole);
                enableGemButton = box.addButton(QObject::tr("Enable Gem"), QMessageBox::ActionRole);
            }
            else
            {
                box.addButton(QMessageBox::Ok);
            }

            box.exec();
            QAbstractButton* clickedButton = box.clickedButton();
            if (clickedButton && clickedButton == enableGemButton)
            {
                done = ShowEnableDisableCryEntityRemovalDialog();
            }
            else
            {
                done = true;
            }
        } while (!done);

        return;
    }

    // Let the user confirm that they want to convert their legacy entities
    bool done = false;
    bool singleLegacyEntity = (numLegacyEntities == 1);
    QString singularOrPlural = (singleLegacyEntity) ? QObject::tr("Entity") : QObject::tr("Entities");
    QString thisOrThese = (singleLegacyEntity) ? QObject::tr("This") : QObject::tr("These");
    QString hasOrHave = (singleLegacyEntity) ? QObject::tr("has") : QObject::tr("have");
    do
    {
        QString singularArticle = (singleLegacyEntity) ? "a " : "";
        QString proceedingMessage = (closeOnCancel) ? QObject::tr("To continue opening this level") : QObject::tr("If you want to work with the new Component - Entity tools");
        QString mustOrCan = (closeOnCancel) ? QObject::tr("must") : QObject::tr("can");
        QMessageBox box(AzToolsFramework::GetActiveWindow());
        box.addButton(QObject::tr("Convert %1").arg(singularOrPlural), QMessageBox::AcceptRole);
        if (closeOnCancel)
        {
            box.addButton(QObject::tr("Close Level"), QMessageBox::RejectRole);
        }
        else
        {
            box.addButton(QMessageBox::Cancel);
        }
        box.setWindowTitle(QObject::tr("Legacy %1 Detected").arg(singularOrPlural));
        box.setText(QObject::tr("The level you loaded has <b>%1 Legacy %2</b> in it. %4, the Legacy %2 %5 be converted into %3Component %2.").arg(numLegacyEntities).arg(singularOrPlural).arg(singularArticle).arg(proceedingMessage).arg(mustOrCan));
        box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);

        // If the legacy UI is disabled, we offer the user the ability to turn
        // it back on through the Editor preferences, but only if their project
        // doesn't have the CryEntityRemoval gem
        bool isCryEntityRemovalGemPresent = AzToolsFramework::IsComponentWithServiceRegistered(AZ_CRC("CryEntityRemovalService", 0x229a458c));
        QPushButton* enableLegacyUI = nullptr;
        if (!isLegacyUIEnabled && !isCryEntityRemovalGemPresent)
        {
            enableLegacyUI = box.addButton(QObject::tr("Enable Legacy UI"), QMessageBox::ActionRole);
            box.setInformativeText(QObject::tr("If you would like to open the level without converting, you will need to enable the Legacy UI by selecting \"Enable Legacy UI\" below. The setting can also be changed at any time by selecting \"Enable Legacy UI\" via \"Edit->Editor Settings->Global Preferences...\". Be aware that the option to enable the Legacy UI will be removed in a future update of Lumberyard and the Legacy tools are no longer supported."));
        }

        int result = box.exec();
        if (box.clickedButton() == enableLegacyUI)
        {
            QString title = QObject::tr("Enable Legacy UI");
            QString message = QObject::tr("The Legacy UI must be enabled to access the Legacy tools. Once you confirm, the Editor will be shut down so this setting can be applied. You will need to relaunch the Editor for the changes to take effect. You can switch back to the Component Entity UI by toggling the Enable Legacy UI setting under \"Edit->Editor Settings->Global Preferences...\"");

            QMessageBox box(AzToolsFramework::GetActiveWindow());
            box.addButton(QObject::tr("Confirm"), QMessageBox::AcceptRole);
            box.addButton(QObject::tr("Back"), QMessageBox::RejectRole);
            box.setWindowTitle(title);
            box.setText(message);
            box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
            if (box.exec() == QMessageBox::AcceptRole)
            {
                // Flip the switch to enable the legacy UI, but this doesn't do
                // anything unless the Editor is restarted, so close it afterwards
                gSettings.enableLegacyUI = true;
                gSettings.Save();

                // Called from a modal dialog with the main window as its parent. Best not to close the main window while the dialog is still active.
                QTimer::singleShot(0, MainWindow::instance(), &MainWindow::close);
                return;
            }
        }
        else if (result != QMessageBox::AcceptRole)
        {
            // If the user doesn't want to convert their entities or disable the gem,
            // we need to close the level because there are likely entities they will
            // be unable to edit without the legacy UI
            if (closeOnCancel && !isLegacyUIEnabled)
            {
                CloseCurrentLevel();
            }

            return;
        }
        else
        {
            done = true;
        }
    } while (!done);

    // Create a model for storing our conversion logs
    QPixmap errorIcon(QStringLiteral(":/stylesheet/img/table_error.png"));
    QPixmap successIcon(QStringLiteral(":/stylesheet/img/table_success.png"));
    QStandardItemModel* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({ tr("Status"), tr("Name"), tr("Type"), tr("Message") });

    // Starting the actual conversion now so start our wait progress indicator
    CWaitProgress wait("Converting objects...");

    // Show a progress bar for our conversion status
    QDialog* progressDialog = new QDialog(AzToolsFramework::GetActiveWindow());
    progressDialog->setMinimumSize(500, 50);
    progressDialog->setWindowTitle(QObject::tr("Converting Entities"));
    QVBoxLayout* progressLayout = new QVBoxLayout(progressDialog);
    progressLayout->setContentsMargins(15, 9, 15, 9);
    progressDialog->setLayout(progressLayout);
    QProgressBar* progressBar = new QProgressBar(progressDialog);
    QLabel* progressLabel = new QLabel(progressDialog);
    progressLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    progressLayout->addWidget(progressBar);
    progressLayout->addWidget(progressLabel);
    progressDialog->setModal(true);
    progressDialog->show();
    progressDialog->raise();

    GetIEditor()->GetDocument()->BackupBeforeSave(true); // force a save backup!

    {
        ScopedUndoBatch componentEntityUndo("Convert Legacy Entities");
        // scoped so that undo can operate.
        CSelectionGroup objectsToDeleteSelectionManager;

        AZStd::unordered_set<CBaseObject*> objectsConverted;

        float lastStep = -1000.0f;

        for (size_t idx = 0; idx < objects.size(); ++idx)
        {
            float newStep = (static_cast<float>(idx) / static_cast<float>(objects.size())) * 100.f;
            if (newStep - lastStep >= 5.0f) // we update every 5% - any more than that and we're going to slow things down with UI updates.
            {
                wait.Step(newStep);
                progressBar->setValue(newStep);
                progressLabel->setText(QObject::tr("%1 / %2 conversions complete").arg(objectsConvertedCount).arg(numLegacyEntities));
                lastStep = newStep;
            }
            CBaseObject* object = objects[idx];

            if (!okToContinue.value)
            {
                break;
            }

            // ignore "ComponentEntity" since those are the already converted ones.
            QString className = object->GetClassDesc()->ClassName();
            if (className == "ComponentEntity")
            {
                continue;
            }

            // Store the name/type of our object now since it could be removed
            // during the conversion process so we can log its conversion status
            QString name = object->GetName();
            QString type = object->GetTypeDescription();
            QString message;
            bool success = false;

            // Convert the entity
            AZ::EBusAggregateResults<LegacyConversionResult> results;
            LegacyConversionEventBus::BroadcastResult(results, &LegacyConversionEvents::ConvertEntity, object);
            // so what happened?
            bool gotKeepEntity = false;
            bool gotDeleteEntity = false;
            for (const LegacyConversionResult& result : results.values)
            {
                if (result == LegacyConversionResult::Failed)
                {
                    // if we failed, we need to terminate this process.
                    message = QObject::tr("An attempt to convert entity %1 failed - rolling back.").arg(name);
                    QByteArray messageUtf8 = message.toUtf8();
                    AZ_Error("Legacy Conversion", false, messageUtf8.data());
                    okToContinue = false;
                    break;
                }
                else if (result == LegacyConversionResult::HandledKeepEntity)
                {
                    gotKeepEntity = true;
                    objectsConvertedCount++;
                    objectsConverted.insert(object);
                }
                else if (result == LegacyConversionResult::HandledDeleteEntity)
                {
                    gotDeleteEntity = true;
                    objectsConvertedCount++;
                    objectsConverted.insert(object);
                }
            }

            if ((gotDeleteEntity) && (!gotKeepEntity))
            {
                // nobody vetoed the deletion.
                objectsToDeleteSelectionManager.AddObject(object);
            }

            // Detect if this object was successfully converted, or if we
            // ignored the object because we didn't know how to convert it
            if (okToContinue.value)
            {
                if (gotKeepEntity || gotDeleteEntity)
                {
                    success = true;
                    message = QObject::tr("Successfully converted.");
                }
                else
                {
                    message = QObject::tr("No conversion available for this object.");
                }
            }

            // Add a row for the conversion status of this object
            QStandardItem* iconItem = new QStandardItem();
            QVariant icon = (success) ? successIcon : errorIcon;
            iconItem->setData(icon, Qt::DecorationRole);
            iconItem->setFlags(Qt::ItemIsEnabled);
            QStandardItem* nameItem = new QStandardItem(name);
            nameItem->setFlags(Qt::ItemIsEnabled);
            QStandardItem* typeItem = new QStandardItem(type);
            typeItem->setFlags(Qt::ItemIsEnabled);
            QStandardItem* messageItem = new QStandardItem(message);
            messageItem->setFlags(Qt::ItemIsEnabled);
            model->appendRow({
                iconItem,
                nameItem,
                typeItem,
                messageItem
            });
        }

        if (okToContinue.value)
        {
            // prefabs are special in that we revert the changing of any prefab who's children didn't get completely converted:
            const IClassDesc* prefabClass = GetIEditor()->GetClassFactory()->FindClass("Prefab");
            for (size_t idx = 0; idx < objects.size(); ++idx)
            {
                const IClassDesc* objectClass = objects[idx]->GetClassDesc();
                if (objectClass == prefabClass)
                {
                    if (objectsConverted.find(objects[idx]) == objectsConverted.end())
                    {
                        continue;
                    }

                    // traverse the heirarchy and make sure every child of the prefab (and their children) were updated, or else abandon them
                    // note that the below is a non-recursive child expansion function, using an active ('to be inspected') list and an 'already inspected' list.
                    AZStd::vector<CBaseObject*> expansionList;
                    AZStd::vector<CBaseObject*> expandedList;
                    expansionList.push_back(objects[idx]);
                    while (!expansionList.empty())
                    {
                        CBaseObject* inspect = expansionList.back();
                        expansionList.pop_back();

                        expandedList.push_back(inspect);
                        for (size_t childIdx = 0; childIdx < inspect->GetChildCount(); ++childIdx)
                        {
                            expansionList.push_back(inspect->GetChild(childIdx));
                        }
                    }

                    // if any children were not converted, we need to revert all children
                    bool foundUnconvertedChild = false;
                    for (CBaseObject* objectToCheck : expandedList)
                    {
                        if (objectsConverted.find(objectToCheck) == objectsConverted.end())
                        {
                            AZ_TracePrintf("Legacy Conversion", "Legacy Conversion did not manage to convert prefab '%s' child object  '%s' (type '%s'), so the whole prefab will be kept as-is.\n",
                                objects[idx]->GetName().toUtf8().data(),
                                objectToCheck->GetName().toUtf8().data(),
                                objectToCheck->GetClassDesc()->ClassName().toUtf8().data());

                            foundUnconvertedChild = true;
                            // we could 'break' here, but we'd rather print out the above notification for each one we encounter
                        }
                    }

                    if (foundUnconvertedChild)
                    {
                        // we have to revert and this means deleting the newly created objects.
                        for (CBaseObject* objectToCheck : expandedList)
                        {
                            if (objectsConverted.find(objectToCheck) != objectsConverted.end())
                            {
                                objectsConverted.erase(objectToCheck);
                                objectsConvertedCount--;
                            }
                            objectsToDeleteSelectionManager.RemoveObject(objectToCheck); // don't delete the actual prefab object either.

                            AZ::EntityId existingEntityId;
                            existingEntityId.SetInvalid();
                            LegacyConversionRequestBus::BroadcastResult(existingEntityId, &LegacyConversionRequests::FindCreatedEntityByExistingObject, objectToCheck);
                            if (existingEntityId.IsValid())
                            {
                                EntityIdList deleter;
                                deleter.push_back(existingEntityId);
                                EBUS_EVENT(ToolsApplicationRequests::Bus, DeleteEntitiesAndAllDescendants, deleter);
                            }
                        }
                    }
                }
            }
            GetIEditor()->GetObjectManager()->DeleteSelection(&objectsToDeleteSelectionManager);
            okToContinue.value = true;
            LegacyConversionEventBus::BroadcastResult(okToContinue, &LegacyConversionEvents::AfterConversionEnds);
        }
        // leaving this scope "seals" the undo stack, no more edits should be made after this.
        // it costs a lot of time to do this so update one last time before we do.
        wait.Step(99.0f);
    }

    if (!okToContinue.value)
    {
        AZ_TracePrintf("Legacy Conversion", "Legacy Conversion was rolled back by a system within the editor.\n");
        GetIEditor()->Undo();
    }
    else
    {
        // for convenience, tally them up
        AZStd::map<AZStd::string, int> conversionTallies;
        objects.clear();
        GetIEditor()->GetObjectManager()->GetObjects(objects);
        if (!objects.empty())
        {
            for (CBaseObject* object : objects)
            {
                AZStd::string className = object->GetClassDesc()->ClassName().toUtf8().data();

                // ignore "ComponentEntity" since those are the already converted ones.
                if (className == "ComponentEntity")
                {
                    continue;
                }

                ++unconvertedTotal;

                auto existing = conversionTallies.find(className);
                if (existing == conversionTallies.end())
                {
                    conversionTallies.insert(AZStd::make_pair(AZStd::string(object->GetClassDesc()->ClassName().toUtf8().data()), 1));
                }
                else
                {
                    ++(existing->second);
                }

                // If this conversion was triggered after detecting legacy entities
                // when the level was loaded, then we need to delete any objects
                // that couldn't be converted
                if (closeOnCancel)
                {
                    GetIEditor()->GetObjectManager()->DeleteObject(object);
                }
            }

            AZ_TracePrintf("Legacy Conversion", "Deleted or remaining object counts:\n");
            for (auto current = conversionTallies.begin(); current != conversionTallies.end(); ++current)
            {
                AZ_TracePrintf("Legacy Conversion", "    '%s' deleted or remaining: %i\n", current->first.c_str(), current->second);
            }
        }
        if (objectsConvertedCount > 0)
        {
            AZ_TracePrintf("Legacy Conversion", "%i object(s) were converted.\n", objectsConvertedCount);
            AZ_TracePrintf("Legacy Conversion", "%i object(s) were deleted or remain.\n", unconvertedTotal);
        }
        else
        {
            AZ_TracePrintf("Legacy Conversion", "No objects were converted.\n", objectsConvertedCount);
        }

        AZ_TracePrintf("Legacy Conversion", "Legacy Conversion complete.\n");
    }

    // If the legacy UI is disabled, don't let the user undo the conversion batch
    // since they could be left in a state where they can't edit their entities
    if (!isLegacyUIEnabled)
    {
        GetIEditor()->ClearLastUndoSteps(1);

        // Since we cleared the modifications from the undo stack, we need to
        // manually set the modified flag so the user will still be presented
        // with a dirty changes dialog if they try to close the Editor or switch
        // to a new level
        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedEntities);
    }

    // Conversion is complete so stop our wait progress indicator/cursor
    wait.Stop();

    // Remove our progress dialog before we show the completion dialog
    delete progressDialog;
    progressDialog = nullptr;

    // Display our completion dialog
    bool conversionSuccess = (unconvertedTotal == 0);
    done = false;
    do
    {
        QMessageBox finishedDialog(AzToolsFramework::GetActiveWindow());
        finishedDialog.setWindowFlags(finishedDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
        QPushButton* viewLog = finishedDialog.addButton(QObject::tr("View Log"), QMessageBox::ActionRole);

        // We give the Continue button the RejectRole so that it will be the action
        // taken when the X button on the dialog titlebar is pressed
        finishedDialog.addButton(QObject::tr("Continue"), QMessageBox::RejectRole);

        // Set the appropriate title and text if we converted all of the entities
        QString finishedTitle;
        QString finishedText;
        QPushButton* enableGem = nullptr;
        QPushButton* revertAndClose = nullptr;
        if (conversionSuccess)
        {
            QString theOrAll = (singleLegacyEntity) ? QObject::tr("the") : QObject::tr("all");
            finishedTitle = QObject::tr("Conversion Complete");
            finishedText = QObject::tr("<b>%1 Legacy %2 %3 been converted to Component-%2.</b><br/>To review %4 converted %2, click \"View Log\".").arg(objectsConvertedCount).arg(singularOrPlural).arg(hasOrHave).arg(theOrAll);

            // If the legacy UI is enabled and the conversion was successful,
            // then offer the user the option to enable the new Cry-Entity free UI
            if (isLegacyUIEnabled)
            {
                enableGem = finishedDialog.addButton(QObject::tr("Enable Gem"), QMessageBox::ActionRole);
            }
        }
        else
        {
            QString deletedOrUnconverted = (closeOnCancel) ? QObject::tr("%1 %2 %3 been removed from the level. ").arg(thisOrThese).arg(singularOrPlural).arg(hasOrHave) : QObject::tr("%1 %2 will exist in your game, but will not be editable. ").arg(thisOrThese).arg(singularOrPlural);
            if (isLegacyUIEnabled)
            {
                // Don't need to warn about objects being deleted or not being editable if the legacy UI is still enabled
                deletedOrUnconverted = "";
            }
            finishedTitle = QObject::tr("Conversion Error");
            finishedDialog.setIcon(QMessageBox::Warning);
            finishedText = QObject::tr("<b>%1 Legacy %2 could not be converted at this time.</b><br/>%3Click \"View Log\" for more details.\nYou may either continue to edit your level with the conversion changes, or revert the changes and close the level.").arg(unconvertedTotal).arg(singularOrPlural).arg(deletedOrUnconverted);

            // Give user option to revert changes and close level if there were errors
            revertAndClose = finishedDialog.addButton(QObject::tr("Revert and Close"), QMessageBox::AcceptRole);
        }
        finishedDialog.setWindowTitle(finishedTitle);
        finishedDialog.setText(finishedText);
        finishedDialog.setInformativeText(componentEntityInformativeText);

        // Display the finished dialog
        int result = finishedDialog.exec();
        QAbstractButton* clickedButton = finishedDialog.clickedButton();
        if (clickedButton == enableGem)
        {
            // Give user the option to enable the CryEntityRemoval gem, and if
            // they choose yes, then bail out since the Editor will be closed
            if (ShowEnableDisableCryEntityRemovalDialog())
            {
                return;
            }
        }
        else if (clickedButton == revertAndClose)
        {
            done = true;
            CloseCurrentLevel();
        }
        else if (clickedButton == viewLog)
        {
            int result = ShowLegacyEntityConversionLogDialog(objectsConvertedCount, unconvertedTotal, model);
            // If the user pressed back, the completion dialog will be shown
            // again, otherwise, control will be given back to the user
            if (result == QDialog::Accepted)
            {
                done = true;
            }
        }
        else
        {
            done = true;
        }
    } while (!done);

    // Cleanup our conversion log model
    delete model;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::InitDirectory()
{
    //////////////////////////////////////////////////////////////////////////
    // Initializes Root folder of the game.
    //////////////////////////////////////////////////////////////////////////
    QString szExeFileName = qApp->applicationDirPath();
    const static char* s_engineMarkerFile = "engine.json";

    while (!QFile::exists(QString("%1/%2").arg(szExeFileName, s_engineMarkerFile)))
    {
        QDir currentdir(szExeFileName);
        if (!currentdir.cdUp())
        {
            break;
        }
        szExeFileName = currentdir.absolutePath();
    }
    QDir::setCurrent(szExeFileName);
}


//////////////////////////////////////////////////////////////////////////
// Needed to work with custom memory manager.
//////////////////////////////////////////////////////////////////////////

CCryEditDoc* CCrySingleDocTemplate::OpenDocumentFile(LPCTSTR lpszPathName, BOOL bMakeVisible /*= true*/)
{
    return OpenDocumentFile(lpszPathName, true, bMakeVisible);
}

CCryEditDoc* CCrySingleDocTemplate::OpenDocumentFile(LPCTSTR lpszPathName, BOOL bAddToMRU, BOOL bMakeVisible)
{
    CCryEditDoc* pCurDoc = GetIEditor()->GetDocument();

    if (pCurDoc)
    {
        if (!pCurDoc->SaveModified())
        {
            return nullptr;
        }
    }

    if (!pCurDoc)
    {
        pCurDoc = qobject_cast<CCryEditDoc*>(m_documentClass->newInstance());
        if (pCurDoc == nullptr)
            return nullptr;
        pCurDoc->setParent(this);
    }

    pCurDoc->SetModifiedFlag(false);
    if (lpszPathName == nullptr)
    {
        pCurDoc->SetTitle(tr("Untitled"));
        pCurDoc->OnNewDocument();
    }
    else
    {
        pCurDoc->OnOpenDocument(lpszPathName);
        pCurDoc->SetPathName(lpszPathName);
        if (bAddToMRU)
        {
            CCryEditApp::instance()->AddToRecentFileList(lpszPathName);
        }
    }

    return pCurDoc;
}

CCrySingleDocTemplate::Confidence CCrySingleDocTemplate::MatchDocType(LPCTSTR lpszPathName, CCryEditDoc*& rpDocMatch)
{
    assert(lpszPathName != NULL);
    rpDocMatch = NULL;

    // go through all documents
    CCryEditDoc* pDoc = GetIEditor()->GetDocument();
    if (pDoc)
    {
        QString prevPathName = pDoc->GetLevelPathName();
        // all we need to know here is whether it is the same file as before.
        if (!prevPathName.isEmpty())
        {
            // QFileInfo is guaranteed to return true iff the two paths refer to the same path.
            if (QFileInfo(prevPathName) == QFileInfo(QString::fromUtf8(lpszPathName)))
            {
                // already open
                rpDocMatch = pDoc;
                return yesAlreadyOpen;
            }
        }
    }

    // see if it matches our default suffix
    const QString strFilterExt = defaultFileExtension;
    const QString strOldFilterExt = oldFileExtension;
    const QString strSliceFilterExt = AzToolsFramework::SliceUtilities::GetSliceFileExtension().c_str();

    // see if extension matches
    assert(strFilterExt[0] == '.');
    QString strDot = "." + Path::GetExt(lpszPathName);
    if (!strDot.isEmpty())
    {
        if(strDot == strFilterExt || strDot == strOldFilterExt || strDot == strSliceFilterExt)
        {
            return yesAttemptNative; // extension matches, looks like ours
        }
    }
    // otherwise we will guess it may work
    return yesAttemptForeign;
}

/////////////////////////////////////////////////////////////////////////////
namespace
{
    CryMutex g_splashScreenStateLock;
    CryConditionVariable g_splashScreenStateChange;
    enum ESplashScreenState
    {
        eSplashScreenState_Init, eSplashScreenState_Started, eSplashScreenState_Destroy
    };
    ESplashScreenState g_splashScreenState = eSplashScreenState_Init;
    IInitializeUIInfo* g_pInitializeUIInfo = nullptr;
    QWidget* g_splashScreen = nullptr;
}

QString FormatVersion(const SFileVersion& v)
{
#if defined(LY_BUILD)
    return QObject::tr("Version %1.%2.%3.%4 - Build %5").arg(v[3]).arg(v[2]).arg(v[1]).arg(v[0]).arg(LY_BUILD);
#else
    return QObject::tr("Version %1.%2.%3.%4").arg(v[3]).arg(v[2]).arg(v[1]).arg(v[0]);
#endif
}

QString FormatRichTextCopyrightNotice()
{
    // copyright symbol is HTML Entity = &#xA9;
    QString copyrightHtmlSymbol = "&#xA9;";
    QString copyrightString = QObject::tr("Lumberyard and related materials Copyright %1 %2 Amazon Web Services, Inc., its affiliates or licensors.<br>By accessing or using these materials, you agree to the terms of the AWS Customer Agreement.");

    return copyrightString.arg(copyrightHtmlSymbol).arg(LUMBERYARD_COPYRIGHT_YEAR);
}

/////////////////////////////////////////////////////////////////////////////
void CCryEditApp::ShowSplashScreen(CCryEditApp* app)
{
    g_splashScreenStateLock.Lock();

    CStartupLogoDialog* splashScreen = new CStartupLogoDialog(FormatVersion(app->m_pEditor->GetFileVersion()), FormatRichTextCopyrightNotice());

    g_pInitializeUIInfo = splashScreen;
    g_splashScreen = splashScreen;
    g_splashScreenState = eSplashScreenState_Started;

    g_splashScreenStateLock.Unlock();
    g_splashScreenStateChange.Notify();

    splashScreen->show();
    // Make sure the initial paint of the splash screen occurs so we dont get stuck with a blank window
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QObject::connect(splashScreen, &QObject::destroyed, splashScreen, [=]
    {
        g_splashScreenStateLock.Lock();
        g_pInitializeUIInfo = nullptr;
        g_splashScreen = nullptr;
        g_splashScreenStateLock.Unlock();
    });
}

/////////////////////////////////////////////////////////////////////////////
void CCryEditApp::CreateSplashScreen()
{
    if (!m_bConsoleMode && !IsInAutotestMode())
    {
        // Create startup output splash
        ShowSplashScreen(this);

        GetIEditor()->Notify(eNotify_OnSplashScreenCreated);
    }
    else
    {
        // Create console
        m_pConsoleDialog = new CConsoleDialog();
        m_pConsoleDialog->show();

        g_pInitializeUIInfo = m_pConsoleDialog;
    }
}

/////////////////////////////////////////////////////////////////////////////
void CCryEditApp::CloseSplashScreen()
{
    if (CStartupLogoDialog::instance())
    {
        delete CStartupLogoDialog::instance();
        g_splashScreenStateLock.Lock();
        g_splashScreenState = eSplashScreenState_Destroy;
        g_splashScreenStateLock.Unlock();
    }

    GetIEditor()->Notify(eNotify_OnSplashScreenDestroyed);
}

/////////////////////////////////////////////////////////////////////////////
void CCryEditApp::OutputStartupMessage(QString str)
{
    g_splashScreenStateLock.Lock();
    if (g_pInitializeUIInfo)
    {
        g_pInitializeUIInfo->SetInfoText(str.toUtf8().data());
    }
    g_splashScreenStateLock.Unlock();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::InitFromCommandLine(CEditCommandLineInfo& cmdInfo)
{
    //! Setup flags from command line
    if (cmdInfo.m_bPrecacheShaders || cmdInfo.m_bPrecacheShadersLevels || cmdInfo.m_bMergeShaders
        || cmdInfo.m_bPrecacheShaderList || cmdInfo.m_bStatsShaderList || cmdInfo.m_bStatsShaders)
    {
        m_bPreviewMode = true;
        m_bConsoleMode = true;
        m_bTestMode = true;
    }
    m_bConsoleMode |= cmdInfo.m_bConsoleMode;
    inEditorBatchMode = AZ::Environment::CreateVariable<bool>("InEditorBatchMode", m_bConsoleMode);

    m_bTestMode |= cmdInfo.m_bTest;

    m_bSkipWelcomeScreenDialog = cmdInfo.m_bSkipWelcomeScreenDialog || !cmdInfo.m_execFile.isEmpty() || !cmdInfo.m_execLineCmd.isEmpty() || cmdInfo.m_bAutotestMode;
    m_bPrecacheShaderList = cmdInfo.m_bPrecacheShaderList;
    m_bStatsShaderList = cmdInfo.m_bStatsShaderList;
    m_bStatsShaders = cmdInfo.m_bStatsShaders;
    m_bPrecacheShaders = cmdInfo.m_bPrecacheShaders;
    m_bPrecacheShadersLevels = cmdInfo.m_bPrecacheShadersLevels;
    m_bMergeShaders = cmdInfo.m_bMergeShaders;
    m_bExportMode = cmdInfo.m_bExport;
    m_bRunPythonTestScript = cmdInfo.m_bRunPythonTestScript;
    m_bRunPythonScript = cmdInfo.m_bRunPythonScript || cmdInfo.m_bRunPythonTestScript;
    m_execFile = cmdInfo.m_execFile;
    m_execLineCmd = cmdInfo.m_execLineCmd;
    m_bAutotestMode = cmdInfo.m_bAutotestMode || cmdInfo.m_bConsoleMode;

    m_pEditor->SetMatEditMode(cmdInfo.m_bMatEditMode);

    if (m_bExportMode)
    {
        m_exportFile = cmdInfo.m_exportFile;
    }

    // Do we have a passed filename ?
    if (!cmdInfo.m_strFileName.isEmpty())
    {
        if (!m_bRunPythonScript && IsPreviewableFileType(cmdInfo.m_strFileName.toUtf8().constData()))
        {
            m_bPreviewMode = true;
            azstrncpy(m_sPreviewFile, _MAX_PATH, cmdInfo.m_strFileName.toUtf8().constData(), _MAX_PATH);
        }
    }

    if (cmdInfo.m_bAutoLoadLevel)
    {
        m_bLevelLoadTestMode = true;
        gEnv->bNoAssertDialog = true;
        CEditorAutoLevelLoadTest::Instance();
    }
}

/////////////////////////////////////////////////////////////////////////////
AZ::Outcome<void, AZStd::string> CCryEditApp::InitGameSystem(HWND hwndForInputSystem)
{
    bool bShaderCacheGen = m_bPrecacheShaderList | m_bPrecacheShaders | m_bPrecacheShadersLevels;

    CGameEngine* pGameEngine = new CGameEngine;

    AZ::Outcome<void, AZStd::string> initOutcome = pGameEngine->Init(m_bPreviewMode, m_bTestMode, bShaderCacheGen, qApp->arguments().join(" ").toUtf8().data(), g_pInitializeUIInfo, hwndForInputSystem);
    if (!initOutcome.IsSuccess())
    {
        return initOutcome;
    }

    AZ_Assert(pGameEngine, "Game engine initialization failed, but initOutcome returned success.");

    m_pEditor->SetGameEngine(pGameEngine);

    // needs to be called after CrySystem has been loaded.
    gSettings.LoadDefaultGamePaths();

    return AZ::Success();
}

/////////////////////////////////////////////////////////////////////////////
BOOL CCryEditApp::CheckIfAlreadyRunning()
{
    bool bForceNewInstance = false;

    if (!m_bPreviewMode)
    {
        FixDanglingSharedMemory(lumberyardApplicationName);
        m_mutexApplication = new QSharedMemory(lumberyardApplicationName);
        if (!m_mutexApplication->create(16))
        {
            // Don't prompt the user in non-interactive export mode.  Instead, default to allowing multiple instances to 
            // run simultaneously, so that multiple level exports can be run in parallel on the same machine.  
            // NOTE:  If you choose to do this, be sure to export *different* levels, since nothing prevents multiple runs 
            // from trying to write to the same level at the same time.
            // If we're running interactively, let's ask and make sure the user actually intended to do this.
            if (!m_bExportMode && QMessageBox::question(AzToolsFramework::GetActiveWindow(), QObject::tr("Too many apps"), QObject::tr("There is already a Lumberyard application running\nDo you want to start another one?")) != QMessageBox::Yes)
            {
                return false;
            }

            bForceNewInstance = true;
        }
    }

    // Shader pre-caching may start multiple editor copies
    if (!FirstInstance(bForceNewInstance) && !m_bPrecacheShaderList)
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
bool CCryEditApp::InitGame()
{
    if (!m_bPreviewMode && !GetIEditor()->IsInMatEditMode())
    {
        ICVar* pVar = gEnv->pConsole->GetCVar("sys_dll_game");
        const char* sGameDLL = pVar ? pVar->GetString() : nullptr;
        Log((QString("sys_dll_game = ") + (sGameDLL && sGameDLL[0] ? sGameDLL : "<not set>")).toUtf8().data());

        pVar = gEnv->pConsole->GetCVar("sys_game_folder");
        const char* sGameFolder = pVar ? pVar->GetString() : nullptr;
        Log((QString("sys_game_folder = ") + (sGameFolder && sGameFolder[0] ? sGameFolder : "<not set>")).toUtf8().data());

        pVar = gEnv->pConsole->GetCVar("sys_localization_folder");
        const char* sLocalizationFolder = pVar ? pVar->GetString() : nullptr;
        Log((QString("sys_localization_folder = ") + (sLocalizationFolder && sLocalizationFolder[0] ? sLocalizationFolder : "<not set>")).toUtf8().data());

        OutputStartupMessage("Starting Game...");

        if (EditorGameRequestBus::GetTotalNumOfEventHandlers() != 0)
        {
            // Don't specify a game dll. CreateEditorGame will be found on the EditorGameRequestBus.
            sGameDLL = nullptr;
        }

        if (!GetIEditor()->GetGameEngine()->InitGame(sGameDLL))
        {
            return false;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Apply settings post engine initialization.
    GetIEditor()->GetDisplaySettings()->PostInitApply();
    gSettings.PostInitApply();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
void CCryEditApp::InitPlugins()
{
    OutputStartupMessage("Loading Plugins...");
    // Load the plugins
    {
        Command_LoadPlugins();

#if defined(AZ_PLATFORM_WINDOWS)
        C3DConnexionDriver* p3DConnexionDriver = new C3DConnexionDriver;
        GetIEditor()->GetPluginManager()->RegisterPlugin(0, p3DConnexionDriver);
#endif
    }
}

////////////////////////////////////////////////////////////////////////////
// Be careful when calling this function: it should be called after
// everything else has finished initializing, otherwise, certain things
// aren't set up yet. If in doubt, wrap it in a QTimer::singleShot(0ms);
void CCryEditApp::InitLevel(const CEditCommandLineInfo& cmdInfo)
{
    if (m_bPreviewMode)
    {
        GetIEditor()->EnableAcceleratos(false);

        // Load geometry object.
        if (!cmdInfo.m_strFileName.isEmpty())
        {
            LoadFile(cmdInfo.m_strFileName);
        }
    }
    else if (m_bExportMode && !m_exportFile.isEmpty())
    {
        GetIEditor()->SetModifiedFlag(false);
        GetIEditor()->SetModifiedModule(eModifiedNothing);
        auto pDocument = OpenDocumentFile(m_exportFile.toUtf8().constData());
        if (pDocument)
        {
            GetIEditor()->SetModifiedFlag(false);
            GetIEditor()->SetModifiedModule(eModifiedNothing);
            if (cmdInfo.m_bExportAI)
            {
                GetIEditor()->GetGameEngine()->GenerateAiAll();
            }
            ExportLevel(cmdInfo.m_bExport, cmdInfo.m_bExportTexture, true);
            // Terminate process.
            CLogFile::WriteLine("Editor: Terminate Process after export");
        }
        // the call to quit() must be posted to the event queue because the app is currently not yet running.
        // if we were to call quit() right now directly, the app would ignore it.
        QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
        return;
    }
    else if ((cmdInfo.m_strFileName.endsWith(defaultFileExtension, Qt::CaseInsensitive)) || (cmdInfo.m_strFileName.endsWith(oldFileExtension, Qt::CaseInsensitive)))
    {
        auto pDocument = OpenDocumentFile(cmdInfo.m_strFileName.toUtf8().constData());
        if (pDocument)
        {
            GetIEditor()->SetModifiedFlag(false);
            GetIEditor()->SetModifiedModule(eModifiedNothing);
        }
    }
    else
    {
        //////////////////////////////////////////////////////////////////////////
        //It can happen that if you are switching between projects and you have auto load set that
        //you could inadvertently load the wrong project and not know it, you would think you are editing
        //one level when in fact you are editing the old one. This can happen if both projects have the same
        //relative path... which is often the case when branching.
        // Ex. D:\cryengine\dev\ gets branched to D:\cryengine\branch\dev
        // Now you have gamesdk in both roots and therefore GameSDK\Levels\Singleplayer\Forest in both
        // If you execute the branch the m_pRecentFileList will be an absolute path to the old gamesdk,
        // then if auto load is set simply takes the old level and loads it in the new branch...
        //I would question ever trying to load a level not in our gamesdk, what happens when there are things that
        //an not exist in the level when built in a different gamesdk.. does it erase them, most likely, then you
        //just screwed up the level for everyone in the other gamesdk...
        //So if we are auto loading a level outside our current gamesdk we should act as though the flag
        //was unset and pop the dialog which should be in the correct location. This is not fool proof, but at
        //least this its a compromise that doesn't automatically do something you probably shouldn't.
        bool autoloadLastLevel = gSettings.bAutoloadLastLevelAtStartup;
        if (autoloadLastLevel
            && GetRecentFileList()
            && GetRecentFileList()->GetSize())
        {
            QString gamePath = Path::GetEditingGameDataFolder().c_str();
            Path::ConvertSlashToBackSlash(gamePath);
            gamePath = Path::ToUnixPath(gamePath.toLower());
            gamePath = Path::AddSlash(gamePath);

            QString fullPath = GetRecentFileList()->m_arrNames[0];
            Path::ConvertSlashToBackSlash(fullPath);
            fullPath = Path::ToUnixPath(fullPath.toLower());
            fullPath = Path::AddSlash(fullPath);

            if (fullPath.indexOf(gamePath, 0) != 0)
            {
                autoloadLastLevel = false;
            }
        }
        //////////////////////////////////////////////////////////////////////////

        QString levelName;
        bool isLevelNameValid = false;
        bool doLevelNeedLoading = true;
        const bool runningPythonScript = cmdInfo.m_bRunPythonScript || cmdInfo.m_bRunPythonTestScript;

        AZ::EBusLogicalResult<bool, AZStd::logical_or<bool> > skipStartupUIProcess(false);
        EBUS_EVENT_RESULT(skipStartupUIProcess, AzToolsFramework::EditorEvents::Bus, SkipEditorStartupUI);

        if (!skipStartupUIProcess.value)
        {
            do
            {
                isLevelNameValid = false;
                doLevelNeedLoading = true;
                if (gSettings.bShowDashboardAtStartup
                    && !runningPythonScript
                    && !GetIEditor()->IsInMatEditMode()
                    && !m_bConsoleMode
                    && !m_bSkipWelcomeScreenDialog
                    && !m_bPreviewMode
                    && !autoloadLastLevel)
                {
                    WelcomeScreenDialog wsDlg(MainWindow::instance());
                    wsDlg.SetRecentFileList(GetRecentFileList());
                    wsDlg.exec();
                    levelName = wsDlg.GetLevelPath();
                }
                else if (autoloadLastLevel
                         && GetRecentFileList()
                         && GetRecentFileList()->GetSize())
                {
                    levelName = GetRecentFileList()->m_arrNames[0];
                }

                if (levelName.isEmpty())
                {
                    break;
                }
                if (levelName == "new")
                {
                    //implies that the user has clicked the create new level option
                    bool wasCreateLevelOperationCancelled = false;
                    bool isNewLevelCreationSuccess = false;
                    // This will show the new level dialog until a valid input has been entered by the user or until the user click cancel
                    while (!isNewLevelCreationSuccess && !wasCreateLevelOperationCancelled)
                    {
                        isNewLevelCreationSuccess = CreateLevel(wasCreateLevelOperationCancelled);
                        if (isNewLevelCreationSuccess == true)
                        {
                            doLevelNeedLoading = false;
                            isLevelNameValid = true;
                        }
                    }
                    ;
                }
                else if (levelName == "new slice")
                {
                    QMessageBox::warning(AzToolsFramework::GetActiveWindow(), "Not implemented", "New Slice is not yet implemented.");
                }
                else
                {
                    //implies that the user wants to open an existing level
                    doLevelNeedLoading = true;
                    isLevelNameValid = true;
                }
            } while (!isLevelNameValid);// if we reach here and levelName is not valid ,it implies that the user has clicked cancel on the create new level dialog

            // load level
            if (doLevelNeedLoading && !levelName.isEmpty())
            {
                if (!CFileUtil::Exists(levelName, false))
                {
                    QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QObject::tr("Missing level"), QObject::tr("Failed to auto-load last opened level. Level file does not exist:\n\n%1").arg(levelName));
                    return;
                }

                QString str;
                str = tr("Loading level %1 ...").arg(levelName);
                OutputStartupMessage(str);

                OpenDocumentFile(levelName.toUtf8().data());
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
BOOL CCryEditApp::InitConsole()
{
    if (m_bPrecacheShaderList)
    {
        GetIEditor()->GetSystem()->GetIConsole()->ExecuteString("r_PrecacheShaderList");
        return false;
    }
    else if (m_bStatsShaderList)
    {
        GetIEditor()->GetSystem()->GetIConsole()->ExecuteString("r_StatsShaderList");
        return false;
    }
    else if (m_bStatsShaders)
    {
        GetIEditor()->GetSystem()->GetIConsole()->ExecuteString("r_StatsShaders");
        return false;
    }
    else if (m_bPrecacheShaders)
    {
        GetIEditor()->GetSystem()->GetIConsole()->ExecuteString("r_PrecacheShaders");
        return false;
    }
    else if (m_bPrecacheShadersLevels)
    {
        GetIEditor()->GetSystem()->GetIConsole()->ExecuteString("r_PrecacheShadersLevels");
        return false;
    }
    else if (m_bMergeShaders)
    {
        GetIEditor()->GetSystem()->GetIConsole()->ExecuteString("r_MergeShaders");
        return false;
    }

    // Execute command from cmdline -exec_line if applicable
    if (!m_execLineCmd.isEmpty())
    {
        gEnv->pConsole->ExecuteString(QString("%1").arg(m_execLineCmd).toLocal8Bit());
    }

    // Execute cfg from cmdline -exec if applicable
    if (!m_execFile.isEmpty())
    {
        gEnv->pConsole->ExecuteString(QString("exec %1").arg(m_execFile).toLocal8Bit());
    }

    // Execute special configs.
    gEnv->pConsole->ExecuteString("exec editor_autoexec.cfg");
    gEnv->pConsole->ExecuteString("exec editor.cfg");
    gEnv->pConsole->ExecuteString("exec user.cfg");

    // should be after init game (should be executed even if there is no game)
    //if (!GetIEditor()->GetSystem()->GetIGame())
    {
        GetISystem()->ExecuteCommandLine();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////

//! This handles the normal logging of Python output in the Editor by outputting
//! the data to both the Editor Console and the Editor.log file
struct CCryEditApp::PythonOutputHandler
    : public AzToolsFramework::EditorPythonConsoleNotificationBus::Handler
{
    PythonOutputHandler()
    {
        AzToolsFramework::EditorPythonConsoleNotificationBus::Handler::BusConnect();
    }

    virtual ~PythonOutputHandler()
    {
        AzToolsFramework::EditorPythonConsoleNotificationBus::Handler::BusDisconnect();
    }

    int GetOrder() override
    {
        return 0;
    }

    void OnTraceMessage(AZStd::string_view message) override
    {
        AZ_TracePrintf("python_test", "%.*s", static_cast<int>(message.size()), message.data());
    }

    void OnErrorMessage(AZStd::string_view message) override
    {
        AZ_Error("python_test", false, "%.*s", static_cast<int>(message.size()), message.data());
    }

    void OnExceptionMessage(AZStd::string_view message) override
    {
        AZ_Error("python_test", false, "EXCEPTION: %.*s", static_cast<int>(message.size()), message.data());
    }
};

//! Outputs Python test script print() to stdout
//! If an exception happens in a Python test script, the process terminates
struct PythonTestOutputHandler final
    : public CCryEditApp::PythonOutputHandler
{
    PythonTestOutputHandler() = default;
    virtual ~PythonTestOutputHandler() = default;

    void OnTraceMessage(AZStd::string_view message) override
    {
        PythonOutputHandler::OnTraceMessage(message);
        printf("%.*s\n", static_cast<int>(message.size()), message.data());
    }

    void OnErrorMessage(AZStd::string_view message) override
    {
        PythonOutputHandler::OnErrorMessage(message);
        printf("ERROR: %.*s\n", static_cast<int>(message.size()), message.data());
    }

    void OnExceptionMessage(AZStd::string_view message) override
    {
        PythonOutputHandler::OnExceptionMessage(message);
        printf("EXCEPTION: %.*s\n", static_cast<int>(message.size()), message.data());
        AZ::Debug::Trace::Terminate(1);
    }
};

void CCryEditApp::RunInitPythonScript(CEditCommandLineInfo& cmdInfo)
{
    if (cmdInfo.m_bRunPythonTestScript)
    {
        m_pythonOutputHandler = AZStd::make_shared<PythonTestOutputHandler>();
    }
    else
    {
        m_pythonOutputHandler = AZStd::make_shared<PythonOutputHandler>();
    }

    using namespace AzToolsFramework;
    if (cmdInfo.m_bRunPythonScript || cmdInfo.m_bRunPythonTestScript)
    {
        if (cmdInfo.m_pythonArgs.length() > 0 || cmdInfo.m_bRunPythonTestScript)
        {
            AZStd::vector<AZStd::string> tokens;
            AzFramework::StringFunc::Tokenize(cmdInfo.m_pythonArgs.toUtf8().constData(), tokens, ' ');
            AZStd::vector<AZStd::string_view> pythonArgs;
            std::transform(tokens.begin(), tokens.end(), std::back_inserter(pythonArgs), [](auto& tokenData) { return tokenData.c_str(); });
            if (cmdInfo.m_bRunPythonTestScript)
            {
                EditorPythonRunnerRequestBus::Broadcast(&EditorPythonRunnerRequestBus::Events::ExecuteByFilenameAsTest, cmdInfo.m_strFileName.toUtf8().constData(), pythonArgs);
            }
            else
            {
                EditorPythonRunnerRequestBus::Broadcast(&EditorPythonRunnerRequestBus::Events::ExecuteByFilenameWithArgs, cmdInfo.m_strFileName.toUtf8().constData(), pythonArgs);
            }
        }
        else
        {
            EditorPythonRunnerRequestBus::Broadcast(&EditorPythonRunnerRequestBus::Events::ExecuteByFilename, cmdInfo.m_strFileName.toUtf8().constData());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// CCryEditApp initialization
BOOL CCryEditApp::InitInstance()
{
    QElapsedTimer startupTimer;
    startupTimer.start();
    InitDirectory();

    // create / attach to the environment:
    AttachEditorCoreAZEnvironment(AZ::Environment::GetInstance());
    InitializeEditorUIQTISystem(AZ::Environment::GetInstance());
    m_pEditor = new CEditorImpl();

    // parameters must be parsed early to capture arguments for test bootstrap
    CEditCommandLineInfo cmdInfo;

    if (IsProjectConfiguratorRunning())
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QObject::tr("Project Configurator is open"),
            QObject::tr("Please close Project Configurator before running the Editor."));
        return false;
    }

    InitFromCommandLine(cmdInfo);

    InitDirectory();

    qobject_cast<Editor::EditorQtApplication*>(qApp)->Initialize(); // Must be done after CEditorImpl() is created
    m_pEditor->Initialize();

    // let anything listening know that they can use the IEditor now
    AzToolsFramework::EditorEvents::Bus::Broadcast(&AzToolsFramework::EditorEvents::NotifyIEditorAvailable, m_pEditor);

#if AZ_TESTS_ENABLED
    // bootstrap the editor to run tests
    if (cmdInfo.m_bBootstrapPluginTests)
    {
        m_bootstrapTestInfo.exitCode = RunPluginUnitTests(cmdInfo);
        m_bootstrapTestInfo.didRun = true;
        return FALSE;
    }
#endif

    if (cmdInfo.m_bShowVersionInfo)
    {
        CAboutDialog aboutDlg(FormatVersion(m_pEditor->GetFileVersion()), FormatRichTextCopyrightNotice());
        aboutDlg.exec();
        return FALSE;
    }

    // check for Lumberyard Setup Assistant's user preference ini file (indicating that you have ran it at least once)
    {
        const char* engineRootPath = nullptr;
        EBUS_EVENT_RESULT(engineRootPath, AzToolsFramework::ToolsApplicationRequestBus, GetEngineRootPath);
        m_rootEnginePath = QString(engineRootPath);

        QDir engineRootDirectory(m_rootEnginePath);

        QString userPreferenceFileName = "SetupAssistantUserPreferences.ini";
        QString userPreferenceFilePath = engineRootDirectory.absoluteFilePath(userPreferenceFileName);
        QFile userPreferenceFile(userPreferenceFilePath);
        if (!userPreferenceFile.exists())
        {
            OpenSetupAssistant();
            return false;
        }
    }


    // Reflect property control classes to the serialize context...
    AZ::SerializeContext* serializeContext = nullptr;
    AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
    AZ_Assert(serializeContext, "Serialization context not available");
    ReflectedVarInit::setupReflection(serializeContext);
    RegisterReflectedVarHandlers();


    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

    CreateSplashScreen();

    // Register the application's document templates. Document templates
    // serve as the connection between documents, frame windows and views
    CCrySingleDocTemplate* pDocTemplate = CCrySingleDocTemplate::create<CCryEditDoc>();

    m_pDocManager = new CCryDocManager;
    ((CCryDocManager*)m_pDocManager)->SetDefaultTemplate(pDocTemplate);

    auto mainWindow = new MainWindow();
#ifdef Q_OS_MACOS
    auto mainWindowWrapper = new AzQtComponents::WindowDecorationWrapper(AzQtComponents::WindowDecorationWrapper::OptionDisabled);
#else
    // No need for mainwindow wrapper for MatEdit mode
    auto mainWindowWrapper = new AzQtComponents::WindowDecorationWrapper(cmdInfo.m_bMatEditMode ? AzQtComponents::WindowDecorationWrapper::OptionDisabled
        : AzQtComponents::WindowDecorationWrapper::OptionAutoTitleBarButtons);
#endif
    mainWindowWrapper->setGuest(mainWindow);
    HWND mainWindowWrapperHwnd = (HWND)mainWindowWrapper->winId();

    QDir engineRoot = AzQtComponents::FindEngineRootDir(qApp);
    AzQtComponents::StyleManager::addSearchPaths(
        QStringLiteral("style"),
        engineRoot.filePath(QStringLiteral("Code/Sandbox/Editor/Style")),
        QStringLiteral(":/Editor/Style"));
    AzQtComponents::StyleManager::setStyleSheet(mainWindow, QStringLiteral("style:Editor.qss"));

    // Note: we should use getNativeHandle to get the HWND from the widget, but
    // it returns an invalid handle unless the widget has been shown and polished and even then
    // it sometimes returns an invalid handle.
    // So instead, we use winId(), which does consistently work
    //mainWindowWrapperHwnd = QtUtil::getNativeHandle(mainWindowWrapper);

    auto initGameSystemOutcome = InitGameSystem(mainWindowWrapperHwnd);
    if (!initGameSystemOutcome.IsSuccess())
    {
        return false;
    }

    // Init Lyzard
    Engines::EngineManagerRequestBus::Broadcast(&Engines::EngineManagerRequests::LoadEngines);

    qobject_cast<Editor::EditorQtApplication*>(qApp)->LoadSettings();

    // Create Sandbox user folder if necessary
    AZ::IO::FileIOBase::GetDirectInstance()->CreatePath(Path::GetUserSandboxFolder().toUtf8().data());

    if (!InitGame())
    {
        if (gEnv && gEnv->pLog)
        {
            gEnv->pLog->LogError("Game can not be initialized, InitGame() failed.");
        }
        if (!cmdInfo.m_bExport)
        {
            QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Game can not be initialized, please refer to the editor log file"));
        }
        return false;
    }

    // Meant to be called before MainWindow::Initialize
    InitPlugins();

    mainWindow->Initialize();

    m_pEditor->InitializeCrashLog();
    // Legacy - hide the "Actor Entity" menu until the code is removed.
    // we do this by unregistering it (it is registered very early on before CVars are loaded from disk).
    auto showActorEntity = gEnv->pConsole->GetCVar("ed_showActorEntity");
    if (showActorEntity == nullptr || showActorEntity->GetIVal() == 0)
    {
        CClassFactory* cf = CClassFactory::Instance();
        GUID actorEntityGUID =
        {
            0x5529607a, 0xc374, 0x4588,{ 0xad, 0x41, 0xd6, 0x8f, 0xb2, 0x6a, 0x1e, 0x8c }
        };
        cf->UnregisterClass(actorEntityGUID);
    }

#if !defined(AZ_PLATFORM_LINUX)
    //Show login screen when not in batch mode OR in auto test mode
    if(!m_bConsoleMode && !IsInAutotestMode())
    {
        //Wrap the existing splash with a QT widget so our login screen has a parent
        AWSNativeSDKInit::InitializationManager::InitAwsApi();
        Amazon::LoginManager loginManager(*g_splashScreen);
        auto loggedInUser = loginManager.GetLoggedInUser();
        if (!loggedInUser)
        {
            ExitInstance();
        }
    }
#else
    AZ_Assert(false,"Amazon Login Manager not supported on Linux")
#endif // !defined(AZ_PLATFORM_LINUX)

    GetIEditor()->GetCommandManager()->RegisterAutoCommands();
    GetIEditor()->AddUIEnums();

    m_pChangeMonitor = new CMannequinChangeMonitor();

    mainWindowWrapper->enableSaveRestoreGeometry("amazon", "lumberyard", "mainWindowGeometry");
    m_pDocManager->OnFileNew();

    if (IsInRegularEditorMode())
    {
        CIndexedFiles::Create();

        if (gEnv->pConsole->GetCVar("ed_indexfiles")->GetIVal())
        {
            Log("Started game resource files indexing...");
            CIndexedFiles::StartFileIndexing();
        }
        else
        {
            Log("Game resource files indexing is disabled.");
        }

        // QuickAccessBar creation should be before m_pMainWnd->SetFocus(),
        // since it receives the focus at creation time. It brakes MainFrame key accelerators.
        m_pQuickAccessBar = new CQuickAccessBar;
        m_pQuickAccessBar->setVisible(false);
    }

    if (MainWindow::instance())
    {
        if (m_bConsoleMode || IsInAutotestMode())
        {
            AZ::Environment::FindVariable<int>("assertVerbosityLevel").Set(1);
            m_pConsoleDialog->raise();
        }
        else if (!GetIEditor()->IsInMatEditMode())
        {
            MainWindow::instance()->show();
            MainWindow::instance()->raise();
            MainWindow::instance()->update();
            MainWindow::instance()->setFocus();

#if AZ_TRAIT_OS_PLATFORM_APPLE
            QWindow* window = mainWindowWrapper->windowHandle();
            if (window)
            {
                Editor::WindowObserver* observer = new Editor::WindowObserver(window, this);
                connect(observer, &Editor::WindowObserver::windowIsMovingOrResizingChanged, Editor::EditorQtApplication::instance(), &Editor::EditorQtApplication::setIsMovingOrResizing);
            }
#endif
        }
    }

    if (m_bAutotestMode)
    {
        ICVar* const noErrorReportWindowCVar = gEnv && gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_no_error_report_window") : nullptr;
        if (noErrorReportWindowCVar)
        {
            noErrorReportWindowCVar->Set(true);
        }
        ICVar* const showErrorDialogOnLoadCVar = gEnv && gEnv->pConsole ? gEnv->pConsole->GetCVar("ed_showErrorDialogOnLoad") : nullptr;
        if (showErrorDialogOnLoadCVar)
        {
            showErrorDialogOnLoadCVar->Set(false);
        }
    }

    SetEditorWindowTitle();
    if (!GetIEditor()->IsInMatEditMode())
    {
        m_pEditor->InitFinished();
    }

    // Make sure Python is started before we attempt to restore the Editor layout, since the user
    // might have custom view panes in the saved layout that will need to be registered.
    auto editorPythonEventsInterface = AZ::Interface<AzToolsFramework::EditorPythonEventsInterface>::Get();
    if (editorPythonEventsInterface)
    {
        editorPythonEventsInterface->StartPython();
    }

    if (!GetIEditor()->IsInMatEditMode() && !GetIEditor()->IsInConsolewMode())
    {
        bool restoreDefaults = !mainWindowWrapper->restoreGeometryFromSettings();
        QtViewPaneManager::instance()->RestoreLayout(restoreDefaults);
    }

    CloseSplashScreen();

    // Register callback with indexed files such that when it gets updated in the case that the file indexer is finished before initial database creation
    if (IsInRegularEditorMode())
    {
        connect(this, &CCryEditApp::RefreshAssetDatabases, this, &CCryEditApp::OnRefreshAssetDatabases);
        CIndexedFiles::RegisterCallback([&](void)
            {
                emit RefreshAssetDatabases();
            });

        CAssetBrowserManager::Instance();
    }

    // let editor game know editor is done initializing
    if (GetIEditor()->GetGameEngine()->GetIEditorGame())
    {
        GetIEditor()->GetGameEngine()->GetIEditorGame()->PostInit();
    }

    // DON'T CHANGE ME!
    // Test scripts listen for this line, so please don't touch this without updating them.
    // We consider ourselves "initialized enough" at this stage because all further initialization may be blocked by the modal welcome screen.
    CLogFile::WriteLine(QString("Engine initialized, took %1s.").arg(startupTimer.elapsed() / 1000.0, 0, 'f', 2));

    // Init the level after everything else is finished initializing, otherwise, certain things aren't set up yet
    QTimer::singleShot(0, this, [this, cmdInfo] {
        InitLevel(cmdInfo);
    });

#ifdef USE_WIP_FEATURES_MANAGER
    // load the WIP features file
    CWipFeatureManager::Instance()->EnableManager(!cmdInfo.m_bDeveloperMode);
    CWipFeatureManager::Init();
#endif

    if (GetIEditor()->IsInMatEditMode())
    {
        m_pMatEditDlg = new CMatEditMainDlg(QStringLiteral("Material Editor"));
        m_pEditor->InitFinished();
        m_pMatEditDlg->show();
        return true;
    }

    if (!m_bConsoleMode && !m_bPreviewMode)
    {
        GetIEditor()->UpdateViews();
        if (MainWindow::instance())
        {
            MainWindow::instance()->setFocus();
        }
    }

    if (!InitConsole())
    {
        return true;
    }

    if (IsInRegularEditorMode())
    {
        int startUpMacroIndex = GetIEditor()->GetToolBoxManager()->GetMacroIndex("startup", true);
        if (startUpMacroIndex >= 0)
        {
            CryLogAlways("Executing the startup macro");
            GetIEditor()->GetToolBoxManager()->ExecuteMacro(startUpMacroIndex, true);
        }
    }

    if (GetIEditor()->GetCommandManager()->IsRegistered("editor.open_lnm_editor"))
    {
        CCommand0::SUIInfo uiInfo;
        bool ok = GetIEditor()->GetCommandManager()->GetUIInfo("editor.open_lnm_editor", uiInfo);
        assert(ok);
        int ID_VIEW_AI_LNMEDITOR(uiInfo.commandId);
    }

    RunInitPythonScript(cmdInfo);

    return true;
}

void CCryEditApp::RegisterEventLoopHook(IEventLoopHook* pHook)
{
    pHook->pNextHook = m_pEventLoopHook;
    m_pEventLoopHook = pHook;
}

void CCryEditApp::UnregisterEventLoopHook(IEventLoopHook* pHookToRemove)
{
    IEventLoopHook* pPrevious = 0;
    for (IEventLoopHook* pHook = m_pEventLoopHook; pHook != 0; pHook = pHook->pNextHook)
    {
        if (pHook == pHookToRemove)
        {
            if (pPrevious)
            {
                pPrevious->pNextHook = pHookToRemove->pNextHook;
            }
            else
            {
                m_pEventLoopHook = pHookToRemove->pNextHook;
            }

            pHookToRemove->pNextHook = 0;
            return;
        }
    }
}

#if AZ_TESTS_ENABLED
#if defined(AZ_PLATFORM_WINDOWS)
class MainArgs
{
public:
    MainArgs(std::vector<std::string> source)
        : m_narg(0)
        , m_args(nullptr)
        , m_sourceArgs(source)
    {
        m_narg = source.size();
        m_args = new char*[m_narg];
        for (int i = 0; i < m_narg; i++)
        {
            const char* arg = m_sourceArgs[i].c_str();
            m_args[i] = const_cast<char*>(arg);
        }
    }

    ~MainArgs()
    {
        delete[] m_args;
    }

    char** GetArgs() { return m_args; }
    int GetNarg() const { return m_narg; }

private:
    char** m_args;

    int m_narg;

    std::vector<std::string> m_sourceArgs;
};

int CCryEditApp::RunPluginUnitTests(CEditCommandLineInfo& cmdLine)
{
    using AzRunUnitTestsFn = int(int, char**);

    const QString& pluginPath = cmdLine.m_strFileName; // plugin to test

    // match AzTestRunner/Scanner exit codes
    enum ExitCodes
    {
        Success = 0,
        IncorrectUsage = 101,
        LibNotFound = 102,
        SymbolNotFound = 103
    };

    int exitCode = ExitCodes::Success;

    const std::wstring strPluginPath = pluginPath.toStdWString();
    HMODULE hPlugin = ::LoadLibraryExW(strPluginPath.c_str(), 0, 0);
    if (NULL != hPlugin)
    {
        // call test hook
        auto fn = (AzRunUnitTestsFn*) ::GetProcAddress(hPlugin, "AzRunUnitTests");
        if (fn)
        {
            // argv[0] is expected by the test hook to be a path to the current application.
            // If not present, the first argument will be ignored.
            cmdLine.m_testArgs.insert(cmdLine.m_testArgs.begin(), std::string("dummy_path_to_executable"));

            MainArgs args(cmdLine.m_testArgs);
            exitCode = fn(args.GetNarg(), args.GetArgs());
        }
        else
        {
            exitCode = ExitCodes::SymbolNotFound;
        }

        ::FreeLibrary(hPlugin);
    }
    else
    {
        exitCode = ExitCodes::LibNotFound;
    }

    return exitCode;
}
#else
int CCryEditApp::RunPluginUnitTests(CEditCommandLineInfo& cmdLine)
{
    return 0;
}
#endif
#endif

bool CCryEditApp::CanAddLegacyTerrainLevelComponent()
{
#ifdef LY_TERRAIN_EDITOR
    AZStd::vector<AZStd::string> componentNames = { "Legacy Terrain", };
    AZStd::vector<AZ::Uuid> uuids;
    AzToolsFramework::EditorComponentAPIBus::BroadcastResult(uuids, &AzToolsFramework::EditorComponentAPIRequests::FindComponentTypeIdsByEntityType, componentNames, AzToolsFramework::EditorComponentAPIRequests::EntityType::Level);
    AZ_Assert(uuids.size() == 1, "FindComponentTypeIdsByEntityType failed to return the correct number of results");
    if (!uuids[0].IsNull())
    {
        return true;
    }
#endif
    return false;
}

bool CCryEditApp::AddLegacyTerrainLevelComponent()
{
#ifdef LY_TERRAIN_EDITOR
    AZStd::vector<AZStd::string> componentNames = { "Legacy Terrain", };
    AZStd::vector<AZ::Uuid> uuids;
    AzToolsFramework::EditorComponentAPIBus::BroadcastResult(uuids, &AzToolsFramework::EditorComponentAPIRequests::FindComponentTypeIdsByEntityType, componentNames, AzToolsFramework::EditorComponentAPIRequests::EntityType::Level);
    AZ_Assert(uuids.size() == 1, "FindComponentTypeIdsByEntityType failed to return the correct number of results");
    if (!uuids[0].IsNull())
    {
        AzToolsFramework::EditorComponentAPIRequests::AddComponentsOutcome outcome;
        AzToolsFramework::EditorLevelComponentAPIBus::BroadcastResult(outcome, &AzToolsFramework::EditorLevelComponentAPIRequests::AddComponentsOfType, uuids);
        if (!outcome.IsSuccess())
        {
            AZ_Error("LegacyTerrain", false, "Failed to add the <%s> component to the level entity", componentNames[0].c_str());
        }
        else
        {
            AZ_Printf("LegacyTerrain", "Successfully added the <%s> component to the level entity", componentNames[0].c_str())
            return true;
        }
    }
#endif
    return false;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::LoadFile(QString fileName)
{
    //CEditCommandLineInfo cmdLine;
    //ProcessCommandLine(cmdinfo);

    //bool bBuilding = false;
    //CString file = cmdLine.SpanExcluding()
    if (GetIEditor()->GetViewManager()->GetViewCount() == 0)
    {
        return;
    }
    CViewport* vp = GetIEditor()->GetViewManager()->GetView(0);
    if (CModelViewport* mvp = viewport_cast<CModelViewport*>(vp))
    {
        mvp->LoadObject(fileName, 1);
    }

    LoadTagLocations();

    if (MainWindow::instance() || m_pConsoleDialog)
    {
        SetEditorWindowTitle(0, 0, GetIEditor()->GetGameEngine()->GetLevelName());
    }

    GetIEditor()->SetModifiedFlag(false);
    GetIEditor()->SetModifiedModule(eModifiedNothing);
}

//////////////////////////////////////////////////////////////////////////
inline void ExtractMenuName(QString& str)
{
    // eliminate &
    int pos = str.indexOf('&');
    if (pos >= 0)
    {
        str = str.left(pos) + str.right(str.length() - pos - 1);
    }
    // cut the string
    for (int i = 0; i < str.length(); i++)
    {
        if (str[i] == 9)
        {
            str = str.left(i);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::EnableAccelerator(bool bEnable)
{
    /*
    if (bEnable)
    {
        //LoadAccelTable( MAKEINTRESOURCE(IDR_MAINFRAME) );
        m_AccelManager.UpdateWndTable();
        CLogFile::WriteLine( "Enable Accelerators" );
    }
    else
    {
        CMainFrame *mainFrame = (CMainFrame*)m_pMainWnd;
        if (mainFrame->m_hAccelTable)
            DestroyAcceleratorTable( mainFrame->m_hAccelTable );
        mainFrame->m_hAccelTable = NULL;
        mainFrame->LoadAccelTable( MAKEINTRESOURCE(IDR_GAMEACCELERATOR) );
        CLogFile::WriteLine( "Disable Accelerators" );
    }
    */
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::SaveAutoRemind()
{
    // Added a static variable here to avoid multiple messageboxes to
    // remind the user of saving the file. Many message boxes would appear as this
    // is triggered by a timer even which does not stop when the message box is called.
    // Used a static variable instead of a member variable because this value is not
    // Needed anywhere else.
    static bool boIsShowingWarning(false);

    // Ingore in game mode, or if no level created, or level not modified
    if (GetIEditor()->IsInGameMode() || !GetIEditor()->GetGameEngine()->IsLevelLoaded() || !GetIEditor()->GetDocument()->IsModified())
    {
        return;
    }

    if (boIsShowingWarning)
    {
        return;
    }

    boIsShowingWarning = true;
    if (QMessageBox::question(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Auto Reminder: You did not save level for at least %1 minute(s)\r\nDo you want to save it now?").arg(gSettings.autoRemindTime), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        // Save now.
        GetIEditor()->SaveDocument();
    }
    boIsShowingWarning = false;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::WriteConfig()
{
    IEditor* pEditor = GetIEditor();
    if (pEditor && pEditor->GetDisplaySettings())
    {
        pEditor->GetDisplaySettings()->SaveRegistry();
    }
}

// App command to run the dialog
void CCryEditApp::OnAppAbout()
{
    CAboutDialog aboutDlg(FormatVersion(m_pEditor->GetFileVersion()), FormatRichTextCopyrightNotice());
    aboutDlg.exec();
}

// App command to open online documentation page
void CCryEditApp::OnDocumentationGettingStartedGuide()
{
    QString webLink = tr("https://docs.aws.amazon.com/console/lumberyard/welcome");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationTutorials()
{
    QString webLink = tr("https://docs.aws.amazon.com/console/lumberyard/tutorials/channel");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationGlossary()
{
    QString webLink = tr("https://docs.aws.amazon.com/console/lumberyard/glossary");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationLumberyard()
{
    QString webLink = tr("https://docs.aws.amazon.com/console/lumberyard/userguide");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationGamelift()
{
    QString webLink = tr("https://docs.aws.amazon.com/gamelift/developerguide");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationReleaseNotes()
{
    QString webLink = tr("https://docs.aws.amazon.com/console/lumberyard/releasenotes");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationGameDevBlog()
{
    QString webLink = tr("https://aws.amazon.com/blogs/gamedev");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationTwitchChannel()
{
    QString webLink = tr("http://twitch.tv/amazongamedev");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationForums()
{
    QString webLink = tr("https://gamedev.amazon.com/forums");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationAWSSupport()
{
    QString webLink = tr("https://aws.amazon.com/contact-us");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnDocumentationFeedback()
{
    FeedbackDialog dialog;
    dialog.exec();
}

bool CCryEditApp::HasAWSCredentials()
{
    CloudCanvas::AWSClientCredentials credsProvider;
    EBUS_EVENT_RESULT(credsProvider, CloudCanvas::CloudCanvasEditorRequestBus, GetCredentials);

    return credsProvider && !credsProvider->GetAWSCredentials().GetAWSAccessKeyId().empty();
}

void CCryEditApp::OnAWSCredentialEditor()
{
    GetIEditor()->OpenWinWidget(WinWidgetId::PROFILE_SELECTOR);
}

void CCryEditApp::OnAWSActiveDeployment()
{
    GetIEditor()->OpenWinWidget(WinWidgetId::ACTIVE_DEPLOYMENT);
}

void CCryEditApp::OnAWSResourceManagement()
{
    QtViewPaneManager::instance()->OpenPane("Cloud Canvas Resource Manager");
}

void CCryEditApp::OnAWSGameliftLearn()
{
    QString webLink = tr("https://aws.amazon.com/gamelift/");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnAWSGameliftConsole()
{
    QString webLink = tr("https://console.aws.amazon.com/gamelift/home");
    OnAWSLaunchConsolePage(webLink);
}

void CCryEditApp::OnAWSGameliftGetStarted()
{
    QString webLink = tr("https://www.youtube.com/amazonlumberyardtutorials");
    QDesktopServices::openUrl(QUrl(webLink));
}

void CCryEditApp::OnAWSGameliftTrialWizard()
{
    QString webLink = tr("https://us-west-2.console.aws.amazon.com/gamelift/home?#/r/fleets/sample");
    OnAWSLaunchConsolePage(webLink);
}

// App command to open Amazon Device Farm Console page
void CCryEditApp::OnAWSDeviceFarmConsole()
{
    QString webLink = tr("https://console.aws.amazon.com/devicefarm/home");
    OnAWSLaunchConsolePage(webLink);
}

// App command to open Amazon Cognito Console page
void CCryEditApp::OnAWSCognitoConsole()
{
    QString webLink = tr("https://console.aws.amazon.com/cognito/home");
    OnAWSLaunchConsolePage(webLink);
}

// App command to open Amazon DynamoDB Console page
void CCryEditApp::OnAWSDynamoDBConsole()
{
    QString webLink = tr("https://console.aws.amazon.com/dynamodb/home");
    OnAWSLaunchConsolePage(webLink);
}

// App command to open Amazon S3 Console page
void CCryEditApp::OnAWSS3Console()
{
    QString webLink = tr("https://console.aws.amazon.com/s3/home");
    OnAWSLaunchConsolePage(webLink);
}

// App command to open Amazon Lambda Console page
void CCryEditApp::OnAWSLambdaConsole()
{
    QString webLink = tr("https://console.aws.amazon.com/lambda/home");
    OnAWSLaunchConsolePage(webLink);
}

// App command to open AWS console page
void CCryEditApp::OnAWSLaunch()
{
    QString webLink = tr("http://console.aws.amazon.com/");
    OnAWSLaunchConsolePage(webLink);
}

// Open a given AWS console page using credentials if available, othwerise go to signup page
void CCryEditApp::OnAWSLaunchConsolePage(const QString& webLink)
{
    if (HasAWSCredentials())
    {
        OpenAWSConsoleFederated(webLink);
    }
    else
    {
        QString signupLink = tr("http://console.aws.amazon.com/");
        QDesktopServices::openUrl(QString(signupLink));
    }
}

void CCryEditApp::OnAWSLaunchUpdate(QAction* action)
{
}

void CCryEditApp::OnHowToSetUpCloudCanvas()
{
    QString setupLink = tr("https://docs.aws.amazon.com/console/lumberyard/cloudcanvas");
    QDesktopServices::openUrl(QUrl(setupLink));
}

void CCryEditApp::OnLearnAboutGameLift()
{
    QString learnLink = tr("https://aws.amazon.com/gamelift/");
    QDesktopServices::openUrl(QUrl(learnLink));
}

// App command to open the AWS console, federated as the currently logged in user
void CCryEditApp::OpenAWSConsoleFederated(const QString& destUrl)
{
    //Make sure they've got their default profile setup or have selected a profile
    CloudCanvas::AWSClientCredentials credsProvider;
    EBUS_EVENT_RESULT(credsProvider, CloudCanvas::CloudCanvasEditorRequestBus, GetCredentials);
    if (!credsProvider || credsProvider->GetAWSCredentials().GetAWSAccessKeyId().empty())
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QObject::tr("Credentials Error"),
            QObject::tr("Your AWS credentials are not configured correctly. Please configure them using the AWS Credentials manager."));
        return;
    }

    Aws::STS::STSClient stsClient(credsProvider);
    Aws::Client::ClientConfiguration config;
    config.enableTcpKeepAlive = AZ_TRAIT_AZFRAMEWORK_AWS_ENABLE_TCP_KEEP_ALIVE_SUPPORTED;
    std::shared_ptr<Aws::Http::HttpClient> httpClient =  Aws::Http::CreateHttpClient(config);

    //Generate a federation token
    Aws::STS::Model::GetFederationTokenRequest request;
    request.SetName("LumberyardConsoleFederation");
    request.SetPolicy("{\"Version\":\"2012-10-17\",\"Statement\":{\"Effect\":\"Allow\",\"Action\":\"*\",\"Resource\" :\"*\"}}");
    auto response = stsClient.GetFederationToken(request);
    if (response.IsSuccess())
    {
        auto federatedCredentials = response.GetResult().GetCredentials();

        Aws::Utils::Json::JsonValue sessionJson;
        sessionJson.WithString("sessionId", federatedCredentials.GetAccessKeyId().c_str())
            .WithString("sessionKey", federatedCredentials.GetSecretAccessKey().c_str())
            .WithString("sessionToken", federatedCredentials.GetSessionToken().c_str());

        QUrl signinURL;
        signinURL.setUrl("https://signin.aws.amazon.com:443/federation");

        QUrlQuery signinQuery;
        auto session = sessionJson.View().WriteCompact();
        QString qSession(session.c_str());

        signinQuery.addQueryItem("Action", "getSigninToken");
        signinQuery.addQueryItem("Session", QUrl::toPercentEncoding(qSession));
        auto queryString = signinQuery.toString();
        signinURL.setQuery(signinQuery);

        auto cURL = signinURL.toString().toStdString();
        Aws::String encodedGetSigninTokenURL(cURL.c_str());

        //Now we  craft a URL to signin to get a federation URL.
        //Documented here: http://docs.aws.amazon.com/STS/latest/UsingSTS/STSMgmtConsole-manualURL.html
        auto httpRequest(Aws::Http::CreateHttpRequest(encodedGetSigninTokenURL, Aws::Http::HttpMethod::HTTP_GET, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod));
        auto httpResponse(httpClient->MakeRequest(httpRequest, nullptr, nullptr));
        auto& body = httpResponse->GetResponseBody();
        auto jsonBody = Aws::Utils::Json::JsonValue(body);
        auto tokenResponse = jsonBody.View().WriteReadable();
        auto signinToken = jsonBody.View().GetString("SigninToken");

        QUrl federationUrl("https://signin.aws.amazon.com/federation");
        QUrlQuery federationQuery;
        federationQuery.addQueryItem("Action", "login");
        federationQuery.addQueryItem("Issuer", "lumberyard");
        federationQuery.addQueryItem("Destination", QUrl::toPercentEncoding(QString(destUrl)));
        federationQuery.addQueryItem("SigninToken", signinToken.c_str());
        federationUrl.setQuery(federationQuery);
        auto federationStr = federationUrl.toString();
        QDesktopServices::openUrl(QUrl(federationStr));
    }
    else if (response.GetError().GetErrorType() == Aws::STS::STSErrors::ACCESS_DENIED)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QObject::tr("Permission Denied"), QObject::tr("You are not authorized to do this. Ask your AWS administrator to grant your user to sts:GetFederationToken"));
    }
    else if (response.GetError().GetErrorType() == Aws::STS::STSErrors::NETWORK_CONNECTION)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QObject::tr("Permission Denied"), QObject::tr("You do not have an active network connection. Please connect to the internet"));
    }
    else if (response.GetError().GetErrorType() == Aws::STS::STSErrors::INVALID_CLIENT_TOKEN_ID)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QObject::tr("Permission Denied"), QObject::tr("Invalid AWS access key ID"));
    }
    else
    {
        QString outStr = QObject::tr("An AWS Credentials error has occurred: (%1)").arg(response.GetError().GetExceptionName().c_str());
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QObject::tr("Unknown error"), outStr);
    }
}

bool CCryEditApp::FixDanglingSharedMemory(const QString& sharedMemName) const
{
    QSystemSemaphore sem(sharedMemName + "_sem", 1);
    sem.acquire();
    {
        QSharedMemory fix(sharedMemName);
        if (!fix.attach())
        {
            if (fix.error() != QSharedMemory::NotFound)
            {
                sem.release();
                return false;
            }
        }
        // fix.detach() when destructed, taking out any dangling shared memory
        // on unix
    }
    sem.release();
    return true;
}

// App command to open Amazon Publish page
void CCryEditApp::OnCommercePublish()
{
    QString webLink = tr("https://developer.amazon.com/appsandservices/solutions/platforms/mac-pc");
    QDesktopServices::openUrl(QUrl(QString(webLink)));
}

// App command to open Amazon Merch page
void CCryEditApp::OnCommerceMerch()
{
    QString webLink = tr("https://merch.amazon.com/landing");
    QDesktopServices::openUrl(QUrl(QString(webLink)));
}

/////////////////////////////////////////////////////////////////////////////
// CCryEditApp message handlers


int CCryEditApp::ExitInstance(int exitCode)
{
    if (m_pEditor)
    {
        m_pEditor->OnBeginShutdownSequence();
    }
    qobject_cast<Editor::EditorQtApplication*>(qApp)->UnloadSettings();

    #ifdef USE_WIP_FEATURES_MANAGER
    //
    // close wip features manager
    //
    CWipFeatureManager::Shutdown();
    #endif

    if (IsInRegularEditorMode())
    {
        if (GetIEditor())
        {
            int shutDownMacroIndex = GetIEditor()->GetToolBoxManager()->GetMacroIndex("shutdown", true);
            if (shutDownMacroIndex >= 0)
            {
                CryLogAlways("Executing the shutdown macro");
                GetIEditor()->GetToolBoxManager()->ExecuteMacro(shutDownMacroIndex, true);
            }
        }
    }

    SAFE_DELETE(m_pChangeMonitor);

    if (IsInRegularEditorMode())
    {
        CIndexedFiles::AbortFileIndexing();
        CIndexedFiles::Destroy();
    }

    if (GetIEditor() && !GetIEditor()->GetGame() && !GetIEditor()->IsInMatEditMode())
    {
#if AZ_TESTS_ENABLED
        // if bootstrapping tests, return the exit code from the test run
        // instead of the normal editor exit code
        if (m_bootstrapTestInfo.didRun)
        {
            exitCode = m_bootstrapTestInfo.exitCode;
        }
#endif
        //Nobody seems to know in what case that kind of exit can happen so instrumented to see if it happens at all
        if (m_pEditor)
        {
            m_pEditor->OnEarlyExitShutdownSequence();
        }

        // note: the intention here is to quit immediately without processing anything further
        // on linux and mac, _exit has that effect
        // however, on windows, _exit() still invokes CRT functions, unloads, and destructors
        // so on windows, we need to use TerminateProcess
#if defined(AZ_PLATFORM_WINDOWS)
       TerminateProcess(GetCurrentProcess(), exitCode);
#else

        _exit(exitCode);
#endif
    }

    SAFE_DELETE(m_pConsoleDialog);
    SAFE_DELETE(m_pQuickAccessBar);

    if (GetIEditor())
    {
        GetIEditor()->Notify(eNotify_OnQuit);
    }

    if (IsInRegularEditorMode() && CAssetBrowserManager::IsInstanceExist())
    {
        CAssetBrowserManager::Instance()->Shutdown();
    }

    CBaseObject::DeleteUIPanels();
    CEntityObject::DeleteUIPanels();

    // if we're aborting due to an unexpected shutdown then don't call into objects that don't exist yet.
    if ((gEnv) && (gEnv->pSystem) && (gEnv->pSystem->GetILevelSystem()))
    {
        gEnv->pSystem->GetILevelSystem()->UnLoadLevel();
    }

    if (GetIEditor())
    {
        GetIEditor()->GetDocument()->DeleteTemporaryLevel();
    }

    m_bExiting = true;

    HEAP_CHECK
    ////////////////////////////////////////////////////////////////////////
    // Executed directly before termination of the editor, just write a
    // quick note to the log so that we can later see that the editor
    // terminated flawlessly. Also delete temporary files.
    ////////////////////////////////////////////////////////////////////////
        WriteConfig();

    if (m_pEditor)
    {
        // Ensure component entities are wiped prior to unloading plugins,
        // since components may be implemented in those plugins.
        EBUS_EVENT(AzToolsFramework::EditorEntityContextRequestBus, ResetEditorContext);

        // vital, so that the Qt integration can unhook itself!
        m_pEditor->UnloadPlugins(true);
        m_pEditor->Uninitialize();
    }

    //////////////////////////////////////////////////////////////////////////
    // Quick end for editor.
    if (gEnv && gEnv->pSystem)
    {
        gEnv->pSystem->Quit();
        SAFE_RELEASE(gEnv->pSystem);
    }
    //////////////////////////////////////////////////////////////////////////

    if (m_pEditor)
    {
        m_pEditor->DeleteThis();
        m_pEditor = nullptr;
    }

    // save accelerator manager configuration.
    //m_AccelManager.SaveOnExit();

#ifdef WIN32
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
#endif

    if (m_mutexApplication)
    {
        delete m_mutexApplication;
    }

    UninitializeEditorUIQTISystem();
    DetachEditorCoreAZEnvironment();
    return 0;
}

bool CCryEditApp::IsWindowInForeground()
{
    return Editor::EditorQtApplication::instance()->IsActive();
}

void CCryEditApp::DisableIdleProcessing()
{
    m_disableIdleProcessingCounter++;
}

void CCryEditApp::EnableIdleProcessing()
{
    m_disableIdleProcessingCounter--;
    AZ_Assert(m_disableIdleProcessingCounter >= 0, "m_disableIdleProcessingCounter must be nonnegative");
}

BOOL CCryEditApp::OnIdle(LONG lCount)
{
    if (0 == m_disableIdleProcessingCounter)
    {
        return IdleProcessing(false);
    }
    else
    {
        return 0;
    }
}

int CCryEditApp::IdleProcessing(bool bBackgroundUpdate)
{
    AZ_Assert(m_disableIdleProcessingCounter == 0, "We should not be in IdleProcessing()");

    //HEAP_CHECK
    if (!MainWindow::instance())
    {
        return 0;
    }

    if (!GetIEditor()->GetSystem())
    {
        return 0;
    }

    ////////////////////////////////////////////////////////////////////////
    // Call the update function of the engine
    ////////////////////////////////////////////////////////////////////////
    if (m_bTestMode && !bBackgroundUpdate)
    {
        // Terminate process.
        CLogFile::WriteLine("Editor: Terminate Process");
        exit(0);
    }

    bool bIsAppWindow = IsWindowInForeground();
    bool bActive = false;
    int res = 0;
    if (bIsAppWindow || m_bForceProcessIdle || m_bKeepEditorActive)
    {
        res = 1;
        bActive = true;
    }

    if (m_bForceProcessIdle && bIsAppWindow)
    {
        m_bForceProcessIdle = false;
    }

    // focus changed
    if (m_bPrevActive != bActive)
    {
        GetIEditor()->GetSystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, bActive, 0);
    #if defined(AZ_PLATFORM_WINDOWS)
        // This is required for the audio system to be notified of focus changes in the editor.  After discussing it
        // with the macOS team, they are working on unifying the system events between the editor and standalone
        // launcher so this is only needed on windows.
        if (bActive)
        {
            EBUS_EVENT(AzFramework::WindowsLifecycleEvents::Bus, OnSetFocus);
        }
        else
        {
            EBUS_EVENT(AzFramework::WindowsLifecycleEvents::Bus, OnKillFocus);
        }
    #endif
    }

    // process the work schedule - regardless if the app is active or not
    GetIEditor()->GetBackgroundScheduleManager()->Update();

    // if there are active schedules keep updating the application
    if (GetIEditor()->GetBackgroundScheduleManager()->GetNumSchedules() > 0)
    {
        bActive = true;
    }

    m_bPrevActive = bActive;

    AZStd::chrono::system_clock::time_point now = AZStd::chrono::system_clock::now();
    static AZStd::chrono::system_clock::time_point lastUpdate = now;

    AZStd::chrono::duration<float> delta = now - lastUpdate;
    float deltaTime = delta.count();

    lastUpdate = now;

    // Don't tick application if we're doing idle processing during an assert.
    const bool isErrorWindowVisible = (gEnv && gEnv->pSystem->IsAssertDialogVisible());
    if (isErrorWindowVisible)
    {
        if (m_pEditor)
        {
            m_pEditor->Update();
        }
    }
    else if (bActive || (bBackgroundUpdate && !bIsAppWindow))
    {
        if (GetIEditor()->IsInGameMode())
        {
            // Update Game
            GetIEditor()->GetGameEngine()->Update();
        }
        else
        {
            // Start profiling frame.
            GetIEditor()->GetSystem()->GetIProfileSystem()->StartFrame();

            GetIEditor()->GetGameEngine()->Update();

            if (m_pEditor)
            {
                m_pEditor->Update();
            }

            // syncronize all animations so ensure that their compuation have finished
            if (GetIEditor()->GetSystem()->GetIAnimationSystem())
            {
                GetIEditor()->GetSystem()->GetIAnimationSystem()->SyncAllAnimations();
            }

            GetIEditor()->Notify(eNotify_OnIdleUpdate);

            IEditor* pEditor = GetIEditor();
            if (!pEditor->GetGameEngine()->IsLevelLoaded() && pEditor->GetSystem()->NeedDoWorkDuringOcclusionChecks())
            {
                pEditor->GetSystem()->DoWorkDuringOcclusionChecks();
            }

            GetIEditor()->GetSystem()->GetIProfileSystem()->EndFrame();

            // Since the rendering is done based on the eNotify_OnIdleUpdate, we should trigger a TickSystem as well.
            // To ensure that there's a system tick for every render done in Idle
            AZ::ComponentApplication* componentApplication = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(componentApplication, &AZ::ComponentApplicationRequests::GetApplication);
            if (componentApplication)
            {
                componentApplication->TickSystem();
            }
        }
    }
    else if (GetIEditor()->GetSystem() && GetIEditor()->GetSystem()->GetILog())
    {
        GetIEditor()->GetSystem()->GetILog()->Update(); // print messages from other threads
    }

    DisplayLevelLoadErrors();

    if (CConsoleSCB::GetCreatedInstance())
    {
        CConsoleSCB::GetCreatedInstance()->FlushText();
    }

    return res;
}

void CCryEditApp::DisplayLevelLoadErrors()
{
    CCryEditDoc* currentLevel = GetIEditor()->GetDocument();
    if (currentLevel && currentLevel->IsDocumentReady() && !m_levelErrorsHaveBeenDisplayed)
    {
        // Generally it takes a few idle updates for meshes to load and be processed by their components. This value
        // was picked based on examining when mesh components are updated and their materials are checked for
        // errors (2 updates) plus one more for good luck.
        const int IDLE_FRAMES_TO_WAIT = 3;
        ++m_numBeforeDisplayErrorFrames;
        if (m_numBeforeDisplayErrorFrames > IDLE_FRAMES_TO_WAIT)
        {
            GetIEditor()->CommitLevelErrorReport();
            GetIEditor()->GetErrorReport()->Display();
            m_numBeforeDisplayErrorFrames = 0;
            m_levelErrorsHaveBeenDisplayed = true;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::ExportLevel(bool bExportToGame, bool bExportTexture, bool bAutoExport)
{
    if (bExportTexture)
    {
        CGameExporter gameExporter;
        gameExporter.SetAutoExportMode(bAutoExport);
        gameExporter.Export(eExp_SurfaceTexture);
    }
    else if (bExportToGame)
    {
        CGameExporter gameExporter;
        gameExporter.SetAutoExportMode(bAutoExport);
        gameExporter.Export();
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditHold()
{
    GetIEditor()->GetDocument()->Hold(HOLD_FETCH_FILE);
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditFetch()
{
    GetIEditor()->GetDocument()->Fetch(HOLD_FETCH_FILE);
}


//////////////////////////////////////////////////////////////////////////
bool CCryEditApp::UserExportToGame(const TerrainTextureExportSettings& exportSettings, bool bReloadTerrain, bool bNoMsgBox)
{
    if (!GetIEditor()->GetGameEngine()->IsLevelLoaded())
    {
        if (bNoMsgBox == false)
        {
            QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Please load a level before attempting to export."));
        }
        return false;
    }
    else
    {
        CAIManager* pAIMgr = GetIEditor()->GetAI();
        if (pAIMgr && !pAIMgr->IsNavigationFullyGenerated())
        {
            int msgBoxResult = CryMessageBox("The processing of the navigation data is still not completed."
                    " If you export now the navigation data will probably be corrupted."
                    " Do you want to proceed anyway with the export operation?",
                    "Navigation System Warning", MB_YESNO | MB_ICONQUESTION);
            if (msgBoxResult != IDYES)
            {
                return false;
            }
        }

        EditorUtils::AzWarningAbsorber absorb("Source Control");

        // Record errors and display a dialog with them at the end.
        CErrorsRecorder errRecorder(GetIEditor());

        // Temporarily disable auto backup.
        CScopedVariableSetter<bool> autoBackupEnabledChange(gSettings.autoBackupEnabled, false);
        CScopedVariableSetter<int> autoRemindTimeChange(gSettings.autoRemindTime, 0);

        m_bIsExportingLegacyData = true;
        CGameExporter gameExporter;

        unsigned int flags = eExp_CoverSurfaces;

        if (bReloadTerrain)
        {
            flags |= eExp_ReloadTerrain;
        }

        if (exportSettings.m_exportType != TerrainTextureExportTechnique::NoExport)
        {
            uint32 textureResolution = exportSettings.m_defaultResolution;
            bool exportSurfaceTexture = true;

            if (exportSettings.m_exportType == TerrainTextureExportTechnique::PromptUser)
            {
                CDimensionsDialog   dimensionDialog;
                dimensionDialog.SetDimensions(textureResolution);

                // Query the size of the preview
                if (dimensionDialog.exec() != QDialog::Accepted)
                {
                    exportSurfaceTexture = false;
                }

                textureResolution = dimensionDialog.GetDimensions();
            }

            if (exportSurfaceTexture)
            {
                SGameExporterSettings& settings = gameExporter.GetSettings();
                settings.iExportTexWidth = textureResolution;

                flags |= eExp_SurfaceTexture;
            }
        }

        // Change the cursor to show that we're busy.
        QWaitCursor wait;

        if (gameExporter.Export(flags, eLittleEndian, "."))
        {
            if (bNoMsgBox == false)
            {
                if (exportSettings.m_exportType == TerrainTextureExportTechnique::NoExport)
                {
                    CLogFile::WriteLine("$3Export to the game was successfully done.");
                    QMessageBox::information(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The level was successfully exported"));
                }
                else
                {
                    if (flags & eExp_SurfaceTexture)
                    {
                        QMessageBox::information(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The terrain texture was successfully generated"));
                    }
                    else
                    {
                        QMessageBox::information(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The terrain texture generation was canceled"));
                    }
                }
            }
            m_bIsExportingLegacyData = false;
            return true;
        }
        m_bIsExportingLegacyData = false;
        return false;
    }
}

void CCryEditApp::ExportToGame(const TerrainTextureExportSettings& exportSettings, bool bNoMsgBox)
{
    CGameEngine* pGameEngine = GetIEditor()->GetGameEngine();
    if (!pGameEngine->IsLevelLoaded())
    {
        if (pGameEngine->GetLevelPath().isEmpty())
        {
            QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), ("Open or create a level first."));
            return;
        }

        CErrorsRecorder errRecorder(GetIEditor());
        // If level not loaded first fast export terrain.
        m_bIsExportingLegacyData = true;
        CGameExporter gameExporter;
        gameExporter.Export(eExp_ReloadTerrain);
        m_bIsExportingLegacyData = false;
    }

    {
        UserExportToGame(exportSettings, true, bNoMsgBox);
    }
}

void CCryEditApp::GenerateTerrainTextureWithPrompts()
{
    ExportToGame(TerrainTextureExportSettings(TerrainTextureExportTechnique::PromptUser), true);
}

void CCryEditApp::GenerateTerrainTexture(const TerrainTextureExportSettings& exportSettings)
{
    ExportToGame(exportSettings, true);
}

void CCryEditApp::OnUpdateGenerateTerrainTexture(QAction* action)
{
    CGameEngine* pGameEngine = GetIEditor()->GetGameEngine();
    action->setEnabled(!m_bIsExportingLegacyData
        && pGameEngine
        && !pGameEngine->GetLevelPath().isEmpty()
        && GetIEditor()
        && GetIEditor()->GetDocument()
        && GetIEditor()->GetDocument()->IsDocumentReady());
}

void CCryEditApp::GenerateTerrain()
{
#ifdef LY_TERRAIN_EDITOR
    CNewTerrainDialog dlg;

    if (!GetIEditor()->GetTerrainManager()->GetUseTerrain() && dlg.exec() == QDialog::Accepted)
    {
        GetIEditor()->Notify(eNotify_OnBeginTerrainCreate);

        int resolution = dlg.GetTerrainResolution();
        int unitSize = dlg.GetTerrainUnits();

        ////////////////////////////////////////////////////////////////////////
        // Reset heightmap (water level, etc) to default
        ////////////////////////////////////////////////////////////////////////
        GetIEditor()->GetTerrainManager()->ResetHeightMap();

        // If possible set terrain to correct size here, this will help with initial camera placement in new levels
        GetIEditor()->GetTerrainManager()->SetTerrainSize(resolution, unitSize);


        GetIEditor()->GetTerrainManager()->CreateDefaultLayer();

        XmlNodeRef m_node = GetIEditor()->GetDocument()->GetEnvironmentTemplate();
        if (m_node)
        {
            XmlNodeRef envState = m_node->findChild("EnvState");
            if (envState)
            {
                XmlNodeRef showTerrainSurface = envState->findChild("ShowTerrainSurface");
                showTerrainSurface->setAttr("value", "true");
                GetIEditor()->GetGameEngine()->ReloadEnvironment();
            }
        }

        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedTerrain);
        GetIEditor()->GetHeightmap()->UpdateEngineTerrain(true);

        GetIEditor()->GetGameEngine()->ReloadEnvironment();

        GetIEditor()->GetTerrainManager()->SetUseTerrain(true);

        GetIEditor()->Notify(eNotify_OnEndTerrainCreate);

        GenerateTerrainTexture(TerrainTextureExportSettings(TerrainTextureExportTechnique::PromptUser));
    }
#endif //#ifdef LY_TERRAIN_EDITOR
}

void CCryEditApp::OnUpdateGenerateTerrain(QAction* action)
{
#ifdef LY_TERRAIN_EDITOR
    CGameEngine* pGameEngine = GetIEditor()->GetGameEngine();
    // Only enable if the current level doesn't use terrain
    action->setEnabled(!m_bIsExportingLegacyData && !GetIEditor()->GetTerrainManager()->GetUseTerrain() && pGameEngine && !pGameEngine->GetLevelPath().isEmpty() && GetIEditor()->GetDocument()->IsDocumentReady());
#endif //#ifdef LY_TERRAIN_EDITOR
}

void CCryEditApp::OnFileExportToGameNoSurfaceTexture()
{
    UserExportToGame(TerrainTextureExportSettings(TerrainTextureExportTechnique::NoExport), false, false);
}

void CCryEditApp::ToolTerrain()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::TerrainEditor);
}

void CCryEditApp::OnOpenParticleEditor()
{
    GetIEditor()->OpenView("Particle Editor");
}

void CCryEditApp::MeasurementSystemTool()
{
    GetIEditor()->OpenView(MEASUREMENT_SYSTEM_WINDOW_NAME);
}

void CCryEditApp::ToolLighting()
{
    ////////////////////////////////////////////////////////////////////////
    // Show the Sun Trajectory dialog
    ////////////////////////////////////////////////////////////////////////

    // Disable all tools. (Possible layer painter tool).
    GetIEditor()->SetEditTool(0);

    GetIEditor()->OpenView(LIGHTING_TOOL_WINDOW_NAME);
}

void CCryEditApp::TerrainTextureExport()
{
#ifdef LY_TERRAIN_EDITOR
    GetIEditor()->SetEditTool(nullptr);

    CTerrainTextureExport cDialog;
    cDialog.exec();
#endif
}

void CCryEditApp::RefineTerrainTextureTiles()
{
#ifdef LY_TERRAIN_EDITOR
    if (QMessageBox::question(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Refine TerrainTexture?\r\n"
        "(all terrain texture tiles become split in 4 parts so a tile with 2048x2048\r\n"
        "no longer limits the resolution) You need to save afterwards!"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        if (!GetIEditor()->GetTerrainManager()->GetRGBLayer()->RefineTiles())
        {
            QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("TerrainTexture refine failed (make sure current data is saved)"));
        }
        else
        {
            QMessageBox::information(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Successfully refined TerrainTexture - Save is now required!"));
        }
    }
#endif //#ifdef LY_TERRAIN_EDITOR
}

void CCryEditApp::ToolTexture()
{
#ifdef LY_TERRAIN_EDITOR
    GetIEditor()->ReinitializeEditTool();

    ////////////////////////////////////////////////////////////////////////
    // Show the terrain texture dialog
    ////////////////////////////////////////////////////////////////////////

    GetIEditor()->OpenView(LyViewPane::TerrainTextureLayers);
#endif //#ifdef LY_TERRAIN_EDITOR
}

void CCryEditApp::OnGeneratorsStaticobjects()
{
    ////////////////////////////////////////////////////////////////////////
    // Show the static objects dialog
    ////////////////////////////////////////////////////////////////////////
    /*
        CStaticObjects cDialog;

        cDialog.DoModal();

        BeginWaitCursor();
        GetIEditor()->UpdateViews( eUpdateStatObj );
        GetIEditor()->GetDocument()->GetStatObjMap()->PlaceObjectsOnTerrain();
        EndWaitCursor();
        */
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditSelectAll()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        AzToolsFramework::EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;
        ////////////////////////////////////////////////////////////////////////
        // Select all map objects
        ////////////////////////////////////////////////////////////////////////
        AABB box(Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX), Vec3(FLT_MAX, FLT_MAX, FLT_MAX));
        GetIEditor()->GetObjectManager()->SelectObjects(box);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditSelectNone()
{
    AzToolsFramework::EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;

    CUndo undo("Unselect All");
    ////////////////////////////////////////////////////////////////////////
    // Remove the selection from all map objects
    ////////////////////////////////////////////////////////////////////////
    GetIEditor()->ClearSelection();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditInvertselection()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        AzToolsFramework::EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;
        GetIEditor()->GetObjectManager()->InvertSelection();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditDelete()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        AzToolsFramework::EditorMetricsEventsBusAction editorMetricsEventsBusActionWrapper(AzToolsFramework::EditorMetricsEventsBusTraits::NavigationTrigger::LeftClickMenu);
        DeleteSelectedEntities(true);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::DeleteSelectedEntities(bool includeDescendants)
{
    // If Edit tool active cannot delete object.
    if (GetIEditor()->GetEditTool())
    {
        if (GetIEditor()->GetEditTool()->OnKeyDown(GetIEditor()->GetViewManager()->GetView(0), VK_DELETE, 0, 0))
        {
            return;
        }
    }

    AzToolsFramework::EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;

    GetIEditor()->BeginUndo();
    CUndo undo("Delete Selected Object");
    GetIEditor()->GetObjectManager()->DeleteSelection();
    GetIEditor()->AcceptUndo("Delete Selection");
    GetIEditor()->SetModifiedFlag();
    GetIEditor()->SetModifiedModule(eModifiedBrushes);
}

void CCryEditApp::OnEditClone()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        if (GetIEditor()->GetObjectManager()->GetSelection()->IsEmpty())
        {
            QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(),
                QObject::tr("You have to select objects before you can clone them!"));
            return;
        }

        // Clear Widget selection - Prevents issues caused by cloning entities while a property in the Reflected Property Editor is being edited.
        if (QApplication::focusWidget())
        {
            QApplication::focusWidget()->clearFocus();
        }

        AzToolsFramework::EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;

        CEditTool* tool = GetIEditor()->GetEditTool();
        if (tool && qobject_cast<CObjectCloneTool*>(tool))
        {
            ((CObjectCloneTool*)tool)->Accept();
        }

        CObjectCloneTool* cloneTool = new CObjectCloneTool;
        GetIEditor()->SetEditTool(cloneTool);
        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedBrushes);

        // Accept the clone operation if users didn't choose to stick duplicated entities to the cursor
        // This setting can be changed in the global preference of the editor
        if (!gSettings.deepSelectionSettings.bStickDuplicate)
        {
            cloneTool->Accept();
            GetIEditor()->GetSelection()->FinishChanges();
        }
    }
}

void CCryEditApp::OnEditEscape()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        CEditTool* pEditTool = GetIEditor()->GetEditTool();
        // Abort current operation.
        if (pEditTool)
        {
            // If Edit tool active cannot delete object.
            CViewport* vp = GetIEditor()->GetActiveView();
            if (GetIEditor()->GetEditTool()->OnKeyDown(vp, VK_ESCAPE, 0, 0))
            {
                return;
            }

            if (GetIEditor()->GetEditMode() == eEditModeSelectArea)
            {
                GetIEditor()->SetEditMode(eEditModeSelect);
            }

            // Disable current tool.
            GetIEditor()->SetEditTool(0);
        }
        else
        {
            AzToolsFramework::EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;

            // Clear selection on escape.
            GetIEditor()->ClearSelection();
        }
    }
}

void CCryEditApp::OnMoveObject()
{
    ////////////////////////////////////////////////////////////////////////
    // Move the selected object to the marker position
    ////////////////////////////////////////////////////////////////////////
}

void CCryEditApp::OnSelectObject()
{
    ////////////////////////////////////////////////////////////////////////
    // Bring up the select object dialog
    ////////////////////////////////////////////////////////////////////////

    QtViewPaneManager::instance()->OpenPane(LyViewPane::LegacyObjectSelector);
}

void CCryEditApp::OnRenameObj()
{
}

void CCryEditApp::OnSetHeight()
{
}

void CCryEditApp::OnScriptCompileScript()
{
    ////////////////////////////////////////////////////////////////////////
    // Use the Lua compiler to compile a script
    ////////////////////////////////////////////////////////////////////////
    AssetSelectionModel selection = AssetSelectionModel::AssetTypeSelection("Lua Script");
    selection.SetMultiselect(true);
    AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::BrowseForAssets, selection);
    if (!selection.IsValid())
    {
        return;
    }

    CErrorsRecorder errRecorder(GetIEditor());

    //////////////////////////////////////////////////////////////////////////
    // Lock resources.
    // Speed ups loading a lot.
    ISystem* pSystem = GetIEditor()->GetSystem();
    pSystem->GetI3DEngine()->LockCGFResources();
    //////////////////////////////////////////////////////////////////////////
    for (auto product : selection.GetResults())
    {
        if (!CFileUtil::CompileLuaFile(product->GetFullPath().c_str()))
        {
            return;
        }

        // No errors
        // Reload this lua file.
        GetIEditor()->GetSystem()->GetIScriptSystem()->ReloadScript(
            product->GetFullPath().c_str(), false);
    }
    //////////////////////////////////////////////////////////////////////////
    // Unlock resources.
    // Some unneeded resources that were locked before may get released here.
    pSystem->GetI3DEngine()->UnlockCGFResources();
    //////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnScriptEditScript()
{
    // Let the user choose a LUA script file to edit
    AssetSelectionModel selection;

    AssetGroupFilter* assetGroupFilter = new AssetGroupFilter();
    assetGroupFilter->SetAssetGroup("Script");

    EntryTypeFilter* entryTypeFilter = new EntryTypeFilter();
    entryTypeFilter->SetEntryType(AssetBrowserEntry::AssetEntryType::Source);

    CompositeFilter* compFilter = new CompositeFilter(CompositeFilter::LogicOperatorType::AND);
    compFilter->AddFilter(FilterConstType(assetGroupFilter));
    compFilter->AddFilter(FilterConstType(entryTypeFilter));

    selection.SetSelectionFilter(FilterConstType(compFilter));

    AzToolsFramework::EditorRequests::Bus::Broadcast(&AzToolsFramework::EditorRequests::BrowseForAssets, selection);
    if (selection.IsValid())
    {
        CFileUtil::EditTextFile(selection.GetResult()->GetFullPath().c_str());
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeMove()
{
    if (GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        using namespace AzToolsFramework;
        EditorTransformComponentSelectionRequestBus::Event(
            GetEntityContextId(),
            &EditorTransformComponentSelectionRequests::SetTransformMode,
            EditorTransformComponentSelectionRequests::Mode::Translation);
    }
    else
    {
        GetIEditor()->SetEditMode(eEditModeMove);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeRotate()
{
    if (GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        using namespace AzToolsFramework;
        EditorTransformComponentSelectionRequestBus::Event(
            GetEntityContextId(),
            &EditorTransformComponentSelectionRequests::SetTransformMode,
            EditorTransformComponentSelectionRequests::Mode::Rotation);
    }
    else
    {
        GetIEditor()->SetEditMode(eEditModeRotate);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeScale()
{
    if (GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        using namespace AzToolsFramework;
        EditorTransformComponentSelectionRequestBus::Event(
            GetEntityContextId(),
            &EditorTransformComponentSelectionRequests::SetTransformMode,
            EditorTransformComponentSelectionRequests::Mode::Scale);
    }
    else
    {
        GetIEditor()->SetEditMode(eEditModeScale);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditToolLink()
{
    // TODO: Add your command handler code here
    if (qobject_cast<CLinkTool*>(GetIEditor()->GetEditTool()))
    {
        GetIEditor()->SetEditTool(0);
    }
    else
    {
        GetIEditor()->SetEditTool(new CLinkTool());
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditToolLink(QAction* action)
{
    if (!GetIEditor()->GetDocument())
    {
        action->setEnabled(false);
        return;
    }
    action->setEnabled(GetIEditor()->GetDocument()->IsDocumentReady());
    CEditTool* pEditTool = GetIEditor()->GetEditTool();
    action->setChecked(qobject_cast<CLinkTool*>(pEditTool) != nullptr);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditToolUnlink()
{
    CUndo undo("Unlink Object(s)");
    CSelectionGroup* pSelection = GetIEditor()->GetObjectManager()->GetSelection();
    for (int i = 0; i < pSelection->GetCount(); i++)
    {
        CBaseObject* pBaseObj = pSelection->GetObject(i);
        CBaseObjectPtr pParent = pBaseObj->GetParent();
        CBaseObjectPtr pGroup = pBaseObj->GetGroup();
        pBaseObj->DetachThis();
        if (pParent != pGroup && pGroup)
        {
            pGroup->AddMember(pBaseObj);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditToolUnlink(QAction* action)
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    if (pSelection->IsEmpty())
    {
        action->setEnabled(false);
    }
    else if (pSelection->GetCount() == 1 && !pSelection->GetObject(0)->GetParent())
    {
        action->setEnabled(false);
    }
    else
    {
        BOOL bEnabled(false);
        for (int i = 0, iSelectionCount(pSelection->GetCount()); i < iSelectionCount; ++i)
        {
            CBaseObject* pSelObject = pSelection->GetObject(i);
            if (pSelObject == NULL)
            {
                continue;
            }
            if (pSelObject->GetLinkParent() != pSelObject->GetGroup())
            {
                bEnabled = true;
                break;
            }
        }
        action->setEnabled(bEnabled);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeSelect()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        GetIEditor()->SetEditMode(eEditModeSelect);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeSelectarea()
{
    // TODO: Add your command handler code here
    GetIEditor()->SetEditMode(eEditModeSelectArea);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeSelectarea(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(GetIEditor()->GetEditMode() == eEditModeSelectArea);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeSelect(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        action->setChecked(GetIEditor()->GetEditMode() == eEditModeSelect);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeMove(QAction* action)
{
    Q_ASSERT(action->isCheckable());

    if (GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        AzToolsFramework::EditorTransformComponentSelectionRequests::Mode mode;
        AzToolsFramework::EditorTransformComponentSelectionRequestBus::EventResult(
            mode, AzToolsFramework::GetEntityContextId(),
            &AzToolsFramework::EditorTransformComponentSelectionRequests::GetTransformMode);

        action->setChecked(mode == AzToolsFramework::EditorTransformComponentSelectionRequests::Mode::Translation);
    }
    else
    {
        action->setChecked(GetIEditor()->GetEditMode() == eEditModeMove);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeRotate(QAction* action)
{
    Q_ASSERT(action->isCheckable());

    if (GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        AzToolsFramework::EditorTransformComponentSelectionRequests::Mode mode;
        AzToolsFramework::EditorTransformComponentSelectionRequestBus::EventResult(
            mode, AzToolsFramework::GetEntityContextId(),
            &AzToolsFramework::EditorTransformComponentSelectionRequests::GetTransformMode);

        action->setChecked(mode == AzToolsFramework::EditorTransformComponentSelectionRequests::Mode::Rotation);
    }
    else
    {
        action->setChecked(GetIEditor()->GetEditMode() == eEditModeRotate);

        CSelectionGroup* pSelection = GetIEditor()->GetSelection();
        if (pSelection->GetCount() == 1 && !pSelection->GetObject(0)->IsRotatable())
        {
            action->setEnabled(false);
        }
        else
        {
            action->setEnabled(true);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeScale(QAction* action)
{
    Q_ASSERT(action->isCheckable());

    if (GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        AzToolsFramework::EditorTransformComponentSelectionRequests::Mode mode;
        AzToolsFramework::EditorTransformComponentSelectionRequestBus::EventResult(
            mode, AzToolsFramework::GetEntityContextId(),
            &AzToolsFramework::EditorTransformComponentSelectionRequests::GetTransformMode);

        action->setChecked(mode == AzToolsFramework::EditorTransformComponentSelectionRequests::Mode::Scale);
    }
    else
    {
        action->setChecked(GetIEditor()->GetEditMode() == eEditModeScale);

        CSelectionGroup* pSelection = GetIEditor()->GetSelection();
        if (pSelection->GetCount() == 1 && !pSelection->GetObject(0)->IsScalable())
        {
            action->setEnabled(false);
        }
        else
        {
            action->setEnabled(true);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeVertexSnapping(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    CEditTool* pEditTool = GetIEditor()->GetEditTool();
    action->setChecked(qobject_cast<CVertexSnappingModeTool*>(pEditTool) != nullptr);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnObjectSetArea()
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    if (!pSelection->IsEmpty())
    {
        bool ok = false;
        int fractionalDigitCount = 2;
        float area = aznumeric_caster(QInputDialog::getDouble(AzToolsFramework::GetActiveWindow(), QObject::tr("Insert Value"), QStringLiteral(""), 0, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max(), fractionalDigitCount, &ok));
        if (!ok)
        {
            return;
        }

        GetIEditor()->BeginUndo();
        for (int i = 0; i < pSelection->GetCount(); i++)
        {
            CBaseObject* obj = pSelection->GetObject(i);
            obj->SetArea(area);
        }
        GetIEditor()->AcceptUndo("Set Area");
        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedBrushes);
    }
    else
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("No objects selected"));
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnObjectSetHeight()
{
#if ENABLE_CRY_PHYSICS
    IPhysicalWorld* pPhysics = GetIEditor()->GetSystem()->GetIPhysicalWorld();
#else
    AzFramework::EntityContextId editorContextId;
    AzToolsFramework::EditorEntityContextRequestBus::BroadcastResult(
        editorContextId, &AzToolsFramework::EditorEntityContextRequests::GetEditorEntityContextId);
#endif

    CSelectionGroup* sel = GetIEditor()->GetObjectManager()->GetSelection();

    if (!sel->IsEmpty())
    {
        // Retrieve the Z origin from where height is messured from
        auto getZOrigin = [&](const Vec3& pos, AZ::EntityId entityId)
        {
            float z = GetIEditor()->GetTerrainElevation(pos.x, pos.y);
            if (z != pos.z)
            {
                float zdown = FLT_MAX;
                float zup = FLT_MAX;
#if ENABLE_CRY_PHYSICS
                ray_hit hit;
                if (pPhysics->RayWorldIntersection(pos, Vec3(0, 0, -4000), ent_all, rwi_stop_at_pierceable | rwi_ignore_noncolliding, &hit, 1) > 0)
                {
                    zdown = hit.pt.z;
                }
                if (pPhysics->RayWorldIntersection(pos, Vec3(0, 0, 4000), ent_all, rwi_stop_at_pierceable | rwi_ignore_noncolliding, &hit, 1) > 0)
                {
                    zup = hit.pt.z;
                }
#else
                AzFramework::RenderGeometry::RayRequest ray;
                ray.m_startWorldPosition = LYVec3ToAZVec3(pos);
                ray.m_onlyVisible = true;
                if (entityId.IsValid()) // Don't check height against self
                {
                    ray.m_entityFilter.m_ignoreEntities.insert(entityId);
                }
                // Down
                ray.m_endWorldPosition = LYVec3ToAZVec3(pos - Vec3(0, 0, 4000));
                {
                    AzFramework::RenderGeometry::RayResult result;
                    AzFramework::RenderGeometry::IntersectorBus::EventResult(result, editorContextId,
                        &AzFramework::RenderGeometry::IntersectorInterface::RayIntersect, ray);
                    if (result)
                    {
                        zdown = result.m_worldPosition.GetZ();
                    }
                }
                // Up
                ray.m_endWorldPosition = LYVec3ToAZVec3(pos + Vec3(0, 0, 4000));
                {
                    AzFramework::RenderGeometry::RayResult result;
                    AzFramework::RenderGeometry::IntersectorBus::EventResult(result, editorContextId,
                        &AzFramework::RenderGeometry::IntersectorInterface::RayIntersect, ray);
                    if (result)
                    {
                        zup = result.m_worldPosition.GetZ();
                    }
                }
#endif
                if (zdown != FLT_MAX && zup != FLT_MAX)
                {
                    if (fabs(zup - z) < fabs(zdown - z))
                    {
                        z = zup;
                    }
                    else
                    {
                        z = zdown;
                    }
                }
                else if (zup != FLT_MAX)
                {
                    z = zup;
                }
                else if (zdown != FLT_MAX)
                {
                    z = zdown;
                }
            }
            return z;
        };


        float height = 0;
        if (sel->GetCount() == 1)
        {
            CBaseObject* obj = sel->GetObject(0);
            Vec3 pos = obj->GetWorldPos();
            AZ::EntityId entityId;
            if (obj->GetType() == OBJTYPE_AZENTITY)
            {
                entityId = static_cast<CComponentEntityObject*>(obj)->GetAssociatedEntityId();
            }
            height = pos.z - getZOrigin(pos, entityId);
        }

        bool ok = false;
        int fractionalDigitCount = 2;
        height = aznumeric_caster(QInputDialog::getDouble(AzToolsFramework::GetActiveWindow(), QObject::tr("Enter Height"), QStringLiteral(""), height, -10000, 10000, fractionalDigitCount, &ok));
        if (!ok)
        {
            return;
        }

        CUndo undo("Set Height");
        for (int i = 0; i < sel->GetCount(); i++)
        {
            CBaseObject* obj = sel->GetObject(i);
            Matrix34 wtm = obj->GetWorldTM();
            Vec3 pos = wtm.GetTranslation();
            AZ::EntityId entityId;
            if (obj->GetType() == OBJTYPE_AZENTITY)
            {
                entityId = static_cast<CComponentEntityObject*>(obj)->GetAssociatedEntityId();
            }
            float z = getZOrigin(pos, entityId);
            pos.z = z + height;
            wtm.SetTranslation(pos);
            obj->SetWorldTM(wtm, eObjectUpdateFlags_UserInput);
        }

        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedBrushes);
    }
    else
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("No objects selected"));
    }
}

void CCryEditApp::OnObjectVertexSnapping()
{
    CEditTool* pEditTool = GetIEditor()->GetEditTool();
    if (qobject_cast<CVertexSnappingModeTool*>(pEditTool))
    {
        GetIEditor()->SetEditTool(NULL);
    }
    else
    {
        GetIEditor()->SetEditTool("EditTool.VertexSnappingMode");
    }
}

void CCryEditApp::OnObjectmodifyFreeze()
{
    // Freeze selection.
    OnEditFreeze();
}

void CCryEditApp::OnObjectmodifyUnfreeze()
{
    // Unfreeze all.
    OnEditUnfreezeall();
}

void CCryEditApp::OnViewSwitchToGame()
{
    if (IsInPreviewMode())
    {
        return;
    }
    // close all open menus
    auto activePopup = qApp->activePopupWidget();
    if (qobject_cast<QMenu*>(activePopup))
    {
        activePopup->hide();
    }
    // TODO: Add your command handler code here
    bool inGame = !GetIEditor()->IsInGameMode();
    GetIEditor()->SetInGameMode(inGame);
}

void CCryEditApp::OnViewDeploy()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::DeploymentTool);
}

void CCryEditApp::OnSelectAxisX()
{
    AxisConstrains axis = (GetIEditor()->GetAxisConstrains() != AXIS_X) ? AXIS_X : AXIS_NONE;
    GetIEditor()->SetAxisConstraints(axis);
}

void CCryEditApp::OnSelectAxisY()
{
    AxisConstrains axis = (GetIEditor()->GetAxisConstrains() != AXIS_Y) ? AXIS_Y : AXIS_NONE;
    GetIEditor()->SetAxisConstraints(axis);
}

void CCryEditApp::OnSelectAxisZ()
{
    AxisConstrains axis = (GetIEditor()->GetAxisConstrains() != AXIS_Z) ? AXIS_Z : AXIS_NONE;
    GetIEditor()->SetAxisConstraints(axis);
}

void CCryEditApp::OnSelectAxisXy()
{
    AxisConstrains axis = (GetIEditor()->GetAxisConstrains() != AXIS_XY) ? AXIS_XY : AXIS_NONE;
    GetIEditor()->SetAxisConstraints(axis);
}

void CCryEditApp::OnUpdateSelectAxisX(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(GetIEditor()->GetAxisConstrains() == AXIS_X);
}

void CCryEditApp::OnUpdateSelectAxisXy(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(GetIEditor()->GetAxisConstrains() == AXIS_XY);
}

void CCryEditApp::OnUpdateSelectAxisY(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(GetIEditor()->GetAxisConstrains() == AXIS_Y);
}

void CCryEditApp::OnUpdateSelectAxisZ(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(GetIEditor()->GetAxisConstrains() == AXIS_Z);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSelectAxisTerrain()
{
    IEditor* editor = GetIEditor();
    bool isAlreadyEnabled = (editor->GetAxisConstrains() == AXIS_TERRAIN) && (editor->IsTerrainAxisIgnoreObjects());
    if (!isAlreadyEnabled)
    {
        editor->SetAxisConstraints(AXIS_TERRAIN);
        editor->SetTerrainAxisIgnoreObjects(true);
    }
    else
    {
        // behave like a toggle button - click on the same thing again to disable.
        editor->SetAxisConstraints(AXIS_NONE);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSelectAxisSnapToAll()
{
    IEditor* editor = GetIEditor();
    bool isAlreadyEnabled = (editor->GetAxisConstrains() == AXIS_TERRAIN) && (!editor->IsTerrainAxisIgnoreObjects());
    if (!isAlreadyEnabled)
    {
        editor->SetAxisConstraints(AXIS_TERRAIN);
        editor->SetTerrainAxisIgnoreObjects(false);
    }
    else
    {
        // behave like a toggle button - click on the same thing again to disable.
        editor->SetAxisConstraints(AXIS_NONE);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSelectAxisTerrain(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(GetIEditor()->GetAxisConstrains() == AXIS_TERRAIN && GetIEditor()->IsTerrainAxisIgnoreObjects());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSelectAxisSnapToAll(QAction* action)
{
    action->setChecked(GetIEditor()->GetAxisConstrains() == AXIS_TERRAIN && !GetIEditor()->IsTerrainAxisIgnoreObjects());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnExportSelectedObjects()
{
    CExportManager* pExportManager = static_cast<CExportManager*> (GetIEditor()->GetExportManager());
    QString filename = "untitled";
    CBaseObject* pObj = GetIEditor()->GetSelectedObject();
    if (pObj)
    {
        filename = pObj->GetName();
    }
    else
    {
        QString levelName = GetIEditor()->GetGameEngine()->GetLevelName();
        if (!levelName.isEmpty())
        {
            filename = levelName;
        }
    }
    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    pExportManager->Export(filename.toUtf8().data(), "obj", levelPath.toUtf8().data());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnExportTerrainArea()
{
    CExportManager* pExportManager = static_cast<CExportManager*> (GetIEditor()->GetExportManager());
    QString levelName = GetIEditor()->GetGameEngine()->GetLevelName();
    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    pExportManager->Export((levelName + "_terrain").toUtf8().data(), "obj", levelPath.toUtf8().data(), false, false, true);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnExportTerrainAreaWithObjects()
{
    CExportManager* pExportManager = static_cast<CExportManager*> (GetIEditor()->GetExportManager());
    QString levelName = GetIEditor()->GetGameEngine()->GetLevelName();
    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    pExportManager->Export(levelName.toUtf8().data(), "obj", levelPath.toUtf8().data(), false, true, true);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileExportOcclusionMesh()
{
    CExportManager* pExportManager = static_cast<CExportManager*> (GetIEditor()->GetExportManager());
    QString levelName = GetIEditor()->GetGameEngine()->GetLevelName();
    QString levelPath = GetIEditor()->GetGameEngine()->GetLevelPath();
    pExportManager->Export(levelName.toUtf8().data(), "ocm", levelPath.toUtf8().data(), false, false, false, true);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateExportTerrainArea(QAction* action)
{
    AABB box;
    GetIEditor()->GetSelectedRegion(box);
    action->setEnabled(!box.IsEmpty());
}

void CCryEditApp::OnSelectionSave()
{
    char szFilters[] = "Object Group Files (*.grp)";
    QtUtil::QtMFCScopedHWNDCapture cap;
    CAutoDirectoryRestoreFileDialog dlg(QFileDialog::AcceptSave, QFileDialog::AnyFile, "grp", {}, szFilters, {}, {}, cap);

    if (dlg.exec())
    {
        QWaitCursor wait;
        CSelectionGroup* sel = GetIEditor()->GetSelection();
        //CXmlArchive xmlAr( "Objects" );


        XmlNodeRef root = XmlHelpers::CreateXmlNode("Objects");
        CObjectArchive ar(GetIEditor()->GetObjectManager(), root, false);
        // Save all objects to XML.
        for (int i = 0; i < sel->GetCount(); i++)
        {
            ar.SaveObject(sel->GetObject(i));
        }
        QString fileName = dlg.selectedFiles().first();
        XmlHelpers::SaveXmlNode(GetIEditor()->GetFileUtil(), root, fileName.toStdString().c_str());
        //xmlAr.Save( dlg.GetPathName() );
    }
}

//////////////////////////////////////////////////////////////////////////
struct SDuplicatedObject
{
    SDuplicatedObject(const QString& name, const GUID& id)
    {
        m_name = name;
        m_id = id;
    }
    QString m_name;
    GUID m_id;
};

void GatherAllObjects(XmlNodeRef node, std::vector<SDuplicatedObject>& outDuplicatedObjects)
{
    if (!azstricmp(node->getTag(), "Object"))
    {
        GUID guid;
        if (node->getAttr("Id", guid))
        {
            if (GetIEditor()->GetObjectManager()->FindObject(guid))
            {
                QString name;
                node->getAttr("Name", name);
                outDuplicatedObjects.push_back(SDuplicatedObject(name, guid));
            }
        }
    }

    for (int i = 0, nChildCount(node->getChildCount()); i < nChildCount; ++i)
    {
        XmlNodeRef childNode = node->getChild(i);
        if (childNode == NULL)
        {
            continue;
        }
        GatherAllObjects(childNode, outDuplicatedObjects);
    }
}

void CCryEditApp::OnOpenAssetImporter()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::SceneSettings);
}

void CCryEditApp::OnSelectionLoad()
{
    // Load objects from .grp file.
    QtUtil::QtMFCScopedHWNDCapture cap;
    CAutoDirectoryRestoreFileDialog dlg(QFileDialog::AcceptOpen, QFileDialog::ExistingFile, "grp", {}, "Object Group Files (*.grp)", {}, {}, cap);
    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }

    QWaitCursor wait;

    XmlNodeRef root = XmlHelpers::LoadXmlFromFile(dlg.selectedFiles().first().toStdString().c_str());
    if (!root)
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Error at loading group file."));
        return;
    }

    std::vector<SDuplicatedObject> duplicatedObjects;
    GatherAllObjects(root, duplicatedObjects);

    CDuplicatedObjectsHandlerDlg::EResult result(CDuplicatedObjectsHandlerDlg::eResult_None);
    int nDuplicatedObjectSize(duplicatedObjects.size());

    if (!duplicatedObjects.empty())
    {
        QString msg = QObject::tr("The following object(s) already exist(s) in the level.\r\n\r\n");

        for (int i = 0; i < nDuplicatedObjectSize; ++i)
        {
            msg += QStringLiteral("\t");
            msg += duplicatedObjects[i].m_name;
            if (i < nDuplicatedObjectSize - 1)
            {
                msg += QStringLiteral("\r\n");
            }
        }

        CDuplicatedObjectsHandlerDlg dlg(msg);
        if (dlg.exec() == QDialog::Rejected)
        {
            return;
        }
        result = dlg.GetResult();
    }

    CUndo undo("Load Objects");
    GetIEditor()->ClearSelection();

    CObjectArchive ar(GetIEditor()->GetObjectManager(), root, true);

    if (result == CDuplicatedObjectsHandlerDlg::eResult_Override)
    {
        for (int i = 0; i < nDuplicatedObjectSize; ++i)
        {
            CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(duplicatedObjects[i].m_id);
            if (pObj)
            {
                GetIEditor()->GetObjectManager()->DeleteObject(pObj);
            }
        }
    }
    else if (result == CDuplicatedObjectsHandlerDlg::eResult_CreateCopies)
    {
        ar.MakeNewIds(true);
    }

    GetIEditor()->GetObjectManager()->LoadObjects(ar, true);
    GetIEditor()->SetModifiedFlag();
    GetIEditor()->SetModifiedModule(eModifiedBrushes);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnConvertSelectionToDesignerObject()
{
    GetIEditor()->Notify(eNotify_OnConvertToDesignerObjects);
}

void CCryEditApp::OnUpdateSelected(QAction* action)
{
    action->setEnabled(!GetIEditor()->GetSelection()->IsEmpty());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAlignObject()
{
    // Align pick callback will release itself.
    CAlignPickCallback* alignCallback = new CAlignPickCallback;
    GetIEditor()->PickObject(alignCallback, 0, "Align to Object");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAlignToGrid()
{
    CSelectionGroup* sel = GetIEditor()->GetSelection();
    if (!sel->IsEmpty())
    {
        CUndo undo("Align To Grid");
        Matrix34 tm;
        for (int i = 0; i < sel->GetCount(); i++)
        {
            CBaseObject* obj = sel->GetObject(i);
            tm = obj->GetWorldTM();
            Vec3 snaped = gSettings.pGrid->Snap(tm.GetTranslation());
            tm.SetTranslation(snaped);
            obj->SetWorldTM(tm, eObjectUpdateFlags_UserInput);
            obj->OnEvent(EVENT_ALIGN_TOGRID);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateAlignObject(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(CAlignPickCallback::IsActive());

    action->setEnabled(!GetIEditor()->GetSelection()->IsEmpty());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAlignToVoxel()
{
    CEditTool* pEditTool = GetIEditor()->GetEditTool();
    if (qobject_cast<CVoxelAligningTool*>(pEditTool) != nullptr)
    {
        GetIEditor()->SetEditTool(nullptr);
    }
    else
    {
        GetIEditor()->SetEditTool(new CVoxelAligningTool());
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateAlignToVoxel(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    CEditTool* pEditTool = GetIEditor()->GetEditTool();
    action->setChecked(qobject_cast<CVoxelAligningTool*>(pEditTool) != nullptr);

    action->setEnabled(!GetIEditor()->GetSelection()->IsEmpty());
}

//////////////////////////////////////////////////////////////////////////
// Groups.
//////////////////////////////////////////////////////////////////////////

void CCryEditApp::OnGroupAttach()
{
    // TODO: Add your command handler code here
    GetIEditor()->GetSelection()->PickGroupAndAddToIt();
}

void CCryEditApp::OnUpdateGroupAttach(QAction* action)
{
    BOOL bEnable = false;
    if (!GetIEditor()->GetSelection()->IsEmpty())
    {
        bEnable = true;
    }
    action->setEnabled(bEnable);
}

void CCryEditApp::OnGroupClose()
{
    // Close _all_ selected open groups.
    CSelectionGroup* selected = GetIEditor()->GetSelection();
    for (unsigned int i = 0; i < selected->GetCount(); i++)
    {
        CBaseObject* obj = selected->GetObject(i);
        if (obj && obj->metaObject() == &CGroup::staticMetaObject)
        {
            if (((CGroup*)obj)->IsOpen())
            {
                GetIEditor()->BeginUndo();
                ((CGroup*)obj)->Close();
                GetIEditor()->AcceptUndo("Group Close");
                GetIEditor()->SetModifiedFlag();
                GetIEditor()->SetModifiedModule(eModifiedBrushes);
            }
        }
    }
}

void CCryEditApp::OnUpdateGroupClose(QAction* action)
{
    BOOL bEnable = false;

    // Alllow multiple groups to be closed at the same time.
    CSelectionGroup* selected = GetIEditor()->GetSelection();
    for (unsigned int i = 0; i < selected->GetCount(); i++)
    {
        // if there are _any_ open grouped objects selected allow them to be closed.
        CBaseObject* obj = selected->GetObject(i);
        if (obj && obj->metaObject() == &CGroup::staticMetaObject)
        {
            if (((CGroup*)obj)->IsOpen())
            {
                bEnable = true;
            }
        }
    }

    action->setEnabled(bEnable);
}


void CCryEditApp::OnGroupDetach()
{
    CUndo undo("Remove Selection from Group");
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    for (int i = 0, cnt = pSelection->GetCount(); i < cnt; ++i)
    {
        if (pSelection->GetObject(i)->GetGroup())
        {
            pSelection->GetObject(i)->GetGroup()->RemoveMember(pSelection->GetObject(i));
        }
    }
}


void CCryEditApp::OnUpdateGroupDetach(QAction* action)
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    for (int i = 0, cnt = pSelection->GetCount(); i < cnt; ++i)
    {
        if (pSelection->GetObject(i)->GetParent())
        {
            action->setEnabled(true);
            return;
        }
    }

    action->setEnabled(false);
}



void CCryEditApp::OnShowHelpers()
{
    CEditTool* pEditTool(GetIEditor()->GetEditTool());
    if (pEditTool && pEditTool->IsNeedSpecificBehaviorForSpaceAcce())
    {
        return;
    }
    GetIEditor()->GetDisplaySettings()->DisplayHelpers(!GetIEditor()->GetDisplaySettings()->IsDisplayHelpers());
    GetIEditor()->Notify(eNotify_OnDisplayRenderUpdate);
}

void CCryEditApp::OnGroupMake()
{
    // Pre-filter the selected objects to make sure that they can be grouped together
    CSelectionGroup* selection = GetIEditor()->GetSelection();
    selection->FilterParents();
    std::vector<CBaseObjectPtr> objects;
    bool canGroup = true;
    for (int i = 0; i < selection->GetFilteredCount(); i++)
    {
        auto selected = selection->GetFilteredObject(i);
        if (!selected->IsGroupable())
        {
            canGroup = false;
            break;
        }
        objects.push_back(selected);
    }
    if (!canGroup)
    {
        CryMessageBox("One or more component entities detected.  You cannot group component entities at this time.",
                      "Error", MB_OK);
        return;
    }

    StringDlg dlg(QObject::tr("Group Name"));


    GetIEditor()->BeginUndo();
    CGroup* group = (CGroup*)GetIEditor()->NewObject("Group");
    if (!group)
    {
        GetIEditor()->CancelUndo();
        return;
    }

    dlg.SetString(group->GetName());

    if (dlg.exec() == QDialog::Accepted)
    {
        // Setup a layer to the group and don't modify the current layer (by Undo system)
        if (objects.size())
        {
            GetIEditor()->SuspendUndo();
            group->SetLayer(objects[0]->GetLayer());
            GetIEditor()->ResumeUndo();
        }

        GetIEditor()->GetObjectManager()->ChangeObjectName(group, dlg.GetString());

        // Snap center to grid.
        Vec3 center = gSettings.pGrid->Snap(selection->GetCenter());
        group->SetPos(center);

        // Prefab support
        CPrefabObject* pPrefabToCompareAgainst = nullptr;
        CPrefabObject* pObjectPrefab = nullptr;
        bool partOfDifferentPrefabs = false;

        for (int i = 0; i < objects.size(); i++)
        {
            // Prefab handling
            pObjectPrefab = objects[i]->GetPrefab();

            if (pPrefabToCompareAgainst && pObjectPrefab)
            {
                partOfDifferentPrefabs = pPrefabToCompareAgainst != pObjectPrefab;
            }

            if (!pPrefabToCompareAgainst)
            {
                pPrefabToCompareAgainst = pObjectPrefab;
            }

            GetIEditor()->GetObjectManager()->UnselectObject(objects[i]);
            group->AddMember(objects[i]);
        }

        // Sanity check if user is trying to group objects from different prefabs
        if (partOfDifferentPrefabs)
        {
            GetIEditor()->CancelUndo();
            GetIEditor()->DeleteObject(group);
            return;
        }

        // Signal that we added a group to the prefab
        if (pPrefabToCompareAgainst)
        {
            pPrefabToCompareAgainst->AddMember(group);
        }

        // need again setup the layer to inform group children
        if (objects.size())
        {
            group->SetLayer(objects[0]->GetLayer());
        }

        GetIEditor()->AcceptUndo("Group Make");
        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedBrushes);
    }
    else
    {
        GetIEditor()->CancelUndo();
        GetIEditor()->DeleteObject(group);
    }
}

void CCryEditApp::OnUpdateGroupMake(QAction* action)
{
    OnUpdateSelected(action);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGroupOpen()
{
    // Ungroup all groups in selection.
    CSelectionGroup* sel = GetIEditor()->GetSelection();
    if (!sel->IsEmpty())
    {
        CUndo undo("Group Open");
        for (int i = 0; i < sel->GetCount(); i++)
        {
            CBaseObject* obj = sel->GetObject(i);
            if (obj && obj->metaObject() == &CGroup::staticMetaObject)
            {
                ((CGroup*)obj)->Open();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateGroupOpen(QAction* action)
{
    BOOL bEnable = false;
    CBaseObject* obj = GetIEditor()->GetSelectedObject();
    if (obj)
    {
        if (obj->metaObject() == &CGroup::staticMetaObject)
        {
            if (!((CGroup*)obj)->IsOpen())
            {
                bEnable = true;
            }
        }
        action->setEnabled(bEnable);
    }
    else
    {
        OnUpdateSelected(action);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGroupUngroup()
{
    // Ungroup all groups in selection.
    std::vector<CBaseObjectPtr> objects;

    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    for (int i = 0; i < pSelection->GetCount(); i++)
    {
        objects.push_back(pSelection->GetObject(i));
    }

    if (objects.size())
    {
        CUndo undo("Ungroup");

        for (int i = 0; i < objects.size(); i++)
        {
            CBaseObject* obj = objects[i];
            if (obj && obj->metaObject() == &CGroup::staticMetaObject)
            {
                static_cast<CGroup*>(obj)->Ungroup();
                // Signal prefab if part of any
                if (CPrefabObject* pPrefab = obj->GetPrefab())
                {
                    pPrefab->RemoveMember(obj);
                }
                GetIEditor()->DeleteObject(obj);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateGroupUngroup(QAction* action)
{
    CBaseObject* obj = GetIEditor()->GetSelectedObject();
    if (obj)
    {
        if (obj->metaObject() == &CGroup::staticMetaObject)
        {
            action->setEnabled(true);
        }
        else
        {
            action->setEnabled(false);
        }
    }
    else
    {
        OnUpdateSelected(action);
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionRename()
{
    CMission *mission = GetIEditor()->GetDocument()->GetCurrentMission();
    StringDlg dlg(tr("Rename Mission"));
    dlg.SetString(mission->GetName());
    if (dlg.exec() == QDialog::Accepted)
    {
        GetIEditor()->BeginUndo();
        mission->SetName(dlg.GetString());
        GetIEditor()->AcceptUndo( "Mission Rename" );
    }
    GetIEditor()->Notify(eNotify_OnInvalidateControls);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionSelect()
{
    CMissionSelectDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        CMission* mission = GetIEditor()->GetDocument()->FindMission(dlg.GetSelected());
        if (mission)
        {
            GetIEditor()->GetDocument()->SetCurrentMission(mission);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnLockSelection()
{
    // Invert selection lock.
    GetIEditor()->LockSelection(!GetIEditor()->IsSelectionLocked());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditLevelData()
{
    auto dir = QFileInfo(GetIEditor()->GetDocument()->GetLevelPathName()).dir();
    CFileUtil::EditTextFile(dir.absoluteFilePath("LevelData.xml").toUtf8().data());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileEditLogFile()
{
    CFileUtil::EditTextFile(CLogFile::GetLogFileName(), 0, IFileUtil::FILE_TYPE_SCRIPT);
}

void CCryEditApp::OnFileResaveSlices()
{
    AZStd::vector<AZ::Data::AssetInfo> sliceAssetInfos;
    sliceAssetInfos.reserve(5000);
    AZ::Data::AssetCatalogRequests::AssetEnumerationCB sliceCountCb = [&sliceAssetInfos](const AZ::Data::AssetId id, const AZ::Data::AssetInfo& info)
    {
        // Only add slices and nothing that has been temporarily added to the catalog with a macro in it (ie @devroot@)
        if (info.m_assetType == azrtti_typeid<AZ::SliceAsset>() && info.m_relativePath[0] != '@')
        {
            sliceAssetInfos.push_back(info);
        }
    };
    AZ::Data::AssetCatalogRequestBus::Broadcast(&AZ::Data::AssetCatalogRequestBus::Events::EnumerateAssets, nullptr, sliceCountCb, nullptr);

    QString warningMessage = QString("Resaving all slices can be *extremely* slow depending on source control and on the number of slices in your project!\n\nYou can speed this up dramatically by checking out all your slices before starting this!\n\n Your project has %1 slices.\n\nDo you want to continue?").arg(sliceAssetInfos.size());

    if (QMessageBox::Cancel == QMessageBox::warning(MainWindow::instance(), tr("!!!WARNING!!!"), warningMessage, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel))
    {
        return;
    }

    AZ::SerializeContext* serialize = nullptr;
    AZ::ComponentApplicationBus::BroadcastResult(serialize, &AZ::ComponentApplicationBus::Events::GetSerializeContext);

    if (!serialize)
    {
        AZ_TracePrintf("Resave Slices", "Couldn't get the serialize context.  Something is very wrong.  Aborting!!!");
        return;
    }

    auto assetFilterCB = [](const AZ::Data::Asset<AZ::Data::AssetData>& asset)
    {
        return false;
    };

    AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
    if (!fileIO)
    {
        AZ_Error("Resave Slices", false, "File IO is not initialized.");
        return;
    }

    int numFailures = 0;

    // Create a lambda for load & save logic to make the lambda below easier to read
    auto LoadAndSaveSlice = [serialize, &assetFilterCB, &numFailures](const AZStd::string& filePath)
    {
        AZ::Entity* newRootEntity = nullptr;

        // Read in the slice file first
        {
            AZ::IO::FileIOStream readStream(filePath.c_str(), AZ::IO::OpenMode::ModeRead);
            newRootEntity = AZ::Utils::LoadObjectFromStream<AZ::Entity>(readStream, serialize, AZ::ObjectStream::FilterDescriptor(assetFilterCB));
        }

        // If we successfully loaded the file
        if (newRootEntity)
        {
            if (!AZ::Utils::SaveObjectToFile(filePath, AZ::DataStream::ST_XML, newRootEntity))
            {
                AZ_TracePrintf("Resave Slices", "Unable to serialize the slice (%s) out to a file.  Unable to resave this slice\n", filePath.c_str());
                numFailures++;
            }
        }
        else
        {
            AZ_TracePrintf("Resave Slices", "Unable to read a slice (%s) file from disk.  Unable to resave this slice.\n", filePath.c_str());
            numFailures++;
        }
    };

    const size_t numSlices = sliceAssetInfos.size();
    int slicesProcessed = 0;
    int slicesRequestedForProcessing = 0;

    if (numSlices > 0)
    {
        AzToolsFramework::ProgressShield::LegacyShowAndWait(MainWindow::instance(), tr("Checking out and resaving slices..."),
            [numSlices, &slicesProcessed, &sliceAssetInfos, &LoadAndSaveSlice, &slicesRequestedForProcessing, &numFailures](int& current, int& max)
            {
                const static int numToProcessPerCall = 5;

                if (slicesRequestedForProcessing < numSlices)
                {
                    for (int index = 0; index < numToProcessPerCall; index++)
                    {
                        if (slicesRequestedForProcessing < numSlices)
                        {
                            AZStd::string sourceFile;
                            AzToolsFramework::AssetSystemRequestBus::Broadcast(&AzToolsFramework::AssetSystemRequestBus::Events::GetFullSourcePathFromRelativeProductPath, sliceAssetInfos[slicesRequestedForProcessing].m_relativePath, sourceFile);

                            AzToolsFramework::ToolsApplicationRequestBus::Broadcast(&AzToolsFramework::ToolsApplicationRequestBus::Events::RequestEditForFile, sourceFile.c_str(), [&slicesProcessed, sourceFile, &LoadAndSaveSlice, &numFailures](bool success)
                                {
                                    slicesProcessed++;

                                    if (success)
                                    {
                                        LoadAndSaveSlice(sourceFile);
                                    }
                                    else
                                    {
                                        AZ_TracePrintf("Resave Slices", "Unable to check a slice (%s) out of source control.  Unable to resave this slice\n", sourceFile.c_str());
                                        numFailures++;
                                    }
                                }
                            );
                            slicesRequestedForProcessing++;
                        }
                    }
                }

                current = slicesProcessed;
                max = static_cast<int>(numSlices);
                return slicesProcessed == numSlices;
            }
        );

        QString completeMessage;
        if (numFailures > 0)
        {
            completeMessage = QString("All slices processed.  There were %1 slices that could not be resaved.  Please check the console for details.").arg(numFailures);
        }
        else
        {
            completeMessage = QString("All slices successfully process and re-saved!");
        }

        QMessageBox::information(MainWindow::instance(), tr("Re-saving complete"), completeMessage, QMessageBox::Ok);
    }
    else
    {
        QMessageBox::information(MainWindow::instance(), tr("No slices found"), tr("There were no slices found to resave."), QMessageBox::Ok);
    }

}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileEditEditorini()
{
    CFileUtil::EditTextFile(EDITOR_CFG_FILE);
}

void CCryEditApp::OnPreferences()
{
    /*
    //////////////////////////////////////////////////////////////////////////////
    // Accels edit by CPropertyPage
    CAcceleratorManager tmpAccelManager;
    tmpAccelManager = m_AccelManager;
    CAccelMapPage page(&tmpAccelManager);
    CPropertySheet sheet;
    sheet.SetTitle( _T("Preferences") );
    sheet.AddPage(&page);
    if (sheet.DoModal() == IDOK) {
        m_AccelManager = tmpAccelManager;
        m_AccelManager.UpdateWndTable();
    }
    */
}

void CCryEditApp::OnReloadTextures()
{
    QWaitCursor wait;
    CLogFile::WriteLine("Reloading Static objects textures and shaders.");
    GetIEditor()->GetObjectManager()->SendEvent(EVENT_RELOAD_TEXTURES);
    GetIEditor()->GetRenderer()->EF_ReloadTextures();
}

void CCryEditApp::OnReloadAllScripts()
{
    OnReloadEntityScripts();
    OnReloadActorScripts();
    OnReloadItemScripts();
    OnReloadAIScripts();
}

void CCryEditApp::OnReloadEntityScripts()
{
    GetIEditor()->GetObjectManager()->SendEvent(EVENT_UNLOAD_ENTITY);
    CEntityScriptRegistry::Instance()->Reload();

    SEntityEvent event;
    event.event = ENTITY_EVENT_RELOAD_SCRIPT;
    if (gEnv->pEntitySystem)
    {
        gEnv->pEntitySystem->SendEventToAll(event);
    }

    GetIEditor()->GetObjectManager()->SendEvent(EVENT_RELOAD_ENTITY);
}

void CCryEditApp::OnReloadActorScripts()
{
    gEnv->pConsole->ExecuteString("net_reseteditorclient");
}

void CCryEditApp::OnReloadItemScripts()
{
    gEnv->pConsole->ExecuteString("i_reload");
}

void CCryEditApp::OnReloadAIScripts()
{
    GetIEditor()->GetAI()->ReloadScripts();
}

void CCryEditApp::OnToggleMultiplayer()
{
    IEditor* pEditor = GetIEditor();
    if (pEditor->GetGameEngine()->SupportsMultiplayerGameRules())
    {
        if (pEditor->IsInGameMode())
        {
            pEditor->SetInGameMode(false);
        }

        pEditor->GetObjectManager()->SendEvent(EVENT_UNLOAD_ENTITY);

        pEditor->GetGameEngine()->ToggleMultiplayerGameRules();

        CEntityScriptRegistry::Instance()->Reload();
        pEditor->GetAI()->ReloadScripts();

        pEditor->GetObjectManager()->SendEvent(EVENT_RELOAD_ENTITY);

        CryLogAlways("Now running '%s' gamerules", pEditor->GetGame()->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules()->GetEntity()->GetClass()->GetName());
    }
    else
    {
        Warning("Multiplayer gamerules not supported.");
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToggleMultiplayerUpdate(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(gEnv->bMultiplayer);
    action->setEnabled(GetIEditor()->GetDocument() && GetIEditor()->GetDocument()->IsDocumentReady() && !GetIEditor()->IsInGameMode());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnReloadGeometry()
{
    CErrorsRecorder errRecorder(GetIEditor());
    CWaitProgress wait("Reloading static geometry");

    CVegetationMap* pVegetationMap = GetIEditor()->GetVegetationMap();
    if (pVegetationMap)
    {
        pVegetationMap->ReloadGeometry();
    }

    CLogFile::WriteLine("Reloading Static objects geometries.");
    CEdMesh::ReloadAllGeometries();

    // Reload CHRs
    if (GetIEditor()->GetSystem()->GetIAnimationSystem())
    {
        GetIEditor()->GetSystem()->GetIAnimationSystem()->ReloadAllModels();
    }

    GetIEditor()->GetObjectManager()->SendEvent(EVENT_UNLOAD_GEOM);
    //GetIEditor()->Get3DEngine()->UnlockCGFResources();

    if (GetIEditor()->GetGame()->GetIGameFramework()->GetIItemSystem() != NULL)

    {
        GetIEditor()->GetGame()->GetIGameFramework()->GetIItemSystem()->ClearGeometryCache();
    }
    //GetIEditor()->Get3DEngine()->LockCGFResources();
    // Force entity system to collect garbage.
    if (GetIEditor()->GetSystem()->GetIEntitySystem())
    {
        GetIEditor()->GetSystem()->GetIEntitySystem()->Update();
    }
    GetIEditor()->GetObjectManager()->SendEvent(EVENT_RELOAD_GEOM);
    GetIEditor()->Notify(eNotify_OnReloadTrackView);

    // Rephysicalize viewport meshes
    for (int i = 0; i < GetIEditor()->GetViewManager()->GetViewCount(); ++i)
    {
        CViewport* vp = GetIEditor()->GetViewManager()->GetView(i);
        if (CModelViewport* mvp = viewport_cast<CModelViewport*>(vp))
        {
            mvp->RePhysicalize();
        }
    }

    OnReloadEntityScripts();
    IRenderNode** plist = new IRenderNode*[
        max(
            max(gEnv->p3DEngine->GetObjectsByType(eERType_Vegetation, 0),
            gEnv->p3DEngine->GetObjectsByType(eERType_Brush, 0)),
            gEnv->p3DEngine->GetObjectsByType(eERType_StaticMeshRenderComponent,0)
           )
    ];
    for (int i = 0; i < 3; i++)
    {
        EERType type;
        switch (i) {
            case 0:
                type = eERType_Brush;
                break;
            case 1:
                type = eERType_Vegetation;
                break;
            case 2:
                type = eERType_StaticMeshRenderComponent;
                break;
            default:
                break;
        }
        for (int j = gEnv->p3DEngine->GetObjectsByType(type, plist) - 1; j >= 0; j--)
        {
            plist[j]->Physicalize(true);
        }
    }
    delete[] plist;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnReloadTerrain()
{
    CErrorsRecorder errRecorder(GetIEditor());
    // Fast export.
    CGameExporter gameExporter;
    gameExporter.Export(eExp_ReloadTerrain);
    // Export does it. GetIEditor()->GetGameEngine()->ReloadLevel();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUndo()
{
    //GetIEditor()->GetObjectManager()->UndoLastOp();
    GetIEditor()->Undo();

    EBUS_EVENT(AzToolsFramework::EditorMetricsEventsBus, Undo);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRedo()
{
    GetIEditor()->Redo();

    EBUS_EVENT(AzToolsFramework::EditorMetricsEventsBus, Redo);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateRedo(QAction* action)
{
    if (GetIEditor()->GetUndoManager()->IsHaveRedo())
    {
        action->setEnabled(true);
    }
    else
    {
        action->setEnabled(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateUndo(QAction* action)
{
    if (GetIEditor()->GetUndoManager()->IsHaveUndo())
    {
        action->setEnabled(true);
    }
    else
    {
        action->setEnabled(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainCollision()
{
    uint32 flags = GetIEditor()->GetDisplaySettings()->GetSettings();
    if (flags & SETTINGS_NOCOLLISION)
    {
        flags &= ~SETTINGS_NOCOLLISION;
    }
    else
    {
        flags |= SETTINGS_NOCOLLISION;
    }
    GetIEditor()->GetDisplaySettings()->SetSettings(flags);
}

void CCryEditApp::OnTerrainCollisionUpdate(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    uint32 flags = GetIEditor()->GetDisplaySettings()->GetSettings();
    action->setChecked(!(flags & SETTINGS_NOCOLLISION));
}

/// Undo command to track entering and leaving Simulation Mode.
class SimulationModeCommand
    : public AzToolsFramework::UndoSystem::URSequencePoint
{
public:
    AZ_CLASS_ALLOCATOR(SimulationModeCommand, AZ::SystemAllocator, 0);
    AZ_RTTI(SimulationModeCommand, "{FB9FB958-5C56-47F6-B168-B5F564F70E69}");

    SimulationModeCommand(const AZStd::string& friendlyName);

    void Undo() override;
    void Redo() override;

    bool Changed() const override { return true; } // State will always have changed.

private:
    void UndoRedo()
    {
        if (ActionManager* actionManager = MainWindow::instance()->GetActionManager())
        {
            if (auto* action = actionManager->GetAction(ID_SWITCH_PHYSICS))
            {
                action->trigger();
            }
        }
    }
};

SimulationModeCommand::SimulationModeCommand(const AZStd::string& friendlyName)
    : URSequencePoint(friendlyName)
{
}

void SimulationModeCommand::Undo()
{
    UndoRedo();
}

void SimulationModeCommand::Redo()
{
    UndoRedo();
}

namespace UndoRedo
{
    static bool IsHappening()
    {
        bool undoRedo = false;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(
            undoRedo, &AzToolsFramework::ToolsApplicationRequests::IsDuringUndoRedo);

        return undoRedo;
    }
} // namespace UndoRedo

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchPhysics()
{
    if (GetIEditor()->GetGameEngine() && !GetIEditor()->GetGameEngine()->GetSimulationMode() && !GetIEditor()->GetGameEngine()->IsLevelLoaded())
    {
        // Don't allow physics to be toggled on if we haven't loaded a level yet
        return;
    }

    QWaitCursor wait;

    AZStd::unique_ptr<AzToolsFramework::ScopedUndoBatch> undoBatch;
    if (!UndoRedo::IsHappening())
    {
        undoBatch = AZStd::make_unique<AzToolsFramework::ScopedUndoBatch>("Switching Physics Simulation");

        auto simulationModeCommand = AZStd::make_unique<SimulationModeCommand>(AZStd::string("Switch Physics"));
        // simulationModeCommand managed by undoBatch
        simulationModeCommand->SetParent(undoBatch->GetUndoBatch());
        simulationModeCommand.release();
    }

    GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_SWITCH_START, 0, 0);

    uint32 flags = GetIEditor()->GetDisplaySettings()->GetSettings();
    if (flags & SETTINGS_PHYSICS)
    {
        flags &= ~SETTINGS_PHYSICS;
    }
    else
    {
        flags |= SETTINGS_PHYSICS;
    }

    GetIEditor()->GetDisplaySettings()->SetSettings(flags);

    if ((flags & SETTINGS_PHYSICS) == 0)
    {
        GetIEditor()->GetGameEngine()->SetSimulationMode(false);
        GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED, 0, 0);
    }
    else
    {
        GetIEditor()->GetGameEngine()->SetSimulationMode(true);
        GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED, 1, 0);
    }

    GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_SWITCH_END, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchPhysicsUpdate(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(!m_bIsExportingLegacyData && GetIEditor()->GetGameEngine()->GetSimulationMode());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSyncPlayer()
{
    GetIEditor()->GetGameEngine()->SyncPlayerPosition(!GetIEditor()->GetGameEngine()->IsSyncPlayerPosition());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSyncPlayerUpdate(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(!GetIEditor()->GetGameEngine()->IsSyncPlayerPosition());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGenerateCgfThumbnails()
{
    qApp->setOverrideCursor(Qt::BusyCursor);
    CThumbnailGenerator gen;
    gen.GenerateForDirectory("Objects\\");
    qApp->restoreOverrideCursor();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerateAll()
{
    CErrorsRecorder errRecorder(GetIEditor());
    // Do game export
    CGameExporter gameExporter;
    gameExporter.Export();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerateTriangulation()
{
    CErrorsRecorder errRecorder(GetIEditor());
    GetIEditor()->GetGameEngine()->GenerateAiTriangulation();
    // Do game export
    CGameExporter gameExporter;
    gameExporter.Export();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerateWaypoint()
{
    CErrorsRecorder errRecorder(GetIEditor());
    GetIEditor()->GetGameEngine()->GenerateAiWaypoint();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerateFlightNavigation()
{
    CErrorsRecorder errRecorder(GetIEditor());
    GetIEditor()->GetGameEngine()->GenerateAiFlightNavigation();
    // Do game export.
    CGameExporter gameExporter;
    gameExporter.Export();
}
//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerate3dvolumes()
{
    CErrorsRecorder errRecorder(GetIEditor());
    GetIEditor()->GetGameEngine()->GenerateAiNavVolumes();
    // Do game export.
    CGameExporter gameExporter;
    gameExporter.Export();
}
//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIValidateNavigation()
{
    CErrorsRecorder errRecorder(GetIEditor());
    GetIEditor()->GetGameEngine()->ValidateAINavigation();
}
//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerate3DDebugVoxels()
{
    CErrorsRecorder errRecorder(GetIEditor());
    GetIEditor()->GetGameEngine()->Generate3DDebugVoxels();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIClearAllNavigation()
{
    CErrorsRecorder errRecorder(GetIEditor());

    GetIEditor()->GetGameEngine()->ClearAllAINavigation();

    // Do game export with empty nav data
    CGameExporter gameExporter;
    gameExporter.Export();

    GetIEditor()->GetGameEngine()->LoadAINavigationData();
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerateSpawners()
{
    GenerateSpawners();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAIGenerateCoverSurfaces()
{
    CErrorsRecorder recorder(GetIEditor());
    GetIEditor()->GetGameEngine()->GenerateAICoverSurfaces();

    // Do game export
    CGameExporter gameExporter;
    gameExporter.Export(eExp_CoverSurfaces);

    if (!GetIEditor()->GetErrorReport()->IsEmpty())
    {
        GetIEditor()->GetErrorReport()->Display();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationShowAreas()
{
    CAIManager* pAIMgr = GetIEditor()->GetAI();
    pAIMgr->ShowNavigationAreas(!gSettings.bNavigationShowAreas);
    gSettings.bNavigationShowAreas = pAIMgr->GetShowNavigationAreasState();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationShowAreasUpdate(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(gSettings.bNavigationShowAreas);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationEnableContinuousUpdate()
{
    if (gSettings.bIsSearchFilterActive)
    {
        CryMessageBox("You need to clear the search box before being able to re-enable Continuous Update.",
            "Warning", MB_OK);
        return;
    }
    CAIManager* pAIMgr = GetIEditor()->GetAI();
    pAIMgr->EnableNavigationContinuousUpdate(!gSettings.bNavigationContinuousUpdate);
    gSettings.bNavigationContinuousUpdate = pAIMgr->GetNavigationContinuousUpdateState();
    gSettings.Save();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationEnableContinuousUpdateUpdate(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(gSettings.bNavigationContinuousUpdate);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationNewArea()
{
    OnSelectAxisSnapToAll();

    IEditor* pEditor = GetIEditor();
    pEditor->StartObjectCreation("NavigationArea");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationFullRebuild()
{
    int result = QMessageBox::question(AzToolsFramework::GetActiveWindow(), QObject::tr("MNM Navigation Rebuilding request"), QObject::tr("Are you sure you want to fully rebuild the MNM data? (The operation can take several minutes)"));
    if (result == QMessageBox::Yes)
    {
        CAIManager* pAIMgr = GetIEditor()->GetAI();
        pAIMgr->RebuildAINavigation();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationAddSeed()
{
    IEditor* pEditor = GetIEditor();
    pEditor->StartObjectCreation("NavigationSeedPoint");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnVisualizeNavigationAccessibility()
{
    CAIManager* pAIMgr = GetIEditor()->GetAI();
    gSettings.bVisualizeNavigationAccessibility = pAIMgr->CalculateAndToggleVisualizationNavigationAccessibility();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnVisualizeNavigationAccessibilityUpdate(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(gSettings.bVisualizeNavigationAccessibility);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAINavigationDisplayAgent()
{
    CAIManager* pAIMgr = GetIEditor()->GetAI();
    pAIMgr->EnableNavigationDebugDisplay(!gSettings.bNavigationDebugDisplay);
    gSettings.bNavigationDebugDisplay = pAIMgr->GetNavigationDebugDisplayState();
}

void CCryEditApp::OnUpdateCurrentLayer(QAction* action)
{
    if (auto currentLayer = GetIEditor()->GetObjectManager()->GetLayersManager()->GetCurrentLayer())
    {
        QString text = QStringLiteral(" ") + currentLayer->GetName();
        if (currentLayer->IsModified())
        {
            text += QStringLiteral("*");
        }
        action->setText(text);
    }
    else
    {
        action->setText(QString());
    }
}

void CCryEditApp::OnUpdateNonGameMode(QAction* action)
{
    action->setEnabled(!GetIEditor()->IsInGameMode());
}

void CCryEditApp::OnUpdateNewLevel(QAction* action)
{
    action->setEnabled(!m_bIsExportingLegacyData);
}

void CCryEditApp::OnUpdatePlayGame(QAction* action)
{
    action->setEnabled(!m_bIsExportingLegacyData && GetIEditor()->IsLevelLoaded());
}

//////////////////////////////////////////////////////////////////////////
CCryEditApp::ECreateLevelResult CCryEditApp::CreateLevel(const QString& levelName, int resolution, int unitSize, bool bUseTerrain, QString& fullyQualifiedLevelName /* ={} */, const TerrainTextureExportSettings& terrainTextureSettings)
{
    // If we are creating a new level and we're in simulate mode, then switch it off before we do anything else
    if (GetIEditor()->GetGameEngine() && GetIEditor()->GetGameEngine()->GetSimulationMode())
    {
        // Preserve the modified flag, we don't want this switch of physics to change that flag
        bool bIsDocModified = GetIEditor()->GetDocument()->IsModified();
        OnSwitchPhysics();
        GetIEditor()->GetDocument()->SetModifiedFlag(bIsDocModified);
    }

    const QScopedValueRollback<bool> rollback(m_creatingNewLevel);
    m_creatingNewLevel = true;
    GetIEditor()->Notify(eNotify_OnBeginCreate);
    CrySystemEventBus::Broadcast(&CrySystemEventBus::Events::OnCryEditorBeginCreate);

    QString currentLevel = GetIEditor()->GetLevelFolder();
    if (!currentLevel.isEmpty())
    {
        GetIEditor()->GetSystem()->GetIPak()->ClosePacks(currentLevel.toUtf8().data());
    }

    QString cryFileName = levelName.mid(levelName.lastIndexOf('/') + 1, levelName.length() - levelName.lastIndexOf('/') + 1);
    QString levelPath = QStringLiteral("%1/Levels/%2/").arg(Path::GetEditingGameDataFolder().c_str(), levelName);
    fullyQualifiedLevelName = levelPath + cryFileName + defaultFileExtension;

    //_MAX_PATH includes null terminator, so we actually want to cap at _MAX_PATH-1
    if (fullyQualifiedLevelName.length() >= _MAX_PATH-1)
    {
        GetIEditor()->Notify(eNotify_OnEndCreate);
        return ECLR_MAX_PATH_EXCEEDED;
    }

    // Does the directory already exist ?
    if (QFileInfo(levelPath).exists())
    {
        GetIEditor()->Notify(eNotify_OnEndCreate);
        return ECLR_ALREADY_EXISTS;
    }

    // Create the directory
    CLogFile::WriteLine("Creating level directory");
    if (!CFileUtil::CreatePath(levelPath))
    {
        GetIEditor()->Notify(eNotify_OnEndCreate);
        return ECLR_DIR_CREATION_FAILED;
    }

    if (GetIEditor()->GetDocument()->IsDocumentReady())
    {
        m_pDocManager->OnFileNew();
    }

    ICVar* sv_map = gEnv->pConsole->GetCVar("sv_map");
    if (sv_map)
    {
        sv_map->Set(levelName.toUtf8().data());
    }


    GetIEditor()->GetDocument()->InitEmptyLevel(resolution, unitSize, bUseTerrain);

    GetIEditor()->SetStatusText("Creating Level...");

#ifdef LY_TERRAIN_EDITOR
    if (bUseTerrain)
    {
        GetIEditor()->GetTerrainManager()->SetTerrainSize(resolution, unitSize);
    }
#endif //#ifdef LY_TERRAIN_EDITOR

    // Save the document to this folder
    GetIEditor()->GetDocument()->SetPathName(fullyQualifiedLevelName);
    GetIEditor()->GetGameEngine()->SetLevelPath(levelPath);

    if (GetIEditor()->GetDocument()->Save())
    {
        m_bIsExportingLegacyData = true;
        CGameExporter gameExporter;
        gameExporter.Export();
        m_bIsExportingLegacyData = false;

        GetIEditor()->GetGameEngine()->LoadLevel(GetIEditor()->GetGameEngine()->GetMissionName(), true, true);
        GetIEditor()->GetSystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_PRECACHE_START, 0, 0);
        if (GetIEditor()->GetGameEngine()->GetIEditorGame())
        {
            GetIEditor()->GetGameEngine()->GetIEditorGame()->OnAfterLevelLoad(GetIEditor()->GetGameEngine()->GetLevelName().toUtf8(), GetIEditor()->GetGameEngine()->GetLevelPath().toUtf8().data());
        }

#ifdef LY_TERRAIN_EDITOR
        GetIEditor()->GetHeightmap()->InitTerrain();
#endif //#ifdef LY_TERRAIN_EDITOR

        // During normal level load flow, the player is hidden after the player is spawned.
        // When creating a new level, the player is not spawned until later, after that hide player command was initially sent.
        // This hide player in level creation ensures that the player is hidden after it is created.
        if (GetIEditor()->GetGameEngine()->GetIEditorGame())
        {
            GetIEditor()->GetGameEngine()->GetIEditorGame()->HidePlayer(true);
        }

        //GetIEditor()->GetGameEngine()->LoadAINavigationData();
        if (!bUseTerrain)
        {
            XmlNodeRef m_node = GetIEditor()->GetDocument()->GetEnvironmentTemplate();
            if (m_node)
            {
                XmlNodeRef envState = m_node->findChild("EnvState");
                if (envState)
                {
                    XmlNodeRef showTerrainSurface = envState->findChild("ShowTerrainSurface");
                    showTerrainSurface->setAttr("value", "false");
                    GetIEditor()->GetGameEngine()->ReloadEnvironment();
                }
            }
        }
        else
        {
            // we need to reload environment after terrain creation in order for water to be initialized
            GetIEditor()->GetGameEngine()->ReloadEnvironment();
        }

        GetIEditor()->GetSystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_PRECACHE_END, 0, 0);
    }

#ifdef LY_TERRAIN_EDITOR
    if (bUseTerrain)
    {
        GenerateTerrainTexture(terrainTextureSettings);
    }
    else
#endif //#ifdef LY_TERRAIN_EDITOR
    {
        // No terrain, but still need to export default octree and visarea data.
        CGameExporter gameExporter;
        gameExporter.Export(eExp_CoverSurfaces | eExp_ReloadTerrain | eExp_SurfaceTexture, eLittleEndian, ".");
    }

    GetIEditor()->GetDocument()->CreateDefaultLevelAssets(resolution, unitSize);
    GetIEditor()->GetDocument()->SetDocumentReady(true);
    GetIEditor()->SetStatusText("Ready");

    // At the end of the creating level process, add this level to the MRU list
    CCryEditApp::instance()->AddToRecentFileList(fullyQualifiedLevelName);

    //Automatically add the legacy terrain component if available.
    if (bUseTerrain)
    {
        AddLegacyTerrainLevelComponent();
    }

    GetIEditor()->Notify(eNotify_OnEndCreate);
    CrySystemEventBus::Broadcast(&CrySystemEventBus::Events::OnCryEditorEndCreate);
    return ECLR_OK;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCreateLevel()
{
    if (m_creatingNewLevel)
    {
        return;
    }
    bool wasCreateLevelOperationCancelled = false;
    bool isNewLevelCreationSuccess = false;
    // This will show the new level dialog until a valid input has been entered by the user or until the user click cancel
    while (!isNewLevelCreationSuccess && !wasCreateLevelOperationCancelled)
    {
        wasCreateLevelOperationCancelled = false;
        isNewLevelCreationSuccess = CreateLevel(wasCreateLevelOperationCancelled);
    }
}

//////////////////////////////////////////////////////////////////////////
bool CCryEditApp::CreateLevel(bool& wasCreateLevelOperationCancelled)
{
    BOOL bIsDocModified = GetIEditor()->GetDocument()->IsModified();
    if (GetIEditor()->GetDocument()->IsDocumentReady() && bIsDocModified)
    {
        QString str = QObject::tr("Level %1 has been changed. Save Level?").arg(GetIEditor()->GetGameEngine()->GetLevelName());
        int result = QMessageBox::question(AzToolsFramework::GetActiveWindow(), QObject::tr("Save Level"), str, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (QMessageBox::Yes == result)
        {
            if (!GetIEditor()->GetDocument()->DoFileSave())
            {
                // if the file save operation failed, assume that the user was informed of why
                // already and treat it as a cancel
                wasCreateLevelOperationCancelled = true;
                return false;
            }

            bIsDocModified = false;
        }
        else if (QMessageBox::No == result)
        {
            // Set Modified flag to false to prevent show Save unchanged dialog again
            GetIEditor()->GetDocument()->SetModifiedFlag(false);
        }
        else if (QMessageBox::Cancel == result)
        {
            wasCreateLevelOperationCancelled = true;
            return false;
        }
    }

    const char* temporaryLevelName = GetIEditor()->GetDocument()->GetTemporaryLevelName();

    CNewLevelDialog dlg;
    dlg.m_level = "";

    if (dlg.exec() != QDialog::Accepted)
    {
        wasCreateLevelOperationCancelled = true;
        GetIEditor()->GetDocument()->SetModifiedFlag(bIsDocModified);
        return false;
    }

    if (!GetIEditor()->GetLevelIndependentFileMan()->PromptChangedFiles())
    {
        return false;
    }

    QString levelNameWithPath = dlg.GetLevel();
    QString levelName = levelNameWithPath.mid(levelNameWithPath.lastIndexOf('/') + 1);

    int resolution = dlg.GetTerrainResolution();
    int unitSize = dlg.GetTerrainUnits();

    bool bUseTerrain = CanAddLegacyTerrainLevelComponent();

    if (levelName == temporaryLevelName && GetIEditor()->GetLevelName() != temporaryLevelName)
    {
        GetIEditor()->GetDocument()->DeleteTemporaryLevel();
    }

    if (levelName.length() == 0 || !CryStringUtils::IsValidFileName(levelName.toUtf8().data()))
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Level name is invalid, please choose another name."));
        return false;
    }

    //Verify that we are not using the temporary level name
    if (QString::compare(levelName, temporaryLevelName) == 0)
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Please enter a level name that is different from the temporary name."));
        return false;
    }

    // We're about to start creating a level, so start recording errors to display at the end.
    GetIEditor()->StartLevelErrorReportRecording();

    QString fullyQualifiedLevelName;
    ECreateLevelResult result = CreateLevel(levelNameWithPath, resolution, unitSize, bUseTerrain, 
                                            fullyQualifiedLevelName, TerrainTextureExportSettings(TerrainTextureExportTechnique::UseDefault));

    if (result == ECLR_ALREADY_EXISTS)
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Level with this name already exists, please choose another name."));
        return false;
    }
    else if (result == ECLR_DIR_CREATION_FAILED)
    {
        QString szLevelRoot = QStringLiteral("%1\\Levels\\%2").arg(Path::GetEditingGameDataFolder().c_str(), levelName);

        QByteArray windowsErrorMessage(ERROR_LEN, 0);
        QByteArray cwd(ERROR_LEN, 0);
        DWORD dw = GetLastError();

#ifdef WIN32
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            windowsErrorMessage.data(),
            windowsErrorMessage.length(), NULL);
        _getcwd(cwd.data(), cwd.length());
#else
        windowsErrorMessage = strerror(dw);
        cwd = QDir::currentPath().toUtf8();
#endif

        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Failed to create level directory: %1\n Error: %2\nCurrent Path: %3").arg(szLevelRoot, windowsErrorMessage, cwd));
        return false;
    }
    else if (result == ECLR_MAX_PATH_EXCEEDED)
    {
        QFileInfo info(fullyQualifiedLevelName);
        const AZStd::string rawProjectDirectory = Path::GetEditingGameDataFolder();
        const QString projectDirectory = QDir::toNativeSeparators(QString::fromUtf8(rawProjectDirectory.data(), rawProjectDirectory.size()));
        const QString elidedLevelName = QStringLiteral("%1...%2").arg(levelName.left(10)).arg(levelName.right(10));
        const QString elidedLevelFileName = QStringLiteral("%1...%2").arg(info.fileName().left(10)).arg(info.fileName().right(10));
        const QString message = QObject::tr(
            "The fully-qualified path for the new level exceeds the maximum supported path length of %1 characters (it's %2 characters long). Please choose a smaller name.\n\n"
            "The fully-qualified path is made up of the project folder (\"%3\", %4 characters), the \"Levels\" sub-folder, a folder named for the level (\"%5\", %6 characters) and the level file (\"%7\", %8 characters), plus necessary separators.\n\n"
            "Please also note that on most platforms, individual components of the path (folder/file names can't exceed  approximately 255 characters)\n\n"
            "Click \"Copy to Clipboard\" to copy the fully-qualified name and close this message.")
            .arg(_MAX_PATH - 1).arg(fullyQualifiedLevelName.size())
            .arg(projectDirectory).arg(projectDirectory.size())
            .arg(elidedLevelName).arg(levelName.size())
            .arg(elidedLevelFileName).arg(info.fileName().size());
        QMessageBox messageBox(QMessageBox::Critical, QString(), message, QMessageBox::Ok, AzToolsFramework::GetActiveWindow());
        QPushButton* copyButton = messageBox.addButton(QObject::tr("Copy to Clipboard"), QMessageBox::ActionRole);
        QObject::connect(copyButton, &QPushButton::pressed, this, [fullyQualifiedLevelName]() { QGuiApplication::clipboard()->setText(fullyQualifiedLevelName); });
        messageBox.exec();
        return false;
    }

    // force the level being rendered at least once
    m_bForceProcessIdle = true;

    m_levelErrorsHaveBeenDisplayed = false;

    return true;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCreateSlice()
{
    QMessageBox::warning(AzToolsFramework::GetActiveWindow(), "Not implemented", "New Slice is not yet implemented.");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenLevel()
{
    CLevelFileDialog levelFileDialog(true);

    levelFileDialog.show();
    levelFileDialog.adjustSize();

    if (levelFileDialog.exec() == QDialog::Accepted)
    {
        OpenDocumentFile(levelFileDialog.GetFileName().toUtf8().data());
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnReloadLevel()
{
    if (auto* doc = GetIEditor()->GetDocument())
    {
        OpenDocumentFileWrapper([&]{
            if (doc->SaveModified())
            {
                // Actually open the level file
                doc->OnOpenDocument(doc->GetLevelPathName());
            }
        });
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenSlice()
{
    QString fileName = QFileDialog::getOpenFileName(MainWindow::instance(),
        tr("Open Slice"),
        Path::GetEditingGameDataFolder().c_str(),
        tr("Slice (*.slice)"));

    if (!fileName.isEmpty())
    {
        OpenDocumentFile(fileName.toUtf8().data());
    }
}

//////////////////////////////////////////////////////////////////////////
CCryEditDoc* CCryEditApp::OpenDocumentFile(LPCTSTR lpszFileName)
{
    return OpenDocumentFileWrapper([&]{
        m_pDocManager->OpenDocumentFile(lpszFileName, true);
    });
}

template<class DoOpen>
inline CCryEditDoc* CCryEditApp::OpenDocumentFileWrapper(DoOpen&& doOpen)
{
    if (m_openingLevel)
    {
        return GetIEditor()->GetDocument();
    }

    // If we are loading and we're in simulate mode, then switch it off before we do anything else
    if (GetIEditor()->GetGameEngine() && GetIEditor()->GetGameEngine()->GetSimulationMode())
    {
        // Preserve the modified flag, we don't want this switch of physics to change that flag
        bool bIsDocModified = GetIEditor()->GetDocument()->IsModified();
        OnSwitchPhysics();
        GetIEditor()->GetDocument()->SetModifiedFlag(bIsDocModified);
    }

    // We're about to start loading a level, so start recording errors to display at the end.
    GetIEditor()->StartLevelErrorReportRecording();

    const QScopedValueRollback<bool> rollback(m_openingLevel, true);

    MainWindow::instance()->menuBar()->setEnabled(false);

    CCryEditDoc* doc = nullptr;
    bool bVisible = false;
    bool bTriggerConsole = false;

    doc = GetIEditor()->GetDocument();
    bVisible = GetIEditor()->ShowConsole(true);
    bTriggerConsole = true;

    if (GetIEditor()->GetLevelIndependentFileMan()->PromptChangedFiles())
    {
        SandboxEditor::StartupTraceHandler openDocTraceHandler;
        openDocTraceHandler.StartCollection();
        if (m_bAutotestMode)
        {
            openDocTraceHandler.SetShowWindow(false);
        }

        // in this case, we set bAddToMRU to always be true because adding files to the MRU list
        // automatically culls duplicate and normalizes paths anyway
        doOpen();

        if (openDocTraceHandler.HasAnyErrors())
        {
            doc->SetHasErrors();
        }
    }

    if (bTriggerConsole)
    {
        GetIEditor()->ShowConsole(bVisible);
    }
    LoadTagLocations();

    MainWindow::instance()->menuBar()->setEnabled(true);

    if (doc->GetEditMode() == CCryEditDoc::DocumentEditingMode::SliceEdit)
    {
        // center camera on entities in slice
        if (ActionManager* actionManager = MainWindow::instance()->GetActionManager())
        {
            GetIEditor()->GetUndoManager()->Suspend();
            actionManager->GetAction(ID_EDIT_SELECTALL)->trigger();
            actionManager->GetAction(ID_GOTO_SELECTED)->trigger();
            actionManager->GetAction(ID_EDIT_SELECTNONE)->trigger();
            GetIEditor()->GetUndoManager()->Resume();
        }
    }

    QTimer::singleShot(0, this, [this] {
        // Need to do the proper adjustments when loading a level that was originally
        // created with Legacy Terrain.
        bool editorWillClose = false;
        AddLegacyTerrainLevelComponentIfNecessary(editorWillClose);

        // If the legacy UI is disabled, and we have legacy entities in our level,
        // then we need to prompt the user with the option to convert their entities
        if (!editorWillClose && !GetIEditor()->IsLegacyUIEnabled() && (FindNumLegacyEntities() > 0))
        {
            ConvertLegacyEntities();
        }
    });

    m_levelErrorsHaveBeenDisplayed = false;

    return doc; // the API wants a CDocument* to be returned. It seems not to be used, though, in our current state.
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnLayerSelect()
{
    if (!GetIEditor()->GetDocument()->IsDocumentReady())
    {
        return;
    }

    QPoint point = QCursor::pos();
    CLayersSelectDialog dlg(point);
    dlg.SetSelectedLayer(GetIEditor()->GetObjectManager()->GetLayersManager()->GetCurrentLayer()->GetName());
    if (dlg.exec() == QDialog::Accepted)
    {
        CUndo undo("Set Current Layer");
        CObjectLayer* pLayer = GetIEditor()->GetObjectManager()->GetLayersManager()->FindLayerByName(dlg.GetSelectedLayer());
        if (pLayer)
        {
            GetIEditor()->GetObjectManager()->GetLayersManager()->SetCurrentLayer(pLayer);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnResourcesReduceworkingset()
{
#ifdef WIN32 // no such thing on macOS
    SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
#endif
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsEquipPacksEdit()
{
    CEquipPackDialog Dlg;
    Dlg.exec();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsUpdateProcVegetation()
{
#ifdef LY_TERRAIN_EDITOR
    GetIEditor()->GetTerrainManager()->ReloadSurfaceTypes();
#endif //#ifdef LY_TERRAIN_EDITOR

    CVegetationMap* pVegetationMap = GetIEditor()->GetVegetationMap();
    if (pVegetationMap)
    {
        pVegetationMap->SetEngineObjectsParams();
    }

    GetIEditor()->SetModifiedFlag();
#ifdef LY_TERRAIN_EDITOR
    GetIEditor()->SetModifiedModule(eModifiedTerrain);
#endif
}

//////////////////////////////////////////////////////////////////////////

void CCryEditApp::OnToggleSelection(bool hide)
{
    CSelectionGroup* sel = GetIEditor()->GetSelection();
    if (!sel->IsEmpty())
    {
        AzToolsFramework::ScopedUndoBatch undo(hide ? "Hide Entity" : "Show Entity");
        for (int i = 0; i < sel->GetCount(); i++)
        {
            // Duplicated object names can exist in the case of prefab objects so passing a name as a script parameter and processing it couldn't be exact.
            GetIEditor()->GetObjectManager()->HideObject(sel->GetObject(i), hide);
        }
    }

}

void CCryEditApp::OnEditHide()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        OnToggleSelection(true);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditHide(QAction* action)
{
    CSelectionGroup* sel = GetIEditor()->GetSelection();
    if (!sel->IsEmpty())
    {
        action->setEnabled(true);
    }
    else
    {
        action->setEnabled(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditShowLastHidden()
{
    AzToolsFramework::ScopedUndoBatch undo("Show Last Hidden Entity");
    GetIEditor()->GetObjectManager()->ShowLastHiddenObject();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditUnhideall()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        if (QMessageBox::question(
            AzToolsFramework::GetActiveWindow(), QObject::tr("Unhide All"),
            QObject::tr("Are you sure you want to unhide all the objects?"),
            QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes)
        {
            // Unhide all.
            AzToolsFramework::ScopedUndoBatch undo("Unhide all Entities");
            GetIEditor()->GetObjectManager()->UnhideAll();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditFreeze()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        // Freeze selection.
        CSelectionGroup* sel = GetIEditor()->GetSelection();
        if (!sel->IsEmpty())
        {
            AzToolsFramework::ScopedUndoBatch undo("Lock Selected Entities");

            // We need to iterate over the list of selected objects in reverse order
            // because when the objects are locked, they are removed from the
            // selection so you would end up with the last selected object not
            // being locked
            int numSelected = sel->GetCount();
            for (int i = numSelected - 1; i >= 0; --i)
            {
                // Duplicated object names can exist in the case of prefab objects so passing a name as a script parameter and processing it couldn't be exact.
                sel->GetObject(i)->SetFrozen(true);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditFreeze(QAction* action)
{
    OnUpdateEditHide(action);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditUnfreezeall()
{
    if (!GetIEditor()->IsNewViewportInteractionModelEnabled())
    {
        if (QMessageBox::question(
            AzToolsFramework::GetActiveWindow(), QObject::tr("Unlock All"),
            QObject::tr("Are you sure you want to unlock all the objects?"),
            QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes)
        {
            // Unfreeze all.
            AzToolsFramework::ScopedUndoBatch undo("Unlock all Entities");
            GetIEditor()->GetObjectManager()->UnfreezeAll();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSnap()
{
    // Switch current snap to grid state.
    bool bGridEnabled = gSettings.pGrid->IsEnabled();
    gSettings.pGrid->Enable(!bGridEnabled);
}

void CCryEditApp::OnWireframe()
{
    int             nWireframe(R_SOLID_MODE);
    ICVar*      r_wireframe(gEnv->pConsole->GetCVar("r_wireframe"));

    if (r_wireframe)
    {
        nWireframe = r_wireframe->GetIVal();
    }

    if (nWireframe != R_WIREFRAME_MODE)
    {
        nWireframe = R_WIREFRAME_MODE;
    }
    else
    {
        nWireframe = R_SOLID_MODE;
    }

    if (r_wireframe)
    {
        r_wireframe->Set(nWireframe);
    }
}

void CCryEditApp::OnUpdateWireframe(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    int             nWireframe(R_SOLID_MODE);
    ICVar*      r_wireframe(gEnv->pConsole->GetCVar("r_wireframe"));

    if (r_wireframe)
    {
        nWireframe = r_wireframe->GetIVal();
    }

    action->setChecked(nWireframe == R_WIREFRAME_MODE);
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnViewGridsettings()
{
    CGridSettingsDialog dlg;
    dlg.exec();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnViewConfigureLayout()
{
    if (GetIEditor()->IsInGameMode())
    {
        // you may not change your viewports while game mode is running.
        CryLog("You may not change viewport configuration while in game mode.");
        return;
    }
    CLayoutWnd* layout = GetIEditor()->GetViewManager()->GetLayout();
    if (layout)
    {
        CLayoutConfigDialog dlg;
        dlg.SetLayout(layout->GetLayout());
        if (dlg.exec() == QDialog::Accepted)
        {
            // Will kill this Pane. so must be last line in this function.
            layout->CreateLayout(dlg.GetLayout());
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::TagLocation(int index)
{
    CViewport* pRenderViewport = GetIEditor()->GetViewManager()->GetGameViewport();
    if (!pRenderViewport)
    {
        return;
    }

    Vec3 vPosVec = pRenderViewport->GetViewTM().GetTranslation();

    m_tagLocations[index - 1] = vPosVec;
    m_tagAngles[index - 1] = Ang3::GetAnglesXYZ(Matrix33(pRenderViewport->GetViewTM()));

    QString sTagConsoleText("");
    sTagConsoleText = tr("Camera Tag Point %1 set to the position: x=%2, y=%3, z=%4 ").arg(index).arg(vPosVec.x, 0, 'f', 2).arg(vPosVec.y, 0, 'f', 2).arg(vPosVec.z, 0, 'f', 2);

    GetIEditor()->WriteToConsole(sTagConsoleText.toUtf8().data());

    if (gSettings.bAutoSaveTagPoints)
    {
        SaveTagLocations();
    }
}

void CCryEditApp::SaveTagLocations()
{
    // Save to file.
    QString filename = QFileInfo(GetIEditor()->GetDocument()->GetLevelPathName()).dir().absoluteFilePath("tags.txt");
    QFile f(filename);
    if (f.open(QFile::WriteOnly))
    {
        QTextStream stream(&f);
        for (int i = 0; i < 12; i++)
        {
            stream <<
                m_tagLocations[i].x << "," << m_tagLocations[i].y << "," <<  m_tagLocations[i].z << "," <<
                m_tagAngles[i].x << "," << m_tagAngles[i].y << "," << m_tagAngles[i].z << Qt::endl;
        }
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::GotoTagLocation(int index)
{
    QString sTagConsoleText("");
    Vec3 pos = m_tagLocations[index - 1];

    if (!IsVectorsEqual(m_tagLocations[index - 1], Vec3(0, 0, 0)))
    {
        // Change render viewport view TM to the stored one.
        CViewport* pRenderViewport = GetIEditor()->GetViewManager()->GetGameViewport();
        if (pRenderViewport)
        {
            Matrix34 tm = Matrix34::CreateRotationXYZ(m_tagAngles[index - 1]);
            tm.SetTranslation(pos);
            pRenderViewport->SetViewTM(tm);
            Vec3 vPosVec(tm.GetTranslation());

            GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_BEAM_PLAYER_TO_CAMERA_POS, (UINT_PTR)&tm, 0);

            sTagConsoleText = tr("Moved Camera To Tag Point %1 (x=%2, y=%3, z=%4)").arg(index).arg(vPosVec.x, 0, 'f', 2).arg(vPosVec.y, 0, 'f', 2).arg(vPosVec.z, 0, 'f', 2);
        }
    }
    else
    {
        sTagConsoleText = tr("Camera Tag Point %1 not set").arg(index);
    }

    if (!sTagConsoleText.isEmpty())
    {
        GetIEditor()->WriteToConsole(sTagConsoleText.toUtf8().data());
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::LoadTagLocations()
{
    QString filename = QFileInfo(GetIEditor()->GetDocument()->GetLevelPathName()).dir().absoluteFilePath("tags.txt");
    // Load tag locations from file.

    ZeroStruct(m_tagLocations);

    QFile f(filename);
    if (f.open(QFile::ReadOnly))
    {
        QTextStream stream(&f);
        for (int i = 0; i < 12; i++)
        {
            QStringList line = stream.readLine().split(",");
            float x = 0, y = 0, z = 0, ax = 0, ay = 0, az = 0;
            if (line.count() == 6)
            {
                x = line[0].toFloat();
                y = line[1].toFloat();
                z = line[2].toFloat();
                ax = line[3].toFloat();
                ay = line[4].toFloat();
                az = line[5].toFloat();
            }

            m_tagLocations[i] = Vec3(x, y, z);
            m_tagAngles[i] = Ang3(ax, ay, az);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsLogMemoryUsage()
{
    gEnv->pConsole->ExecuteString("SaveLevelStats");
    //GetIEditor()->GetHeightmap()->LogLayerSizes();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTagLocation1() { TagLocation(1); }
void CCryEditApp::OnTagLocation2() { TagLocation(2); }
void CCryEditApp::OnTagLocation3() { TagLocation(3); }
void CCryEditApp::OnTagLocation4() { TagLocation(4); }
void CCryEditApp::OnTagLocation5() { TagLocation(5); }
void CCryEditApp::OnTagLocation6() { TagLocation(6); }
void CCryEditApp::OnTagLocation7() { TagLocation(7); }
void CCryEditApp::OnTagLocation8() { TagLocation(8); }
void CCryEditApp::OnTagLocation9() { TagLocation(9); }
void CCryEditApp::OnTagLocation10() { TagLocation(10); }
void CCryEditApp::OnTagLocation11() { TagLocation(11); }
void CCryEditApp::OnTagLocation12() { TagLocation(12); }


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGotoLocation1() { GotoTagLocation(1); }
void CCryEditApp::OnGotoLocation2() { GotoTagLocation(2); }
void CCryEditApp::OnGotoLocation3() { GotoTagLocation(3); }
void CCryEditApp::OnGotoLocation4() { GotoTagLocation(4); }
void CCryEditApp::OnGotoLocation5() { GotoTagLocation(5); }
void CCryEditApp::OnGotoLocation6() { GotoTagLocation(6); }
void CCryEditApp::OnGotoLocation7() { GotoTagLocation(7); }
void CCryEditApp::OnGotoLocation8() { GotoTagLocation(8); }
void CCryEditApp::OnGotoLocation9() { GotoTagLocation(9); }
void CCryEditApp::OnGotoLocation10() { GotoTagLocation(10); }
void CCryEditApp::OnGotoLocation11() { GotoTagLocation(11); }
void CCryEditApp::OnGotoLocation12() { GotoTagLocation(12); }

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainExportblock()
{
#ifdef LY_TERRAIN_EDITOR
    // TODO: Add your command handler code here
    char szFilters[] = "Terrain Block files (*.trb);;All files (*)";
    QString filename;
    if (CFileUtil::SelectSaveFile(szFilters, "trb", {}, filename))
    {
        QWaitCursor wait;
        AABB box;
        GetIEditor()->GetSelectedRegion(box);

        QPoint p1 = GetIEditor()->GetHeightmap()->WorldToHmap(box.min);
        QPoint p2 = GetIEditor()->GetHeightmap()->WorldToHmap(box.max);
        QRect rect(p1, p2 - QPoint(1, 1));

        CXmlArchive ar("Root");
        GetIEditor()->GetHeightmap()->ExportBlock(rect, ar);

        // Save selected objects.
        CSelectionGroup* sel = GetIEditor()->GetSelection();
        XmlNodeRef objRoot = ar.root->newChild("Objects");
        CObjectArchive objAr(GetIEditor()->GetObjectManager(), objRoot, false);
        // Save all objects to XML.
        for (int i = 0; i < sel->GetCount(); i++)
        {
            CBaseObject* obj = sel->GetObject(i);
            objAr.node = objRoot->newChild("Object");
            obj->Serialize(objAr);
        }

        ar.Save(filename);
    }
#endif //#ifdef LY_TERRAIN_EDITOR
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainImportblock()
{
#ifdef LY_TERRAIN_EDITOR
    // TODO: Add your command handler code here
    char szFilters[] = "Terrain Block files (*.trb);;All files (*)";
    QString filename;
    if (CFileUtil::SelectFile(szFilters, "", filename))
    {
        QWaitCursor wait;
        CXmlArchive* ar = new CXmlArchive;
        if (!ar->Load(filename))
        {
            QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QObject::tr("Warning"), QObject::tr("Loading of Terrain Block file failed"));
            delete ar;
            return;
        }

        CErrorsRecorder errorsRecorder(GetIEditor());

        // Import terrain area.
        CUndo undo("Import Terrain Area");

        CHeightmap* pHeightmap = GetIEditor()->GetHeightmap();
        pHeightmap->ImportBlock(*ar, QPoint(0, 0), false);
        // Load selection from archive.
        XmlNodeRef objRoot = ar->root->findChild("Objects");
        if (objRoot)
        {
            GetIEditor()->ClearSelection();
            CObjectArchive ar(GetIEditor()->GetObjectManager(), objRoot, true);
            GetIEditor()->GetObjectManager()->LoadObjects(ar, true);
        }

        delete ar;
        ar = 0;

        /*
        // Archive will be deleted within Move tool.
        CTerrainMoveTool *mt = new CTerrainMoveTool;
        mt->SetArchive( ar );
        GetIEditor()->SetEditTool( mt );
        */
    }
#endif //#ifdef LY_TERRAIN_EDITOR
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateTerrainExportblock(QAction* action)
{
#ifdef LY_TERRAIN_EDITOR
    AABB box;
    GetIEditor()->GetSelectedRegion(box);
    action->setEnabled(!m_bIsExportingLegacyData && !box.IsEmpty());
#endif
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateTerrainImportblock(QAction* action)
{
#ifdef LY_TERRAIN_EDITOR
    action->setEnabled(!m_bIsExportingLegacyData);
#endif
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCustomizeKeyboard()
{
    MainWindow::instance()->OnCustomizeToolbar();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsConfiguretools()
{
    ToolsConfigDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        MainWindow::instance()->UpdateToolsMenu();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsScriptHelp()
{
    CScriptHelpDialog::GetInstance()->show();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnExportIndoors()
{
    //CBrushIndoor *indoor = GetIEditor()->GetObjectManager()->GetCurrentIndoor();
    //CBrushExporter exp;
    //exp.Export( indoor, "C:\\MasterCD\\Objects\\Indoor.bld" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnViewCycle2dviewport()
{
    GetIEditor()->GetViewManager()->Cycle2DViewport();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnDisplayGotoPosition()
{
    CGotoPositionDlg dlg;
    dlg.exec();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnDisplaySetVector()
{
    CSetVectorDlg dlg;
    dlg.exec();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSnapangle()
{
    gSettings.pGrid->EnableAngleSnap(!gSettings.pGrid->IsAngleSnapEnabled());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSnapangle(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(gSettings.pGrid->IsAngleSnapEnabled());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRuler()
{
    CRuler* pRuler = GetIEditor()->GetRuler();
    pRuler->SetActive(!pRuler->IsActive());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateRuler(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    action->setChecked(GetIEditor()->GetRuler()->IsActive());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionXaxis()
{
    CUndo undo("Rotate X");
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    pSelection->Rotate(Ang3(m_fastRotateAngle, 0, 0), GetIEditor()->GetReferenceCoordSys());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionYaxis()
{
    CUndo undo("Rotate Y");
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    pSelection->Rotate(Ang3(0, m_fastRotateAngle, 0), GetIEditor()->GetReferenceCoordSys());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionZaxis()
{
    CUndo undo("Rotate Z");
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    pSelection->Rotate(Ang3(0, 0, m_fastRotateAngle), GetIEditor()->GetReferenceCoordSys());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionRotateangle()
{
    bool ok = false;
    int fractionalDigitCount = 5;
    float angle = aznumeric_caster(QInputDialog::getDouble(AzToolsFramework::GetActiveWindow(), QObject::tr("Rotate Angle"), QStringLiteral(""), m_fastRotateAngle, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max(), fractionalDigitCount, &ok));
    if (ok)
    {
        m_fastRotateAngle = angle;
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnConvertselectionTobrushes()
{
    std::vector<CBaseObjectPtr> objects;
    // Convert every possible object in selection to the brush.
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    for (int i = 0; i < pSelection->GetCount(); i++)
    {
        objects.push_back(pSelection->GetObject(i));
    }

    bool isFail = false;
    for (int i = 0; i < objects.size(); i++)
    {
        if (!GetIEditor()->GetObjectManager()->ConvertToType(objects[i], "Brush"))
        {
            gEnv->pLog->LogError("Object %s can't be converted to the brush type.", objects[i]->GetName().toUtf8().constData());
            isFail = true;
        }
    }

    if (isFail)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Some of the selected objects can't be converted to the brush type."));
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnConvertselectionTosimpleentity()
{
    std::vector<CBaseObjectPtr> objects;
    // Convert every possible object in selection to the brush.
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    for (int i = 0; i < pSelection->GetCount(); i++)
    {
        objects.push_back(pSelection->GetObject(i));
    }

    bool isFail = false;
    for (int i = 0; i < objects.size(); i++)
    {
        if (!GetIEditor()->GetObjectManager()->ConvertToType(objects[i], "GeomEntity"))
        {
            gEnv->pLog->LogError("Object %s can't be converted to the simple entity type.", objects[i]->GetName().toUtf8().constData());
            isFail = true;
        }
    }

    if (isFail)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Some of the selected objects can't be converted to the simple entity type."));
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnConvertselectionToGameVolume()
{
    std::vector<CBaseObjectPtr> objects;
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    for (int i = 0; i < pSelection->GetCount(); i++)
    {
        objects.push_back(pSelection->GetObject(i));
    }

    bool isFail = false;
    for (int i = 0; i < objects.size(); i++)
    {
        if (!GetIEditor()->GetObjectManager()->ConvertToType(objects[i], "GameVolume"))
        {
            gEnv->pLog->LogError("Object %s can't be converted to the game volume type.", objects[i]->GetName().toUtf8().constData());
            isFail = true;
        }
    }

    if (isFail)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Some of the selected objects can't be converted to the game volume type."));
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnConvertSelectionToComponentEntity()
{
    using namespace AzToolsFramework;

    std::vector<CBaseObjectPtr> objects;
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    for (int i = 0; i < pSelection->GetCount(); i++)
    {
        objects.push_back(pSelection->GetObject(i));
    }

    CWaitProgress wait("Converting objects...");
    QWaitCursor waitCursor;

    for (int i = 0; i < objects.size(); i++)
    {
        wait.Step((static_cast<float>(i) / static_cast<float>(objects.size())) * 100.f);

        CBaseObjectPtr object = objects[i];
        QString meshFile;

        if (qobject_cast<CBrushObject*>(object.get()))
        {
            CBrushObject* brush = qobject_cast<CBrushObject*>(object.get());
            meshFile = brush->GetGeometryFile();
        }
        else if (qobject_cast<CGeomEntity*>(object.get()))
        {
            CGeomEntity* brush = qobject_cast<CGeomEntity*>(object.get());
            meshFile = brush->GetGeometryFile();
        }
        else
        {
            AZ_Error("Editor", false, "Object %s could not be converted. Only Brushes and GeomEntities are supported sources.", objects[i]->GetName().toUtf8().data());
            continue;
        }

        if (!meshFile.isEmpty())
        {
            // Find the asset Id by path.
            AZ::Data::AssetId meshId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(meshId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath, meshFile.toUtf8().data(), AZ::Data::s_invalidAssetType, false);

            if (meshId.IsValid())
            {
                // If we found it, create a new editor entity and add a mesh component pointing to the asset.
                AZ::EntityId newEntityId;
                EditorEntityContextRequestBus::BroadcastResult(newEntityId, &EditorEntityContextRequests::CreateNewEditorEntity, object->GetName().toUtf8().data());

                if (newEntityId.IsValid())
                {
                    // The editor doesn't have a compile-time dependency on LmbrCentral, so we use the Mesh
                    // Component's Uuid directly. This is perfectly safe, just tough on the eyes.
                    const AZ::Uuid meshComponentId("{FC315B86-3280-4D03-B4F0-5553D7D08432}");

                    EntityCompositionRequests::AddComponentsOutcome outcome = AZ::Failure(AZStd::string("Failed to add MeshComponent"));
                    EntityCompositionRequestBus::BroadcastResult(outcome, &EntityCompositionRequests::AddComponentsToEntities, EntityIdList{ newEntityId }, AZ::ComponentTypeList{ meshComponentId });
                    if (outcome.IsSuccess())
                    {
                        // Assign mesh asset.
                        LmbrCentral::MeshComponentRequestBus::Event(newEntityId, &LmbrCentral::MeshComponentRequests::SetMeshAsset, meshId);

                        // Move entity to same transform.
                        const AZ::Transform transform = LYTransformToAZTransform(object->GetWorldTM());
                        AZ::TransformBus::Event(newEntityId, &AZ::TransformInterface::SetWorldTM, transform);

                        // Delete old object.
                        GetIEditor()->DeleteObject(object);
                    }
                    else
                    {
                        AZ_Error("Editor", false, "Object %s could not be converted because mesh component could not be created.", objects[i]->GetName().toUtf8().data());
                    }
                }
                else
                {
                    AZ_Error("Editor", false, "Object %s could not be converted because component entity could not be created.", objects[i]->GetName().toUtf8().data());
                }
            }
            else
            {
                AZ_Error("Editor", false, "Object %s could not be converted because Id for asset \"%s\" could not be determined.", objects[i]->GetName().toUtf8().data(), meshFile.toUtf8().data());
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditRenameobject()
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    if (pSelection->IsEmpty())
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("No Selected Objects!"));
        return;
    }

    IObjectManager* pObjMan = GetIEditor()->GetObjectManager();

    if (!pObjMan)
    {
        return;
    }

    StringDlg dlg(QObject::tr("Rename Object(s)"));
    if (dlg.exec() == QDialog::Accepted)
    {
        CUndo undo("Rename Objects");
        QString newName;
        QString str = dlg.GetString();
        int num = 0;
        bool bWarningShown = false;

        for (int i = 0; i < pSelection->GetCount(); ++i)
        {
            CBaseObject* pObject = pSelection->GetObject(i);

            if (pObject)
            {
                if (pObjMan->IsDuplicateObjectName(str))
                {
                    pObjMan->ShowDuplicationMsgWarning(pObject, str, true);
                    return;
                }
            }
        }

        for (int i = 0; i < pSelection->GetCount(); ++i)
        {
            newName = QStringLiteral("%1%2").arg(str).arg(num);
            ++num;
            CBaseObject* pObject = pSelection->GetObject(i);

            if (pObject)
            {
                pObjMan->ChangeObjectName(pObject, newName);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangemovespeedIncrease()
{
    gSettings.cameraMoveSpeed += m_moveSpeedStep;
    if (gSettings.cameraMoveSpeed < 0.01f)
    {
        gSettings.cameraMoveSpeed = 0.01f;
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangemovespeedDecrease()
{
    gSettings.cameraMoveSpeed -= m_moveSpeedStep;
    if (gSettings.cameraMoveSpeed < 0.01f)
    {
        gSettings.cameraMoveSpeed = 0.01f;
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangemovespeedChangestep()
{
    bool ok = false;
    int fractionalDigitCount = 5;
    float step = aznumeric_caster(QInputDialog::getDouble(AzToolsFramework::GetActiveWindow(), QObject::tr("Change Move Increase/Decrease Step"), QStringLiteral(""), m_moveSpeedStep, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max(), fractionalDigitCount, &ok));
    if (ok)
    {
        m_moveSpeedStep = step;
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnModifyAipointPicklink()
{
    // Special command to emulate pressing pick button in ai point.
    CBaseObject* pObject = GetIEditor()->GetSelectedObject();
    if (pObject && qobject_cast<CAIPoint*>(pObject))
    {
        ((CAIPoint*)pObject)->StartPick();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnModifyAipointPickImpasslink()
{
    // Special command to emulate pressing pick impass button in ai point.
    CBaseObject* pObject = GetIEditor()->GetSelectedObject();
    if (pObject && qobject_cast<CAIPoint*>(pObject))
    {
        ((CAIPoint*)pObject)->StartPickImpass();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileSavelevelresources()
{
    CGameResourcesExporter saver;
    saver.GatherAllLoadedResources();
    saver.ChooseDirectoryAndSave();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnClearRegistryData()
{
    if (QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Clear all sandbox registry data ?"),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        QSettings settings;
        settings.clear();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnValidatelevel()
{
    // TODO: Add your command handler code here
    CLevelInfo levelInfo;
    levelInfo.Validate();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnValidateObjectPositions()
{
    IObjectManager* objMan = GetIEditor()->GetObjectManager();

    if (!objMan)
    {
        return;
    }

    CErrorReport errorReport;
    errorReport.SetCurrentFile("");
    errorReport.SetImmediateMode(false);

    int objCount = objMan->GetObjectCount();
    AABB bbox1;
    AABB bbox2;
    int bugNo = 0;
    QString statTxt("");

    std::vector<CBaseObject*> objects;
    objMan->GetObjects(objects);

    std::vector<CBaseObject*> foundObjects;

    std::vector<GUID> objIDs;
    bool reportVeg = false;

    for (int i1 = 0; i1 < objCount; ++i1)
    {
        CBaseObject* pObj1 = objects[i1];

        if (!pObj1)
        {
            continue;
        }

        // Ignore groups in search
        if (pObj1->GetType() == OBJTYPE_GROUP)
        {
            continue;
        }

        // Object must have geometry
        if (!pObj1->GetGeometry())
        {
            continue;
        }

        // Ignore solids
        if (pObj1->GetType() == OBJTYPE_SOLID)
        {
            continue;
        }

        pObj1->GetBoundBox(bbox1);

        // Check if object has vegetation inside its bbox
        CVegetationMap* pVegetationMap = GetIEditor()->GetVegetationMap();
        if (pVegetationMap && !pVegetationMap->IsAreaEmpty(bbox1))
        {
            CErrorRecord error;
            error.pObject = pObj1;
            error.count = bugNo;
            error.error = tr("%1 has vegetation object(s) inside").arg(pObj1->GetName());
            error.description = "Vegetation left inside an object";
            errorReport.ReportError(error);
        }

        // Check if object has other objects inside its bbox
        foundObjects.clear();
        objMan->FindObjectsInAABB(bbox1, foundObjects);

        for (int i2 = 0; i2 < foundObjects.size(); ++i2)
        {
            CBaseObject* pObj2 = objects[i2];
            if (!pObj2)
            {
                continue;
            }

            if (pObj2->GetId() == pObj1->GetId())
            {
                continue;
            }

            if (pObj2->GetParent())
            {
                continue;
            }

            if (pObj2->GetType() == OBJTYPE_SOLID)
            {
                continue;
            }

            if (stl::find(objIDs, pObj2->GetId()))
            {
                continue;
            }

            if ((!pObj2->GetGeometry() || pObj2->GetType() == OBJTYPE_SOLID) && (pObj2->GetType()))
            {
                continue;
            }

            pObj2->GetBoundBox(bbox2);

            if (!bbox1.IsContainPoint(bbox2.max))
            {
                continue;
            }

            if (!bbox1.IsContainPoint(bbox2.min))
            {
                continue;
            }

            objIDs.push_back(pObj2->GetId());

            CErrorRecord error;
            error.pObject = pObj2;
            error.count = bugNo;
            error.error = tr("%1 inside %2 object").arg(pObj2->GetName(), pObj1->GetName());
            error.description = "Object left inside other object";
            errorReport.ReportError(error);
            ++bugNo;
        }

        statTxt = tr("%1/%2 [Reported Objects: %3]").arg(i1).arg(objCount).arg(bugNo);
        GetIEditor()->SetStatusText(statTxt);
    }

    if (errorReport.GetErrorCount() == 0)
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("No Errors Found"));
    }
    else
    {
        errorReport.Display();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainResizeterrain()
{
#ifdef LY_TERRAIN_EDITOR
    if (!GetIEditor()->GetDocument()->IsDocumentReady())
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("Please wait until previous operation will be finished."));
        return;
    }

    CHeightmap* pHeightmap = GetIEditor()->GetHeightmap();

    CNewLevelDialog dlg(AzToolsFramework::GetActiveWindow());
    dlg.SetTerrainResolution(pHeightmap->GetWidth());
    dlg.SetTerrainUnits(pHeightmap->GetUnitSize());
    dlg.IsResize(true);
    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }

    pHeightmap->ClearModSectors();

    CUndoManager* undoMgr = GetIEditor()->GetUndoManager();
    if (undoMgr)
    {
        undoMgr->Flush();
    }

    int resolution = dlg.GetTerrainResolution();
    int unitSize = dlg.GetTerrainUnits();

    if (resolution != pHeightmap->GetWidth() || unitSize != pHeightmap->GetUnitSize())
    {
        pHeightmap->Resize(resolution, resolution, unitSize, false);
        UserExportToGame(TerrainTextureExportSettings(TerrainTextureExportTechnique::PromptUser), false);
    }

    GetIEditor()->Notify(eNotify_OnTerrainRebuild);
#endif //#ifdef LY_TERRAIN_EDITOR
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateTerrainResizeterrain(QAction* action)
{
#ifdef LY_TERRAIN_EDITOR
    action->setEnabled(GetIEditor()->GetDocument() && GetIEditor()->GetDocument()->IsDocumentReady());
#endif
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsPreferences()
{
    EditorPreferencesDialog dlg(MainWindow::instance());
    dlg.exec();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGraphicsSettings()
{
    GraphicsSettingsDialog dlg(MainWindow::instance());
    dlg.exec();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPrefabsMakeFromSelection()
{
    GetIEditor()->GetPrefabManager()->MakeFromSelection();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdatePrefabsMakeFromSelection(QAction* action)
{
    bool bInvalidSelection = false;
    CSelectionGroup* pSel = GetIEditor()->GetSelection();
    for (int i = 0; i < pSel->GetCount(); i++)
    {
        CBaseObject* pObj = pSel->GetObject(i);
        if (pObj && qobject_cast<CPrefabObject*>(pObj) || pObj->GetGroup())
        {
            bInvalidSelection = true;
            break;
        }
    }
    action->setEnabled((!bInvalidSelection && pSel->GetCount() > 0));
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAddSelectionToPrefab()
{
    GetIEditor()->GetPrefabManager()->AddSelectionToPrefab();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateAddSelectionToPrefab(QAction* action)
{
    int nPrefabsFound = 0;
    CSelectionGroup* pEditorSelection = GetIEditor()->GetSelection();
    for (int i = 0; i < pEditorSelection->GetCount(); i++)
    {
        CBaseObject* pObj = pEditorSelection->GetObject(i);
        if (qobject_cast<CPrefabObject*>(pObj))
        {
            nPrefabsFound++;
        }
    }
    action->setEnabled((nPrefabsFound == 1 && pEditorSelection->GetCount() > 1));
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloneSelectionFromPrefab()
{
    std::vector<CBaseObject*> selectedObjects;
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();

    for (int i = 0, count = pSelection->GetCount(); i < count; i++)
    {
        selectedObjects.push_back(pSelection->GetObject(i));
    }

    if (selectedObjects.empty())
    {
        return;
    }

    GetIEditor()->GetPrefabManager()->CloneObjectsFromPrefabs(selectedObjects);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateCloneSelectionFromPrefab(QAction* action)
{
    int nValidObjectsSelected = 0;
    CSelectionGroup* pEditorSelection = GetIEditor()->GetSelection();
    if (pEditorSelection->GetCount())
    {
        for (int i = 0; i < pEditorSelection->GetCount(); i++)
        {
            if (CBaseObject* pObject = pEditorSelection->GetObject(i))
            {
                if (qobject_cast<CPrefabObject*>(pObject->GetParent()))
                {
                    nValidObjectsSelected++;
                }
            }
        }
    }
    action->setEnabled((nValidObjectsSelected == pEditorSelection->GetCount() && pEditorSelection->GetCount() > 0));
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnExtractSelectionFromPrefab()
{
    std::vector<CBaseObject*> selectedObjects;
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();

    for (int i = 0, count = pSelection->GetCount(); i < count; i++)
    {
        selectedObjects.push_back(pSelection->GetObject(i));
    }

    if (selectedObjects.empty())
    {
        return;
    }

    GetIEditor()->GetPrefabManager()->ExtractObjectsFromPrefabs(selectedObjects);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateExtractSelectionFromPrefab(QAction* action)
{
    int nValidObjectsSelected = 0;
    CSelectionGroup* pEditorSelection = GetIEditor()->GetSelection();
    if (pEditorSelection->GetCount())
    {
        for (int i = 0; i < pEditorSelection->GetCount(); i++)
        {
            if (CBaseObject* pObject = pEditorSelection->GetObject(i))
            {
                if (qobject_cast<CPrefabObject*>(pObject->GetParent()))
                {
                    nValidObjectsSelected++;
                }
            }
        }
    }
    action->setEnabled((nValidObjectsSelected == pEditorSelection->GetCount() && pEditorSelection->GetCount() > 0));
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPrefabsOpenAll()
{
    std::vector<CPrefabObject*> prefabObjects;
    GetIEditor()->GetPrefabManager()->GetPrefabObjects(prefabObjects);
    GetIEditor()->GetPrefabManager()->OpenPrefabs(prefabObjects, "Open all prefab objects");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPrefabsCloseAll()
{
    std::vector<CPrefabObject*> prefabObjects;
    GetIEditor()->GetPrefabManager()->GetPrefabObjects(prefabObjects);
    GetIEditor()->GetPrefabManager()->ClosePrefabs(prefabObjects, "Close all prefab objects");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPrefabsRefreshAll()
{
    GetIEditor()->GetObjectManager()->SendEvent(EVENT_PREFAB_REMAKE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolterrainmodifySmooth()
{
    GetIEditor()->GetCommandManager()->Execute("edit_tool.terrain_flatten");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainmodifySmooth()
{
    GetIEditor()->GetCommandManager()->Execute("edit_tool.terrain_smooth");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainVegetation()
{
    GetIEditor()->GetCommandManager()->Execute("edit_tool.vegetation_activate");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainPaintlayers()
{
    GetIEditor()->GetCommandManager()->Execute("edit_tool.terrain_painter_activate");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchToDefaultCamera()
{
    CViewport* vp = GetIEditor()->GetViewManager()->GetSelectedViewport();
    if (CRenderViewport* rvp = viewport_cast<CRenderViewport*>(vp))
    {
        rvp->SetDefaultCamera();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSwitchToDefaultCamera(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    CViewport* pViewport = GetIEditor()->GetViewManager()->GetSelectedViewport();
    if (CRenderViewport* rvp = viewport_cast<CRenderViewport*>(pViewport))
    {
        action->setEnabled(true);
        action->setChecked(rvp->IsDefaultCamera());
    }
    else
    {
        action->setEnabled(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchToSequenceCamera()
{
    CViewport* vp = GetIEditor()->GetViewManager()->GetSelectedViewport();
    if (CRenderViewport* rvp = viewport_cast<CRenderViewport*>(vp))
    {
        rvp->SetSequenceCamera();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSwitchToSequenceCamera(QAction* action)
{
    Q_ASSERT(action->isCheckable());

    CViewport* pViewport = GetIEditor()->GetViewManager()->GetSelectedViewport();

    if (CRenderViewport* rvp = viewport_cast<CRenderViewport*>(pViewport))
    {
        bool enableAction = false;

        // only enable if we're editing a sequence in Track View and have cameras in the level
        if (GetIEditor()->GetAnimation()->GetSequence())
        {

            AZ::EBusAggregateResults<AZ::EntityId> componentCameras;
            Camera::CameraBus::BroadcastResult(componentCameras, &Camera::CameraRequests::GetCameras);

            std::vector<CCameraObject*> legacyCameras;
            ((CObjectManager*)GetIEditor()->GetObjectManager())->GetCameras(legacyCameras);

            const int numCameras = legacyCameras.size() + componentCameras.values.size();

            enableAction = (numCameras > 0);
        }

        action->setEnabled(enableAction);
        action->setChecked(rvp->IsSequenceCamera());
    }
    else
    {
        action->setEnabled(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchToSelectedcamera()
{
    CViewport* vp = GetIEditor()->GetViewManager()->GetSelectedViewport();
    if (CRenderViewport* rvp = viewport_cast<CRenderViewport*>(vp))
    {
        rvp->SetSelectedCamera();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSwitchToSelectedCamera(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    CBaseObject* pObject = GetIEditor()->GetSelectedObject();
    AzToolsFramework::EntityIdList selectedEntityList;
    AzToolsFramework::ToolsApplicationRequests::Bus::BroadcastResult(selectedEntityList, &AzToolsFramework::ToolsApplicationRequests::GetSelectedEntities);
    AZ::EBusAggregateResults<AZ::EntityId> cameras;
    Camera::CameraBus::BroadcastResult(cameras, &Camera::CameraRequests::GetCameras);
    bool isCameraComponentSelected = selectedEntityList.size() > 0 ? AZStd::find(cameras.values.begin(), cameras.values.end(), *selectedEntityList.begin()) != cameras.values.end() : false;

    CViewport* pViewport = GetIEditor()->GetViewManager()->GetSelectedViewport();
    CRenderViewport* rvp = viewport_cast<CRenderViewport*>(pViewport);
    if ((qobject_cast<CCameraObject*>(pObject) || isCameraComponentSelected) && rvp)
    {
        action->setEnabled(true);
        action->setChecked(rvp->IsSelectedCamera());
    }
    else
    {
        action->setEnabled(false);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchcameraNext()
{
    CViewport* vp = GetIEditor()->GetActiveView();
    if (CRenderViewport* rvp = viewport_cast<CRenderViewport*>(vp))
    {
        rvp->CycleCamera();
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMaterialAssigncurrent()
{
    CUndo undo("Assign Material To Selection");
    GetIEditor()->GetMaterialManager()->Command_AssignToSelection();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMaterialResettodefault()
{
    GetIEditor()->GetMaterialManager()->Command_ResetSelection();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMaterialGetmaterial()
{
    GetIEditor()->GetMaterialManager()->Command_SelectFromObject();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenMaterialEditor()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::MaterialEditor);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenMannequinEditor()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::LegacyMannequin);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenCharacterTool()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::LegacyGeppetto);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenDataBaseView()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::DatabaseView);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenAssetBrowserView()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::AssetBrowser);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenTrackView()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::TrackView);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenAudioControlsEditor()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::AudioControlsEditor);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenUICanvasEditor()
{
    QtViewPaneManager::instance()->OpenPane("UI Editor");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenAIDebugger()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::AIDebugger);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenLayerEditor()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::LegacyLayerEditor);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenTerrainEditor()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::TerrainEditor);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSubobjectmodeVertex()
{
    SubObjectModeVertex();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSubobjectmodeEdge()
{
    SubObjectModeEdge();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSubobjectmodeFace()
{
    SubObjectModeFace();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMaterialPicktool()
{
    GetIEditor()->SetEditTool("EditTool.PickMaterial");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsCreate()
{
    StringDlg dlg(QObject::tr("Cloud Name"));
    dlg.SetString(GetIEditor()->GetObjectManager()->GenerateUniqueObjectName( "Cloud" ));
    if (dlg.exec() == QDialog::Accepted)
    {
        GetIEditor()->BeginUndo();

        CCloudGroup* group = (CCloudGroup*)GetIEditor()->NewObject("Cloud");
        if (!group)
        {
            GetIEditor()->CancelUndo();
            return;
        }
        GetIEditor()->GetObjectManager()->ChangeObjectName(group, dlg.GetString());

        CSelectionGroup* selection = GetIEditor()->GetSelection();
        selection->FilterParents();

        int i;
        std::vector<CBaseObjectPtr> objects;
        for (i = 0; i < selection->GetFilteredCount(); i++)
        {
            objects.push_back(selection->GetFilteredObject(i));
        }

        // Snap center to grid.
        Vec3 center = gSettings.pGrid->Snap(selection->GetCenter());
        group->SetPos(center);

        for (i = 0; i < objects.size(); i++)
        {
            GetIEditor()->GetObjectManager()->UnselectObject(objects[i]);
            group->AddMember(objects[i]);
        }
        GetIEditor()->AcceptUndo("Cloud Make");
        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedTerrain);
        GetIEditor()->SelectObject(group);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsDestroy()
{
    // Ungroup all groups in selection.
    CSelectionGroup* sel = GetIEditor()->GetSelection();
    if (!sel->IsEmpty())
    {
        CUndo undo("CloudsDestroy");
        for (int i = 0; i < sel->GetCount(); i++)
        {
            CBaseObject* obj = sel->GetObject(i);
            if (qobject_cast<CCloudGroup*>(obj))
            {
                ((CGroup*)obj)->Ungroup();
                GetIEditor()->DeleteObject(obj);
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateCloudsDestroy(QAction* action)
{
    CBaseObject* obj = GetIEditor()->GetSelectedObject();
    if (obj)
    {
        action->setEnabled(obj->metaObject() == &CCloudGroup::staticMetaObject);
    }
    else
    {
        OnUpdateSelected(action);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsOpen()
{
    // Ungroup all groups in selection.
    CSelectionGroup* sel = GetIEditor()->GetSelection();
    if (!sel->IsEmpty())
    {
        CUndo undo("Clouds Open");
        for (int i = 0; i < sel->GetCount(); i++)
        {
            CBaseObject* obj = sel->GetObject(i);
            if (obj && obj->metaObject() == &CCloudGroup::staticMetaObject)
            {
                ((CGroup*)obj)->Open();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateCloudsOpen(QAction* action)
{
    BOOL bEnable = false;
    CBaseObject* obj = GetIEditor()->GetSelectedObject();
    if (obj)
    {
        if (obj->metaObject() == &CCloudGroup::staticMetaObject)
        {
            if (!((CGroup*)obj)->IsOpen())
            {
                bEnable = true;
            }
        }
        action->setEnabled(bEnable);
    }
    else
    {
        OnUpdateSelected(action);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsClose()
{
    CBaseObject* obj = GetIEditor()->GetSelectedObject();
    if (obj)
    {
        GetIEditor()->BeginUndo();
        ((CGroup*)obj)->Close();
        GetIEditor()->AcceptUndo("Clouds Close");
        GetIEditor()->SetModifiedFlag();
        GetIEditor()->SetModifiedModule(eModifiedTerrain);
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateCloudsClose(QAction* action)
{
    BOOL bEnable = false;
    CBaseObject* obj = GetIEditor()->GetSelectedObject();
    if (obj && obj->metaObject() == &CCloudGroup::staticMetaObject)
    {
        if (((CGroup*)obj)->IsOpen())
        {
            bEnable = true;
        }
    }

    action->setEnabled(bEnable);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTimeOfDay()
{
    GetIEditor()->OpenView("Time Of Day");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnResolveMissingObjects()
{
    GetIEditor()->GetObjectManager()->ResolveMissingObjects();
}

inline namespace Commands
{
    void PySetConfigSpec(int spec, int platform)
    {
        CUndo undo("Set Config Spec");
        if (CUndo::IsRecording())
        {
            CUndo::Record(new CUndoConficSpec());
        }
        GetIEditor()->SetEditorConfigSpec((ESystemConfigSpec)spec, (ESystemConfigPlatform)platform);
    }

    int PyGetConfigSpec()
    {
        return static_cast<int>(GetIEditor()->GetEditorConfigSpec());
    }

    int PyGetConfigPlatform()
    {
        return static_cast<int>(GetIEditor()->GetEditorConfigPlatform());
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangeGameSpec(UINT nID)
{
    switch (nID)
    {
    case ID_GAME_PC_ENABLELOWSPEC:
        Commands::PySetConfigSpec(CONFIG_LOW_SPEC, CONFIG_PC);
        break;
    case ID_GAME_PC_ENABLEMEDIUMSPEC:
        Commands::PySetConfigSpec(CONFIG_MEDIUM_SPEC, CONFIG_PC);
        break;
    case ID_GAME_PC_ENABLEHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_HIGH_SPEC, CONFIG_PC);
        break;
    case ID_GAME_PC_ENABLEVERYHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_VERYHIGH_SPEC, CONFIG_PC);
        break;
    case ID_GAME_OSXMETAL_ENABLELOWSPEC:
        Commands::PySetConfigSpec(CONFIG_LOW_SPEC, CONFIG_OSX_METAL);
        break;
    case ID_GAME_OSXMETAL_ENABLEMEDIUMSPEC:
        Commands::PySetConfigSpec(CONFIG_MEDIUM_SPEC, CONFIG_OSX_METAL);
        break;
    case ID_GAME_OSXMETAL_ENABLEHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_HIGH_SPEC, CONFIG_OSX_METAL);
        break;
    case ID_GAME_OSXMETAL_ENABLEVERYHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_VERYHIGH_SPEC, CONFIG_OSX_METAL);
        break;
    case ID_GAME_ANDROID_ENABLELOWSPEC:
        Commands::PySetConfigSpec(CONFIG_LOW_SPEC, CONFIG_ANDROID);
        break;
    case ID_GAME_ANDROID_ENABLEMEDIUMSPEC:
        Commands::PySetConfigSpec(CONFIG_MEDIUM_SPEC, CONFIG_ANDROID);
        break;
    case ID_GAME_ANDROID_ENABLEHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_HIGH_SPEC, CONFIG_ANDROID);
        break;
    case ID_GAME_ANDROID_ENABLEVERYHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_VERYHIGH_SPEC, CONFIG_ANDROID);
        break;
    case ID_GAME_IOS_ENABLELOWSPEC:
        Commands::PySetConfigSpec(CONFIG_LOW_SPEC, CONFIG_IOS);
        break;
    case ID_GAME_IOS_ENABLEMEDIUMSPEC:
        Commands::PySetConfigSpec(CONFIG_MEDIUM_SPEC, CONFIG_IOS);
        break;
    case ID_GAME_IOS_ENABLEHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_HIGH_SPEC, CONFIG_IOS);
        break;
    case ID_GAME_IOS_ENABLEVERYHIGHSPEC:
        Commands::PySetConfigSpec(CONFIG_VERYHIGH_SPEC, CONFIG_IOS);
        break;
#if defined(AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS)
#define AZ_RESTRICTED_PLATFORM_EXPANSION(CodeName, CODENAME, codename, PrivateName, PRIVATENAME, privatename, PublicName, PUBLICNAME, publicname, PublicAuxName1, PublicAuxName2, PublicAuxName3)\
    case ID_GAME_##CODENAME##_ENABLELOWSPEC:\
        Commands::PySetConfigSpec(CONFIG_LOW_SPEC, CONFIG_##CODENAME);\
        break;\
    case ID_GAME_##CODENAME##_ENABLEMEDIUMSPEC:\
        Commands::PySetConfigSpec(CONFIG_MEDIUM_SPEC, CONFIG_##CODENAME);\
        break;\
    case ID_GAME_##CODENAME##_ENABLEHIGHSPEC:\
        Commands::PySetConfigSpec(CONFIG_HIGH_SPEC, CONFIG_##CODENAME);\
        break;
    AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS
#undef AZ_RESTRICTED_PLATFORM_EXPANSION
#endif
    case ID_GAME_APPLETV_ENABLESPEC:
        Commands::PySetConfigSpec(CONFIG_LOW_SPEC, CONFIG_APPLETV);
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::SetGameSpecCheck(ESystemConfigSpec spec, ESystemConfigPlatform platform, int &nCheck, bool &enable)
{
    if (GetIEditor()->GetEditorConfigSpec() == spec && GetIEditor()->GetEditorConfigPlatform() == platform)
    {
        nCheck = 1;
    }
    enable = spec <= GetIEditor()->GetSystem()->GetMaxConfigSpec();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateGameSpec(QAction* action)
{
    Q_ASSERT(action->isCheckable());
    int nCheck = 0;
    bool enable = true;
    switch (action->data().toInt())
    {
    case ID_GAME_PC_ENABLELOWSPEC:
        SetGameSpecCheck(CONFIG_LOW_SPEC, CONFIG_PC, nCheck, enable);
        break;
    case ID_GAME_PC_ENABLEMEDIUMSPEC:
        SetGameSpecCheck(CONFIG_MEDIUM_SPEC, CONFIG_PC, nCheck, enable);
        break;
    case ID_GAME_PC_ENABLEHIGHSPEC:
        SetGameSpecCheck(CONFIG_HIGH_SPEC, CONFIG_PC, nCheck, enable);
        break;
    case ID_GAME_PC_ENABLEVERYHIGHSPEC:
        SetGameSpecCheck(CONFIG_VERYHIGH_SPEC, CONFIG_PC, nCheck, enable);
        break;
    case ID_GAME_OSXMETAL_ENABLELOWSPEC:
        SetGameSpecCheck(CONFIG_LOW_SPEC, CONFIG_OSX_METAL, nCheck, enable);
        break;
    case ID_GAME_OSXMETAL_ENABLEMEDIUMSPEC:
        SetGameSpecCheck(CONFIG_MEDIUM_SPEC, CONFIG_OSX_METAL, nCheck, enable);
        break;
    case ID_GAME_OSXMETAL_ENABLEHIGHSPEC:
        SetGameSpecCheck(CONFIG_HIGH_SPEC, CONFIG_OSX_METAL, nCheck, enable);
        break;
    case ID_GAME_OSXMETAL_ENABLEVERYHIGHSPEC:
        SetGameSpecCheck(CONFIG_VERYHIGH_SPEC, CONFIG_OSX_METAL, nCheck, enable);
        break;
    case ID_GAME_ANDROID_ENABLELOWSPEC:
        SetGameSpecCheck(CONFIG_LOW_SPEC, CONFIG_ANDROID, nCheck, enable);
        break;
    case ID_GAME_ANDROID_ENABLEMEDIUMSPEC:
        SetGameSpecCheck(CONFIG_MEDIUM_SPEC, CONFIG_ANDROID, nCheck, enable);
        break;
    case ID_GAME_ANDROID_ENABLEHIGHSPEC:
        SetGameSpecCheck(CONFIG_HIGH_SPEC, CONFIG_ANDROID, nCheck, enable);
        break;
    case ID_GAME_ANDROID_ENABLEVERYHIGHSPEC:
        SetGameSpecCheck(CONFIG_VERYHIGH_SPEC, CONFIG_ANDROID, nCheck, enable);
        break;
    case ID_GAME_IOS_ENABLELOWSPEC:
        SetGameSpecCheck(CONFIG_LOW_SPEC, CONFIG_IOS, nCheck, enable);
        break;
    case ID_GAME_IOS_ENABLEMEDIUMSPEC:
        SetGameSpecCheck(CONFIG_MEDIUM_SPEC, CONFIG_IOS, nCheck, enable);
        break;
    case ID_GAME_IOS_ENABLEHIGHSPEC:
        SetGameSpecCheck(CONFIG_HIGH_SPEC, CONFIG_IOS, nCheck, enable);
        break;
    case ID_GAME_IOS_ENABLEVERYHIGHSPEC:
        SetGameSpecCheck(CONFIG_VERYHIGH_SPEC, CONFIG_IOS, nCheck, enable);
        break;
#if defined(AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS)
#define AZ_RESTRICTED_PLATFORM_EXPANSION(CodeName, CODENAME, codename, PrivateName, PRIVATENAME, privatename, PublicName, PUBLICNAME, publicname, PublicAuxName1, PublicAuxName2, PublicAuxName3)\
    case ID_GAME_##CODENAME##_ENABLELOWSPEC:\
        SetGameSpecCheck(CONFIG_LOW_SPEC, CONFIG_##CODENAME, nCheck, enable);\
        break;\
    case ID_GAME_##CODENAME##_ENABLEMEDIUMSPEC:\
        SetGameSpecCheck(CONFIG_MEDIUM_SPEC, CONFIG_##CODENAME, nCheck, enable);\
        break;\
    case ID_GAME_##CODENAME##_ENABLEHIGHSPEC:\
        SetGameSpecCheck(CONFIG_HIGH_SPEC, CONFIG_##CODENAME, nCheck, enable);\
        break;
        AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS
#undef AZ_RESTRICTED_PLATFORM_EXPANSION
#endif
    case ID_GAME_APPLETV_ENABLESPEC:
        SetGameSpecCheck(CONFIG_LOW_SPEC, CONFIG_APPLETV, nCheck, enable);
        break;
    }
    action->setChecked(nCheck);
    action->setEnabled(enable);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGotoViewportSearch()
{
    if (MainWindow::instance())
    {
        CLayoutViewPane* viewPane = MainWindow::instance()->GetActiveView();
        if (viewPane)
        {
            viewPane->SetFocusToViewportSearch();
        }
    }
}

RecentFileList* CCryEditApp::GetRecentFileList()
{
    static RecentFileList list;
    return &list;
};

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::AddToRecentFileList(const QString& lpszPathName)
{
    // In later MFC implementations (WINVER >= 0x0601) files must exist before they can be added to the recent files list.
    // Here we override the new CWinApp::AddToRecentFileList code with the old implementation to remove this requirement.

    if (IsInAutotestMode())
    {
        // Never add to the recent file list when in auto test mode
        // This would cause issues for devs running tests locally impacting their normal workflows/setups
        return;
    }

    if (GetRecentFileList())
    {
        GetRecentFileList()->Add(lpszPathName);
    }

    // write the list immediately so it will be remembered even after a crash
    if (GetRecentFileList())
    {
        GetRecentFileList()->WriteList();
    }
    else
    {
        CLogFile::WriteLine("ERROR: Recent File List is NULL!");
    }
}

//////////////////////////////////////////////////////////////////////////
bool CCryEditApp::IsInRegularEditorMode()
{
    return !IsInTestMode() && !IsInPreviewMode()
           && !IsInExportMode() && !IsInConsoleMode() && !IsInLevelLoadTestMode() && !GetIEditor()->IsInMatEditMode();
}

void CCryEditApp::OnOpenQuickAccessBar()
{
    if (m_pQuickAccessBar == NULL)
    {
        return;
    }

    CEditTool* pEditTool(GetIEditor()->GetEditTool());
    if (pEditTool && pEditTool->IsNeedSpecificBehaviorForSpaceAcce())
    {
        return;
    }

    QRect geo = m_pQuickAccessBar->geometry();
    geo.moveCenter(MainWindow::instance()->geometry().center());
    m_pQuickAccessBar->setGeometry(geo);
    m_pQuickAccessBar->setVisible(true);
    m_pQuickAccessBar->setFocus();
}

void CCryEditApp::SetEditorWindowTitle(QString sTitleStr, QString sPreTitleStr, QString sPostTitleStr)
{
    if (MainWindow::instance() || m_pConsoleDialog)
    {
        QString platform = "";
        const SFileVersion& v = GetIEditor()->GetFileVersion();

#ifdef WIN64
        platform = "[x64]";
#else
        platform = "[x86]";
#endif //WIN64

        if (sTitleStr.isEmpty())
        {
            sTitleStr = QObject::tr("Lumberyard Editor Beta %1 - Build %2").arg(platform).arg(LY_BUILD);
        }

        if (!sPreTitleStr.isEmpty())
        {
            sTitleStr.insert(0, sPreTitleStr);
        }

        if (!sPostTitleStr.isEmpty())
        {
            sTitleStr.insert(sTitleStr.length(), QStringLiteral(" - %1").arg(sPostTitleStr));
        }

        MainWindow::instance()->setWindowTitle(sTitleStr);
        if (m_pConsoleDialog)
        {
            m_pConsoleDialog->setWindowTitle(sTitleStr);
        }
    }
}

namespace
{
    bool g_runScriptResult = false; // true -> success, false -> failure
}

namespace
{
    void PySetResultToSuccess()
    {
        g_runScriptResult = true;
    }

    void PySetResultToFailure()
    {
        g_runScriptResult = false;
    }

    void PyIdleEnable(bool enable)
    {
        if (Editor::EditorQtApplication::instance())
        {
            Editor::EditorQtApplication::instance()->EnableOnIdle(enable);
        }
    }

    bool PyIdleIsEnabled()
    {
        if (!Editor::EditorQtApplication::instance())
        {
            return false;
        }
        return Editor::EditorQtApplication::instance()->OnIdleEnabled();
    }

    void PyIdleWait(double timeInSec)
    {
        const bool wasIdleEnabled = PyIdleIsEnabled();
        if (!wasIdleEnabled)
        {
            PyIdleEnable(true);
        }

        clock_t start = clock();
        do
        {
            QEventLoop loop;
            QTimer::singleShot(timeInSec * 1000, &loop, &QEventLoop::quit);
            loop.exec();
        } while ((double)(clock() - start) / CLOCKS_PER_SEC < timeInSec);

        if (!wasIdleEnabled)
        {
            PyIdleEnable(false);
        }
    }

    void PyIdleWaitFrames(uint32 frames)
    {
        struct Ticker : public AZ::TickBus::Handler
        {
            Ticker(QEventLoop* loop, uint32 targetFrames) : m_loop(loop), m_targetFrames(targetFrames)
            {
                AZ::TickBus::Handler::BusConnect();
            }
            ~Ticker()
            {
                AZ::TickBus::Handler::BusDisconnect();
            }

            void OnTick(float deltaTime, AZ::ScriptTimePoint time) override
            {
                AZ_UNUSED(deltaTime);
                AZ_UNUSED(time);
                if (++m_elapsedFrames == m_targetFrames)
                {
                    m_loop->quit();
                }
            }
            QEventLoop* m_loop = nullptr;
            uint32 m_elapsedFrames = 0;
            uint32 m_targetFrames = 0;
        };

        QEventLoop loop;
        Ticker ticker(&loop, frames);
        loop.exec();
    }
}

bool CCryEditApp::Command_ExportToEngine()
{
    return CCryEditApp::instance()->UserExportToGame(TerrainTextureExportSettings(TerrainTextureExportTechnique::NoExport), false, true);
}


void CCryEditApp::SubObjectModeVertex()
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    if (pSelection->GetCount() != 1)
    {
        return;
    }

    CBaseObject* pSelObject = pSelection->GetObject(0);

    if (pSelObject->GetType() == OBJTYPE_BRUSH)
    {
        GetIEditor()->ExecuteCommand("edit_mode.select_vertex");
    }
    else
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The current selected object(s) do(es)n't support the mesh editing"));
    }
}

void CCryEditApp::SubObjectModeEdge()
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    if (pSelection->GetCount() != 1)
    {
        return;
    }

    CBaseObject* pSelObject = pSelection->GetObject(0);

    if (pSelObject->GetType() == OBJTYPE_BRUSH)
    {
        GetIEditor()->ExecuteCommand("edit_mode.select_edge");
    }
    else
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The current selected object(s) do(es)n't support the mesh editing"));
    }
}

void CCryEditApp::SubObjectModeFace()
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();
    if (pSelection->GetCount() != 1)
    {
        return;
    }

    CBaseObject* pSelObject = pSelection->GetObject(0);

    if (pSelObject->GetType() == OBJTYPE_BRUSH)
    {
        GetIEditor()->ExecuteCommand("edit_mode.select_face");
    }
    else
    {
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The current selected object(s) do(es)n't support the mesh editing"));
    }
}


CMainFrame * CCryEditApp::GetMainFrame() const
{
    return MainWindow::instance()->GetOldMainFrame();
}

void CCryEditApp::OnOpenProjectConfiguratorGems()
{
    bool isProjectExternal = false;
    AzFramework::ApplicationRequests::Bus::BroadcastResult(isProjectExternal, &AzFramework::ApplicationRequests::IsEngineExternal);

    if (isProjectExternal)
    {
        // External project folders is in preview mode, so we cannot use Project Configurator to set it yet.  Once either Project Configurator
        // or any new GUI application supports it, replace the warning below with the launch of the tool to configure GEMS.
        QMessageBox::warning(QApplication::activeWindow(),
                             QString(),
                             QObject::tr(
                                 "Project folders external to the engine is in preview mode.  "
                                 "Configuration of GEMS for external project folders needs to be "
                                 "run from the command tool lmbr.exe"
                             ), QMessageBox::Ok);
    }
    else
    {
        // Add save message prompt to message only if a level has been loaded and modified
        QString saveChanges;
        if (GetIEditor()->GetDocument()->IsDocumentReady() && GetIEditor()->GetDocument()->IsModified())
        {
            saveChanges = "save your changes and ";
        }
        QString message = QObject::tr("You must use the Project Configurator to enable gems for your project. \nDo you want to %1close the editor before continuing to the Project Configurator?").arg(saveChanges);
        ToProjectConfigurator(message, QObject::tr("Editor"), PROJECT_CONFIGURATOR_GEM_PAGE);
    }
}

void CCryEditApp::OnOpenProjectConfiguratorSwitchProject()
{
    OpenProjectConfiguratorSwitchProject();
}

bool CCryEditApp::OpenProjectConfiguratorSwitchProject()
{
    bool isProjectExternal = false;
    AzFramework::ApplicationRequests::Bus::BroadcastResult(isProjectExternal, &AzFramework::ApplicationRequests::IsEngineExternal);

    if (isProjectExternal)
    {
        // External project folders is in preview mode, so we cannot use Project Configurator to set it.
        QMessageBox::warning(QApplication::activeWindow(),
            QString(),
            QObject::tr(
                "The feature to allow project folders external to the engine is in preview mode. You can only switch to "
                "game projects that exist inside the engine folder from within the editor. To switch projects, shutdown "
                "the Editor and the Asset Processor and open your desired project."
            ), QMessageBox::Ok);
        return false;

    }
    // Add save message prompt to message only if a level has been loaded and modified
    QString saveChanges;
    if (GetIEditor()->GetDocument()->IsDocumentReady() && GetIEditor()->GetDocument()->IsModified())
    {
        saveChanges = QObject::tr("\nDo you wish to save changes before closing the Editor?");
    }

    const QString message = QObject::tr("You must use the Project Configurator to set a new default project.\n"
                                        "This will close the Editor and launch the Project Configurator.%1").arg(saveChanges);
    return ToProjectConfigurator(message, QObject::tr("Editor"), "Project Selection");
}

void CCryEditApp::StartProcessDetached(const char* process, const char* args)
{
    // Build the arguments as a QStringList
    AZStd::vector<AZStd::string> tokens;

    // separate the string based on spaces for paths like "-launch", "lua", "-files";
    // also separate the string and keep spaces inside the folder path;
    // Ex: C:\dev\Foundation\dev\Cache\SamplesProject\pc\samplesproject\scripts\components\a a\empty.lua;
    // Ex: C:\dev\Foundation\dev\Cache\SamplesProject\pc\samplesproject\scripts\components\a a\'empty'.lua;
    AZStd::string currentStr(args);
    AZStd::size_t firstQuotePos = AZStd::string::npos;
    AZStd::size_t secondQuotePos = 0;
    AZStd::size_t pos = 0;

    while (!currentStr.empty())
    {
        firstQuotePos = currentStr.find_first_of('\"');
        pos = currentStr.find_first_of(" ");

        if ((firstQuotePos != AZStd::string::npos) && (firstQuotePos < pos || pos == AZStd::string::npos))
        {
            secondQuotePos = currentStr.find_first_of('\"', firstQuotePos + 1);
            if (secondQuotePos == AZStd::string::npos)
            {
                AZ_Warning("StartProcessDetached", false, "String tokenize failed, no matching \" found.");
                return;
            }

            AZStd::string newElement(AZStd::string(currentStr.data() + (firstQuotePos + 1), (secondQuotePos - 1)));
            tokens.push_back(newElement);

            currentStr = currentStr.substr(secondQuotePos + 1);

            firstQuotePos = AZStd::string::npos;
            secondQuotePos = 0;
            continue;
        }
        else
        {
            if (pos != AZStd::string::npos)
            {
                AZStd::string newElement(AZStd::string(currentStr.data() + 0, pos));
                tokens.push_back(newElement);
                currentStr = currentStr.substr(pos + 1);
            }
            else
            {
                tokens.push_back(AZStd::string(currentStr));
                break;
            }
        }
    }

    QStringList argsList;
    for (const auto& arg : tokens)
    {
        argsList.push_back(QString(arg.c_str()));
    }

    // Launch the process
    bool startDetachedReturn = QProcess::startDetached(
        process,
        argsList,
        QCoreApplication::applicationDirPath()
    );
    AZ_Warning("StartProcessDetached", startDetachedReturn, "Failed to start process:%s args:%s", process, args);
}

void CCryEditApp::OpenLUAEditor(const char* files)
{
    AZStd::string args = "-launch lua";
    if (files && strlen(files) > 0)
    {
        AZStd::vector<AZStd::string> resolvedPaths;

        AZStd::vector<AZStd::string> tokens;

        AzFramework::StringFunc::Tokenize(files, tokens, '|');

        for (const auto& file : tokens)
        {
            char resolved[AZ_MAX_PATH_LEN];

            AZStd::string fullPath = Path::GamePathToFullPath(file.c_str()).toUtf8().data();
            azstrncpy(resolved, AZ_MAX_PATH_LEN, fullPath.c_str(), fullPath.size());

            if (AZ::IO::FileIOBase::GetInstance()->Exists(resolved))
            {
                AZStd::string current = '\"' + AZStd::string(resolved) + '\"';
                AZStd::replace(current.begin(), current.end(), '\\', '/');
                resolvedPaths.push_back(current);
            }
        }

        if (!resolvedPaths.empty())
        {
            for (const auto& resolvedPath : resolvedPaths)
            {
                args.append(AZStd::string::format(" -files %s", resolvedPath.c_str()));
            }
        }
    }

    const char* engineRoot = nullptr;
    AzFramework::ApplicationRequests::Bus::BroadcastResult(engineRoot, &AzFramework::ApplicationRequests::GetEngineRoot);
    AZ_Assert(engineRoot != nullptr, "Unable to communicate to AzFramework::ApplicationRequests::Bus");

    const char* appRoot = nullptr;
    AzFramework::ApplicationRequests::Bus::BroadcastResult(appRoot, &AzFramework::ApplicationRequests::GetAppRoot);
    AZ_Assert(appRoot != nullptr, "Unable to communicate to AzFramework::ApplicationRequests::Bus");

    AZStd::string_view binFolderName;
    AZ::ComponentApplicationBus::BroadcastResult(binFolderName, &AZ::ComponentApplicationRequests::GetBinFolder);

#if defined(AZ_PLATFORM_WINDOWS)
    AZStd::string process = AZStd::string::format("\"%s%s" AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING "LuaIDE.exe\"", engineRoot, binFolderName.data());
#else
    AZStd::string process = AZStd::string::format("\"%s%s" AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING "LuaIDE\"", engineRoot, binFolderName.data());
#endif
    AZStd::string processArgs = AZStd::string::format("%s -app-root \"%s\"", args.c_str(), appRoot);
    StartProcessDetached(process.c_str(), processArgs.c_str());
}

bool CCryEditApp::IsProjectConfiguratorRunning() const
{
    FixDanglingSharedMemory("ProjectConfiguratorApp");
    QSharedMemory testMem("ProjectConfiguratorApp");
    bool createSuccess = testMem.create(1, QSharedMemory::AccessMode::ReadOnly);
    if (createSuccess)
    {
        return false;
    }

    if (testMem.error() != QSharedMemory::AlreadyExists)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(),
            QString("Cannot test if ProjectConfigurator is running: Failed to create shared memory. (%0)").arg(testMem.errorString()));
        return false;
    }

    return true;
}

bool CCryEditApp::ToProjectConfigurator(const QString& msg, const QString& caption, const QString& location)
{
    if (ToExternalToolPrompt(msg, caption) &&
        OpenProjectConfigurator(location))
    {
        // Called from a modal dialog with the main window as its parent. Best not to close the main window while the dialog is still active.
        QTimer::singleShot(0, MainWindow::instance(), &MainWindow::close);
        return true;
    }
    return false;
}

void CCryEditApp::OnRefreshAssetDatabases()
{
    CAssetBrowserManager::Instance()->RefreshAssetDatabases();
}

bool CCryEditApp::OpenProjectConfigurator(const QString& startPage, bool restartEditor) const
{
#if defined(Q_OS_WIN)
    const char* projectConfigurator = "ProjectConfigurator.exe";
#elif defined(Q_OS_MACOS)
    const char* projectConfigurator = "ProjectConfigurator";
#elif defined(Q_OS_LINUX)
    //KDAB_TODO verify it
    const char* projectConfigurator = "ProjectConfigurator";
#else
#error Unsupported Platform for Project Configurator
#endif

    AzToolsFramework::ToolsApplicationRequests::ResolveToolPathOutcome result = AZ::Failure(AZStd::string("No responders have been installed to execute to ResolveConfigToolsPath"));
    AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(result, &AzToolsFramework::ToolsApplicationRequests::ResolveConfigToolsPath, projectConfigurator);

    if (!result.IsSuccess())
    {
        QMessageBox::warning(QApplication::activeWindow(),
            QString(),
            QObject::tr("Unable to find Project Configurator executable (%1). Please make sure it's installed or built and try again.")
            .arg(projectConfigurator));
        return false;
    }

    QStringList appArguments = { "-s", startPage, "-i", QString::number(GetCurrentProcessId()) };
    if (restartEditor)
    {
        appArguments.append( { "-e", "1" } );
    }

    QString projectConfiguratorPath(QString::fromUtf8(result.GetValue().c_str()));
    bool success = QProcess::startDetached(
        projectConfiguratorPath,
        appArguments);

    if (!success)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The Project Configurator executable (%1) could not be found. Please ensure that it is installed in:\n%2\nand try again.").arg(projectConfigurator, projectConfiguratorPath));
    }

    return success;
}

bool CCryEditApp::OpenSetupAssistant() const
{
#if defined(Q_OS_WIN)
    const char* setupAssistant = "SetupAssistant.exe";
#elif defined(Q_OS_MACOS)
    const char* setupAssistant = "SetupAssistant";
#elif defined(Q_OS_LINUX)
    //KDAB_TODO verify it
    const char* setupAssistant = "SetupAssistant";
#else
#error Unsupported Platform for Project Configurator
#endif

    AzToolsFramework::ToolsApplicationRequests::ResolveToolPathOutcome result = AZ::Failure(AZStd::string("No responders have been installed to execute to ResolveConfigToolsPath"));
    AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(result, &AzToolsFramework::ToolsApplicationRequests::ResolveConfigToolsPath, setupAssistant);

    if (!result.IsSuccess())
    {
        QMessageBox::warning(QApplication::activeWindow(),
            QString(),
            QObject::tr("Unable to find SetupAssistant executable (%1). Please make sure its installed or built and try again.")
            .arg(setupAssistant));
        return false;
    }

    QString setupAssistantPath(QString::fromUtf8(result.GetValue().c_str()));
    if (QProcess::startDetached(setupAssistantPath, {}) == false)
    {
        QMessageBox::warning(AzToolsFramework::GetActiveWindow(), QString(), QObject::tr("The Setup Assistant executable failed to launch. Please ensure that it is installed in:\n%1\nand try again.").arg(setupAssistantPath));
        return false;
    }

    return true;
}

QString CCryEditApp::GetRootEnginePath() const
{
    return m_rootEnginePath;
}

bool CCryEditApp::ToExternalToolPrompt(const QString& msg, const QString& caption)
{
    bool askToSave = GetIEditor()->GetDocument()->IsDocumentReady() && GetIEditor()->GetDocument()->IsModified();
    QMessageBox::StandardButtons buttons = QMessageBox::Cancel;
    auto action = askToSave ? (QMessageBox::Save | QMessageBox::Discard) : QMessageBox::Ok;
    buttons |= action;

    const QMessageBox::StandardButtons result = QMessageBox::warning(AzToolsFramework::GetActiveWindow(), caption, msg, buttons);
    if (result & QMessageBox::Save)
    {
        return (GetIEditor()->GetGameEngine()->IsLevelLoaded() &&
                GetIEditor()->GetDocument()->Save()) ||
                GetIEditor()->GetDocument()->CanCloseFrame();
    }
    else if (result & QMessageBox::Discard)
    {
        GetIEditor()->GetDocument()->SetModifiedFlag(false);
    }
    return  result & action;
}


void CCryEditApp::OnError(AzFramework::AssetSystem::AssetSystemErrors error)
{
    AZStd::string errorMessage = "";

    switch (error)
    {
    case AzFramework::AssetSystem::ASSETSYSTEM_FAILED_TO_LAUNCH_ASSETPROCESSOR:
        errorMessage = AZStd::string::format("Failed to start the Asset Processor.\r\nPlease make sure that AssetProcessor.exe is available in the same folder the Editor is in.\r\n");
        break;
    case AzFramework::AssetSystem::ASSETSYSTEM_FAILED_TO_CONNECT_TO_ASSETPROCESSOR:
        errorMessage = AZStd::string::format("Failed to connect to the Asset Processor.\r\nPlease make sure that AssetProcessor.exe is available in the same folder the Editor is in and another copy is not already running somewhere else.\r\n");
        break;
    }

    CryMessageBox(errorMessage.c_str(), "Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

void CCryEditApp::OnOpenProceduralMaterialEditor()
{
    QtViewPaneManager::instance()->OpenPane(LyViewPane::SubstanceEditor);
}


#if defined(AZ_PLATFORM_WINDOWS)
//Due to some laptops not autoswitching to the discrete gpu correctly we are adding these
//dllspecs as defined in the amd and nvidia white papers to 'force on' the use of the
//discrete chips.  This will be overriden by users setting application profiles
//and may not work on older drivers or bios. In theory this should be enough to always force on
//the discrete chips.

//http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
//https://community.amd.com/thread/169965

// It is unclear if this is also needed for linux or osx at this time(22/02/2017)
extern "C"
{
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}
#endif

#ifdef Q_OS_WIN
#pragma comment(lib, "Shell32.lib")
#endif

int SANDBOX_API CryEditMain(int argc, char* argv[])
{
#if !defined(LEGACYALLOCATOR_MODULE_STATIC)
    AZ_Assert(!AZ::AllocatorInstance<AZ::LegacyAllocator>::IsReady(), "Expected allocator to not be initialized, hunt down the static that is initializing it");
    AZ::AllocatorInstance<AZ::LegacyAllocator>::Create();
#endif
    AZ_Assert(!AZ::AllocatorInstance<CryStringAllocator>::IsReady(), "Expected allocator to not be initialized, hunt down the static that is initializing it");
    AZ::AllocatorInstance<CryStringAllocator>::Create();

    CCryEditApp* theApp = new CCryEditApp();
    // this does some magic to set the current directory...
    {
        QCoreApplication app(argc, argv);
        CCryEditApp::InitDirectory();
    }

    // Be very careful with any work done in this scope. There will be no way to show
    // any errors or warnings to the user via GUI until after the QApplication(EditorQtApplication)
    // is created below.
    AzQtComponents::PrepareQtPaths();

    // Must be set before QApplication is initialized, so that we support HighDpi monitors, like the Retina displays
    // on Windows 10
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // QtOpenGL attributes and surface format setup.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setSwapInterval(0);
#ifdef AZ_DEBUG_BUILD
    format.setOption(QSurfaceFormat::DebugContext);
#endif
    QSurfaceFormat::setDefaultFormat(format);

    Editor::EditorQtApplication::InstallQtLogHandler();

    AzQtComponents::Utilities::HandleDpiAwareness(AzQtComponents::Utilities::SystemDpiAware);
    Editor::EditorQtApplication app(argc, argv);

    // Hook the trace bus to catch errors, boot the AZ app after the QApplication is up
    int ret = 0;

    // open a scope to contain the AZToolsApp instance;
    {
        EditorInternal::EditorToolsApplication AZToolsApp(&argc, &argv);

        if (!AZToolsApp.Start())
        {
            return -1;
        }

        AzToolsFramework::EditorEvents::Bus::Broadcast(&AzToolsFramework::EditorEvents::NotifyQtApplicationAvailable, &app);

    #if defined(AZ_PLATFORM_MAC)
        // Native menu bars do not work on macOS due to all the tool dialogs
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    #endif

        int exitCode = 0;

        BOOL didCryEditStart = CCryEditApp::instance()->InitInstance();
        AZ_Error("Editor", didCryEditStart, "CryEditor did not initialize correctly, and will close."
            "\nThis could be because of incorrectly configured components, or missing required gems."
            "\nSee other errors for more details.");

        if (didCryEditStart)
        {
            app.EnableOnIdle();

            ret = app.exec();
        }
        else
        {
            exitCode = 1;
        }

        CCryEditApp::instance()->ExitInstance(exitCode);

    }

    delete theApp;

#if !defined(LEGACYALLOCATOR_MODULE_STATIC)
    AZ::AllocatorInstance<AZ::LegacyAllocator>::Destroy();
#endif

    return ret;
}

namespace
{
    void PyStartProcessDetached(const char* process, const char* args)
    {
        CCryEditApp::instance()->StartProcessDetached(process, args);
    }

    void PyLaunchLUAEditor(const char* files)
    {
        CCryEditApp::instance()->OpenLUAEditor(files);
    }

    bool PyCheckOutDialogEnableForAll(bool isEnable)
    {
        return CCheckOutDialog::EnableForAll(isEnable);
    }
}

namespace AzToolsFramework
{
    void CryEditPythonHandler::Reflect(AZ::ReflectContext* context)
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
            addLegacyGeneral(behaviorContext->Method("open_level", PyOpenLevel, nullptr, "Opens a level."));
            addLegacyGeneral(behaviorContext->Method("open_level_no_prompt", PyOpenLevelNoPrompt, nullptr, "Opens a level. Doesn't prompt user about saving a modified level."));
            addLegacyGeneral(behaviorContext->Method("reload_current_level", PyReloadCurrentLevel, nullptr, "Re-loads the current level. If no level is loaded, then does nothing."));
            addLegacyGeneral(behaviorContext->Method("create_level", PyCreateLevel, nullptr, "Creates a level with the parameters of 'levelName', 'resolution', 'unitSize' and 'bUseTerrain'."));
            addLegacyGeneral(behaviorContext->Method("create_level_no_prompt", PyCreateLevelNoPrompt, nullptr, "Creates a level with the parameters of 'levelName', 'resolution', 'unitSize' and 'bUseTerrain'."));
            addLegacyGeneral(behaviorContext->Method("get_game_folder", PyGetGameFolderAsString, nullptr, "Gets the path to the Game folder of current project."));
            addLegacyGeneral(behaviorContext->Method("get_current_level_name", PyGetCurrentLevelName, nullptr, "Gets the name of the current level."));
            addLegacyGeneral(behaviorContext->Method("get_current_level_path", PyGetCurrentLevelPath, nullptr, "Gets the fully specified path of the current level."));

            addLegacyGeneral(behaviorContext->Method("load_all_plugins", Command_LoadPlugins, nullptr, "Loads all available plugins."));
            addLegacyGeneral(behaviorContext->Method("get_current_view_position", PyGetCurrentViewPosition, nullptr, "Returns the position of the current view as a Vec3."));
            addLegacyGeneral(behaviorContext->Method("get_current_view_rotation", PyGetCurrentViewRotation, nullptr, "Returns the rotation of the current view as a Vec3 of Euler angles."));
            addLegacyGeneral(behaviorContext->Method("set_current_view_position", PySetCurrentViewPosition, nullptr, "Sets the position of the current view as given x, y, z coordinates."));
            addLegacyGeneral(behaviorContext->Method("set_current_view_rotation", PySetCurrentViewRotation, nullptr, "Sets the rotation of the current view as given x, y, z Euler angles."));

            addLegacyGeneral(behaviorContext->Method("export_to_engine", CCryEditApp::Command_ExportToEngine, nullptr, "Exports the current level to the engine."));
            addLegacyGeneral(behaviorContext->Method("set_config_spec", PySetConfigSpec, nullptr, "Sets the system config spec and platform."));
            addLegacyGeneral(behaviorContext->Method("get_config_platform", PyGetConfigPlatform, nullptr, "Gets the system config platform."));
            addLegacyGeneral(behaviorContext->Method("get_config_spec", PyGetConfigSpec, nullptr, "Gets the system config spec."));

            addLegacyGeneral(behaviorContext->Method("set_result_to_success", PySetResultToSuccess, nullptr, "Sets the result of a script execution to success. Used only for Sandbox AutoTests."));
            addLegacyGeneral(behaviorContext->Method("set_result_to_failure", PySetResultToFailure, nullptr, "Sets the result of a script execution to failure. Used only for Sandbox AutoTests."));

            addLegacyGeneral(behaviorContext->Method("idle_enable", PyIdleEnable, nullptr, "Enables/Disables idle processing for the Editor. Primarily used for auto-testing."));
            addLegacyGeneral(behaviorContext->Method("is_idle_enabled", PyIdleIsEnabled, nullptr, "Returns whether or not idle processing is enabled for the Editor. Primarily used for auto-testing."));
            addLegacyGeneral(behaviorContext->Method("idle_is_enabled", PyIdleIsEnabled, nullptr, "Returns whether or not idle processing is enabled for the Editor. Primarily used for auto-testing."));
            addLegacyGeneral(behaviorContext->Method("idle_wait", PyIdleWait, nullptr, "Waits idling for a given seconds. Primarily used for auto-testing."));
            addLegacyGeneral(behaviorContext->Method("idle_wait_frames", PyIdleWaitFrames, nullptr, "Waits idling for a frames. Primarily used for auto-testing."));

            addLegacyGeneral(behaviorContext->Method("start_process_detached", PyStartProcessDetached, nullptr, "Launches a detached process with an optional space separated list of arguments."));
            addLegacyGeneral(behaviorContext->Method("launch_lua_editor", PyLaunchLUAEditor, nullptr, "Launches the Lua editor, may receive a list of space separate file paths, or an empty string to only open the editor."));

            // this will put these methods into the 'azlmbr.legacy.checkout_dialog' module
            auto addCheckoutDialog = [](AZ::BehaviorContext::GlobalMethodBuilder methodBuilder)
            {
                methodBuilder->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Category, "Legacy/CheckoutDialog")
                    ->Attribute(AZ::Script::Attributes::Module, "legacy.checkout_dialog");
            };
            addCheckoutDialog(behaviorContext->Method("enable_for_all", PyCheckOutDialogEnableForAll, nullptr, "Enables the 'Apply to all' button in the checkout dialog; useful for allowing the user to apply a decision to check out files to multiple, related operations."));

            behaviorContext->EnumProperty<ESystemConfigSpec::CONFIG_AUTO_SPEC>("SystemConfigSpec_Auto")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigSpec::CONFIG_LOW_SPEC>("SystemConfigSpec_Low")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigSpec::CONFIG_MEDIUM_SPEC>("SystemConfigSpec_Medium")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigSpec::CONFIG_HIGH_SPEC>("SystemConfigSpec_High")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigSpec::CONFIG_VERYHIGH_SPEC>("SystemConfigSpec_VeryHigh")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);

            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_INVALID_PLATFORM>("SystemConfigPlatform_InvalidPlatform")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_PC>("SystemConfigPlatform_Pc")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_OSX_GL>("SystemConfigPlatform_OsxGl")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_OSX_METAL>("SystemConfigPlatform_OsxMetal")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_ANDROID>("SystemConfigPlatform_Android")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_IOS>("SystemConfigPlatform_Ios")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_XENIA>("SystemConfigPlatform_Xenia")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_PROVO>("SystemConfigPlatform_Provo")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
            behaviorContext->EnumProperty<ESystemConfigPlatform::CONFIG_APPLETV>("SystemConfigPlatform_AppleTv")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation);
        }
    }
    
}

#include <CryEdit.moc>
