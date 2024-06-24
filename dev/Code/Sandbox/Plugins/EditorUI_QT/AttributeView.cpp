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
#include "EditorUI_QT_Precompiled.h"
#include "AttributeView.h"
#include <AttributeView.moc>
#include "AttributeItem.h"
#include "AttributeGroup.h"
#include "VariableWidgets/QCollapsePanel.h"
#include "Utils.h"

// Cry
#include <ITexture.h>

// EditorCore
#include <Util/Variable.h>
#include <Util/Image.h>
#include <Clipboard.h>
#include <Particles/ParticleUIDefinition.h>
#include <Include/ILogFile.h>
#include <Include/IEditorParticleManager.h>
#include <Util/VariablePropertyType.h>

//QT
#include <QLabel>
#include <QFile>
#include <QtXml/QDomDocument>
#include "VariableWidgets/QCopyModalDialog.h"
#include "PanelWidget/paneltitlebar.h"
#include "AttributeListView.h"
#include "AttributeViewConfig.h"
#include "CurveEditorContent.h"
#include "VariableWidgets/QGradientColorDialog.h"
#include "VariableWidgets/QColorCurve.h"
#include "ContextMenu.h"
#include "IEditorParticleUtils.h"
#include "UIFactory.h"
#include "VariableWidgets/QColumnWidget.h"
#include "AttributeItemLogicCallbacks.h"
#include "DefaultViewWidget.h"
#include "VariableWidgets/QWidgetVector.h"

// AZ
#include <AzFramework/API/ApplicationAPI.h>

#define NAME_COLLAPSE_ALL       tr("Collapse All")
#define NAME_UNCOLLAPSE_ALL     tr("Uncollapse All")

template <class T, class T2>
void WalkVariables(const T& treeObject, const T2& callback, QString path = "")
{
    for (int i = 0; i < treeObject.GetNumVariables(); i++)
    {
        auto* var = treeObject.GetVariable(i);

        // generate new path
        QString subPath = path;
        if (subPath.isEmpty() == false)
        {
            subPath += ".";
        }
        QString varName = var->GetName();
        varName.replace(' ', '_');
        subPath += varName;

        // call callback
        callback(subPath, var);

        // recurse
        WalkVariables(*var, callback, subPath);
    }
}

CAttributeView::CAttributeView(QWidget* parent)
    : QDockWidget(parent)
    , m_contextMenuCollapse(new ContextMenu(tr("Collapse Menu"), this))
    , m_scrollAreaWidget(new PanelWidget())
    , m_scrollArea(new QScrollArea(this))
    , m_refreshCallback(nullptr)
    , m_gradientSwatch_1(SCurveEditorContent(), QGradientStops(), this)
    , m_gradientSwatch_2(SCurveEditorContent(), QGradientStops(), this)
    , m_colorSwatch_1(Qt::white, this)
    , m_colorSwatch_2(Qt::black, this)
    , m_isReloading(false)
    , m_bMultiSelectionOn(false)
    , m_attributeViewStatus(0)
    , m_currentItem(nullptr)
    , m_updateLogicalChildrenFence(false)
{
    setObjectName("CAttributeView"); // needed to save state

    setMouseTracking(true);
    m_gradientSwatch_1.hide();
    m_gradientSwatch_2.hide();
    m_colorSwatch_2.hide();
    m_colorSwatch_1.hide();

    m_scrollAreaWidget->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
    m_scrollAreaWidget->setMouseTracking(true);

    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setWidget(m_scrollAreaWidget);
    m_scrollArea->setMouseTracking(true);
    QScrollBar* vscrollBar = m_scrollArea->verticalScrollBar();
    connect(vscrollBar, &QScrollBar::valueChanged, this, [this](int v)
        {
            setValue(m_currLibraryName + "_scroll", QString().asprintf("%d", v));
        });
    connect(m_scrollAreaWidget, &PanelWidget::SignalRenamePanel, this, &CAttributeView::RenamePanel);
    connect(m_scrollAreaWidget, &PanelWidget::SignalExportPanel, this, &CAttributeView::ExportPanel);
    connect(m_scrollAreaWidget, &PanelWidget::SignalRemovePanel, this, &CAttributeView::ClosePanel);
    connect(m_scrollAreaWidget, &PanelWidget::SignalRemoveAllParams, this, &CAttributeView::removeAllParams);

    m_defaultView = new DefaultViewWidget(this);
    DecorateDefaultView();

    // Layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);
    mainLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    mainLayout->addWidget(m_defaultView);
    mainLayout->addWidget(m_scrollArea);
    centralWidget->setLayout(mainLayout);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setContextMenuPolicy(Qt::CustomContextMenu);
    centralWidget->setFocusPolicy(Qt::StrongFocus);
    setWidget(centralWidget);
    if (m_attributeViewStatus == 0)
    {
        HideAllButGroup("Group");
    }
    else
    {
        ShowAllButGroup("Group&&Comment");
    }
    ShowDefaultView();
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

CAttributeView::~CAttributeView()
{
    Clear();
}

void CAttributeView::Clear()
{
    m_custompanels.clear();
    // Actions need to be cleared before CAttributeItems are nuked.
    // Otherwise because of lambda function usage the deletion cycle gets corrupted3
    m_contextMenuCollapseSinglePanelActions.clear();
    m_contextMenuCollapse->clear();

    m_currLibraryName.clear();
    m_importedConfigs.clear();
}

void CAttributeView::showAdvanced(bool show)
{
    for (int i = 0; i < (int)m_children.size(); i++)
    {
        CAttributeItem* item = m_children[i];
        item->showAdvanced(show);
    }
}

void CAttributeView::copyItem(const CAttributeItem* item, bool bRecursively)
{
    m_variableCopyList.clear();
    CopyItemVariables(item, bRecursively, m_variableCopyList);
}

void CAttributeView::CopyItemVariables(const CAttributeItem* item, bool recursive, /*out*/ QVector<TSmartPtr<IVariable>>& variableList)
{
    if (item->getVar() != nullptr)
    {
        variableList.push_back(item->getVar()->Clone(false));
    }

    if (recursive)
    {
        for (const CAttributeItem* child : item->getChildren())
        {
            CopyItemVariables(child, recursive, variableList);
        }
    }
}

void CAttributeView::GetItemVariables(CAttributeItem* item, bool recursive, /*out*/ QVector<IVariable*>& variableList, /*out*/ QVector<CAttributeItem*>* itemList)
{
    if (item->getVar() != nullptr)
    {
        variableList.push_back(item->getVar());
        if (itemList)
        {
            itemList->push_back(item);
        }
    }

    if (recursive)
    {
        for (CAttributeItem* child : item->getChildren())
        {
            GetItemVariables(child, recursive, variableList, itemList);
        }
    }
}

bool CAttributeView::CanCopyVariable(IVariable* varSrc, IVariable* varDst)
{
    //note: the src variable is a cloned variable which is a simple string, Check CVariableTypeInfo::Clone function for more detail
    //this function should only be used for checking copy an AttributeItem's IVariable's clone to an AttributeItem's IVariable
    AZ_Assert(varSrc->GetType() == IVariable::EType::STRING, "Source Variable must be a String Variable created by CVariableTypeInfo::Clone");

    //can be copied if it's the the same parameter
    if (varSrc->GetName().compare(varDst->GetName()) == 0)
    {
        return true;
    }

    //no copy if there datatype not same
    if (varSrc->GetDataType() != varDst->GetDataType())
    {
        return false;
    }

    //no copy if the etype not same. varSrc is using userData to save variable type, check CVariableTypeInfo::Clone for detail
    if (varSrc->GetUserData().toInt() != varDst->GetType())
    {
        return false;
    }

    //no copy if the etype is unknown return false
    if (varDst->GetType() == IVariable::EType::UNKNOWN)
    {
        return false;
    }

    return true;
}

void CAttributeView::pasteItem(CAttributeItem* attribute)
{
    if (m_variableCopyList.size() == 0)
    {
        return;
    }

    //get variables list to be pasted to
    bool recursive = m_variableCopyList.size() > 1;
    QVector<IVariable*> toVariables;
    QVector<CAttributeItem*> toItems;
    GetItemVariables(attribute, recursive, toVariables, &toItems);

    AZ_Assert(toVariables.size() == m_variableCopyList.size(), "pasteItem should only be called if checkPasteItem return true");

    //do the copy
    for (int varIdx = 0; varIdx < m_variableCopyList.size(); varIdx++)
    {
        //some curves were disabled so don't copy them
        if (toItems[varIdx]->isEnabled())
        {
            toVariables[varIdx]->CopyValue(m_variableCopyList[varIdx]);
        }
    }
}

bool CAttributeView::checkPasteItem(CAttributeItem* attribute)
{
    if (m_variableCopyList.size() == 0)
    {
        return false;
    }

    bool recursive = m_variableCopyList.size() > 1;

    //recursive
    QVector<IVariable*> toVariables;
    GetItemVariables(attribute, recursive, toVariables, nullptr);

    //if count not match
    if (m_variableCopyList.size() != toVariables.size())
    {
        return false;
    }

    //check type for each variable
    for (int varIdx = 0; varIdx < m_variableCopyList.size(); varIdx++)
    {
        if (!CanCopyVariable(m_variableCopyList[varIdx], toVariables[varIdx]))
        {
            return false;
        }
    }
    return true;
}


void CAttributeView::removeAllParams(QDockWidget* panel)
{
    ClearAttributeSelection();
    if (!PanelExist(panel->windowTitle()))
    {
        // Could not find panel
        return;
    }
    ItemPanelTitleBar* bar = (ItemPanelTitleBar*)panel->titleBarWidget();
    if (bar->getCollapsed()) //UnCollapse the panel
    {
        bar->setCollapsed(false);
    }
    CAttributeItem* originWidget = static_cast<CAttributeItem*>(panel->widget());
    originWidget->resetItem();
}

CAttributeItem* CAttributeView::findItemByPath(const QString& path)
{
    CAttributeItem* item = nullptr;

    for (CAttributeItem* child : m_children)
    {
        item = child->findItemByPath(path);
        if (item)
        {
            break;
        }
    }

    return item;
}

QVector<CAttributeItem*> CAttributeView::findItemsByVar(IVariable* var)
{
    CAttributeItem* item = nullptr;
    QVector<CAttributeItem*> itemList;
    for (CAttributeItem* child : m_children)
    {
        item = child->findItemByVar(var);
        if (item != nullptr && item->getVar() == var)
        {
            itemList.push_back(item);
        }
    }

    return itemList;
}

CAttributeItem* CAttributeView::findItemByVar(IVariable* var)
{
    QVector<CAttributeItem*> list = findItemsByVar(var);
    if (list.count() > 0)
    {
        return list.takeFirst();
    }
    return nullptr;
}

const QString& CAttributeView::getCurrentLibraryName() const
{
    return m_currLibraryName;
}

void CAttributeView::setRefreshCallback(const RefreshCallback& refreshCallback)
{
    m_refreshCallback = refreshCallback;
}

void CAttributeView::InitConfiguration(const QString& itemPath)
{
    m_currLibraryName = itemPath;
    RestoreScrollPosition();
    BuildCollapseUncollapseAllMenu();
    CreateDefaultConfigFile();
    BuildDefaultUI();
    FillFromConfiguration();

    //hide the default view to save a good default layout
    m_scrollAreaWidget->finalizePanels();
    HideDefaultView();
    m_fillCount++;
}

void CAttributeView::FillFromConfiguration()
{
    //this function is for bulid panels base on current config (m_config)
    //it rebuilds custom panels. For default panels, if they already exist, no changes applied.
    //The m_config is loaded from saved config file. it should include default panels and custom panels

    for (int i = 0; i < m_importedConfigs.count(); i++)
    {
        SAFE_DELETE(m_importedConfigs[i]);
    }
    m_importedConfigs.clear();

    //remove custom panels. it will trigger SignalRmovePanel for each of the custom panels
    m_scrollAreaWidget->ClearCustomPanels();

    for (auto& panel : m_config.GetRoot().panels)
    {
        if (PanelExist(panel.innerGroup.name))
        {
            continue;
        }
        // If it is a custom group and there is no items under group, show default panel
        if (panel.innerGroup.isCustom == true &&
            (panel.innerGroup.groups.size() == 0 && panel.innerGroup.items.size() == 0))
        {
            ShowEmptyCustomPanel(panel.innerGroup.name, panel.innerGroup.isGroupAttribute);
        }
        else
        {
            CAttributeItem* widget = new CAttributeItem(this, this, nullptr, panel.innerGroup.name, &panel.innerGroup);
            m_children.push_back(widget);
            m_panels.push_back(m_scrollAreaWidget->addItemPanel(panel.innerGroup.name.toUtf8().data(), widget, false, panel.innerGroup.isCustom, panel.innerGroup.isGroupAttribute));
            if (panel.innerGroup.isCustom)
            {
                m_custompanels.push_back(panel.innerGroup.name);
            }
        }
    }

    //connect to inheritance's enum change signal to refresh the tab
    CAttributeItem* item = findItemByPath("Emitter.Inheritance");
    if (item)
    {
        connect(item, &CAttributeItem::SignalEnumChanged, this, &CAttributeView::SignalRefreshAttributes);
    }
}

void CAttributeView::ExportPanel(QDockWidget* dockPanel, const QString filePath)
{
    QDomDocument doc;
    QDomNode root = doc.createElement("tree");

    QFileInfo info(filePath);
    // Rename panel to file name
    if (info.baseName() != dockPanel->windowTitle())
    {
        if (!RenamePanel(dockPanel, dockPanel->windowTitle(), info.baseName()))
        {
            return;
        }
    }

    // Write panels
    QDomElement panelNode = doc.createElement("panel");
    ItemPanelTitleBar* bar = (ItemPanelTitleBar*)dockPanel->titleBarWidget();
    panelNode.setAttribute("name", dockPanel->windowTitle());
    panelNode.setAttribute("custom", true);
    //Get Attributes from the panel
    CAttributeItem* attributes = nullptr;
    if (bar->getCollapsed())
    {
        attributes = static_cast<CAttributeItem*>(bar->getStoredWidget());
    }
    else
    {
        attributes = static_cast<CAttributeItem*>(dockPanel->widget());
    }

    CAttributeItem::AttributeList itemList = attributes->getChildren();
    for (CAttributeItem* item : itemList)
    {
        WriteAttributeItem(item, &panelNode, &doc);
    }

    root.appendChild(panelNode);

    doc.appendChild(root);
    QFile file(filePath);
    file.open(QIODevice::WriteOnly);
    file.write(doc.toString().toUtf8().data());        // write to stderr
    file.close();

    emit SignalAddAttributePreset(info.baseName(), doc.toString());
}

void CAttributeView::ImportPanel(const QString data, bool isFilePath)
{
    AttributeItemLogicCallbacks::SetCallbacksEnabled(false);
    CAttributeViewConfig* custompanel = nullptr;
    if (isFilePath)
    {
        custompanel = new CAttributeViewConfig(data);
        m_importedConfigs.push_back(custompanel);
    }
    else // else QString is the file data
    {
        QDomDocument* doc = new QDomDocument();
        doc->setContent(data);
        custompanel = new CAttributeViewConfig(*doc);
        m_importedConfigs.push_back(custompanel);
    }

    if (!custompanel) // Config load fail
    {
        // Report Warning
        QString text = "Fail to load panel. Wrong panel data format.\n";
        QMessageBox::warning(this, "Warning", text, "Ok");
        return;
    }

    for (auto& panel : custompanel->GetRoot().panels)
    {
        QDockWidget* newpanel = nullptr;

        if (PanelExist(panel.innerGroup.name))
        {
            // Already have panel named panel.innergroup.name. Zoom to the panel
            for (QWidget* dockWidget : m_panels)
            {
                if (dockWidget->windowTitle() == panel.innerGroup.name)
                {
                    ZoomToAttribute(m_scrollAreaWidget->mapToGlobal(dockWidget->pos()));
                }
            }
            // Warning Dialog: Already have panel named panel.innergroup.name
            QString text = "Custom attribute panel \"" + panel.innerGroup.name + "\" already exists.\n";
            QMessageBox::warning(this, "Warning", text, "Ok");
        }
        else
        {
            CAttributeItem* widget = nullptr;
            // If it is a custom group and there is no items under group, show default panel
            if (panel.innerGroup.isCustom == true &&
                (panel.innerGroup.groups.size() == 0 && panel.innerGroup.items.size() == 0))
            {
                widget = new CAttributeItem(this, this, "Fill the panel by dragging parameter in.");
            }
            else
            {
                widget = new CAttributeItem(this, this, nullptr, panel.innerGroup.name, &panel.innerGroup);
            }

            if (widget != nullptr)
            {
                m_custompanels.append(panel.innerGroup.name);
                CAttributeListView* dWidget = new CAttributeListView(m_scrollAreaWidget, panel.innerGroup.isCustom, panel.innerGroup.isGroupAttribute);
                dWidget->setWidget(widget);
                widget->setSizePolicy(QSizePolicy(widget->sizePolicy().horizontalPolicy(), QSizePolicy::Maximum));
                connect(dWidget, &QDockWidget::topLevelChanged, dWidget, [dWidget](bool isTopLevel)
                    {
                        dWidget->setWindowOpacity(isTopLevel ? 0.5 : 1.0);
                    });

                dWidget->setObjectName(QString("obj_") + panel.innerGroup.name);
                dWidget->setWindowTitle(panel.innerGroup.name);
                ItemPanelTitleBar* bar = new ItemPanelTitleBar(dWidget, widget, true);
                dWidget->setTitleBarWidget(bar);
                //Add CAttributeItem to m_children, otherwise the callbacks will not get called
                m_children.push_back(widget);
                m_panels.push_back(dWidget);
                m_scrollAreaWidget->AddCustomPanel(dWidget);
                bar->setCollapsed(true);
            }
        }
    }

    emit SignalRefreshAttributeView();

    // If import from file path, add it to preset list
    if (isFilePath)
    {
        AddToPresetList(data);
    }
    AttributeItemLogicCallbacks::SetCallbacksEnabled(true);
}

void CAttributeView::AddToPresetList(QString filepath)
{
    QFile file(filepath);
    file.open(QIODevice::ReadOnly);
    QByteArray data = file.readAll();
    QString docText(data);
    QFileInfo fileInfo(filepath);
    QString panelName(fileInfo.baseName());
    emit SignalAddAttributePreset(panelName, docText);
}

void CAttributeView::WriteToBuffer(QByteArray& out)
{
    QDomDocument doc;
    QDomNode root = doc.createElement("tree");
    // Write Ignored Varaibels
    QMap<QString, bool>::iterator iter;

    // Write panels
    for (auto& panel : m_panels)
    {
        QDomElement panelNode = doc.createElement("panel");
        CAttributeListView* dockPanel = static_cast<CAttributeListView*>(panel);
        ItemPanelTitleBar* bar = (ItemPanelTitleBar*)dockPanel->titleBarWidget();
        panelNode.setAttribute("name", panel->windowTitle());
        panelNode.setAttribute("custom", dockPanel->isCustomPanel());
        panelNode.setAttribute("GroupVisibility", dockPanel->getGroupVisibility());
        //Get Attributes from the panel
        CAttributeItem* attributes = nullptr;
        if (bar->getCollapsed())
        {
            attributes = static_cast<CAttributeItem*>(bar->getStoredWidget());
        }
        else
        {
            attributes = static_cast<CAttributeItem*>(dockPanel->widget());
        }
        CAttributeItem::AttributeList itemList = attributes->getChildren();
        for (CAttributeItem* item : itemList)
        {
            WriteAttributeItem(item, &panelNode, &doc);
        }

        root.appendChild(panelNode);
    }
    doc.appendChild(root);
    out = doc.toByteArray();
}

void CAttributeView::WriteToFile(const QString& itemPath)
{
    QFile file(itemPath);
    file.open(QIODevice::WriteOnly);
    QByteArray output;
    WriteToBuffer(output);
    file.write(output);
    file.close();
}

void CAttributeView::WriteAttributeItem(CAttributeItem* node, QDomNode* parent, QDomDocument* doc)
{
    if (node->isGroup())
    {
        QDomElement group = doc->createElement("group");
        group.setAttribute("name", node->objectName());
        for (CAttributeItem* child : node->getChildren())
        {
            WriteAttributeItem(child, &group, doc);
        }

        QString visibilityValues = node->GetVisibilityValues();
        if (!visibilityValues.isEmpty())
        {
            group.setAttribute("visibility", visibilityValues);
        }
        parent->appendChild(group);
    }
    else
    {
        if (node->objectName().size() == 0)
        {
            return;
        }
        QDomElement item = doc->createElement("item");
        item.setAttribute("as", node->objectName());

        if (node->isAdvanced())
        {
            item.setAttribute("advanced", "yes");
        }

        // Childrens
        for (CAttributeItem* child : node->getChildren())
        {
            WriteAttributeItem(child, &item, doc);
        }
        parent->appendChild(item);
    }
}

void CAttributeView::CreateNewCustomPanel()
{
    QString name = "Custom";
    // Check if the name is valiad
    int count = 0;
    while (PanelExist(name))
    {
        // Appending number to name to get a unique name. ex: Custom01
        name = QString("Custom%1").arg(++count, 2, 10, QChar('0'));
    }
    QDockWidget* newpanel = ShowEmptyCustomPanel(name, m_attributeViewStatus == 0 ? "emitter" : "group");
    newpanel->show();

    // Allow user to rename custom panel
    ItemPanelTitleBar* bar = static_cast<ItemPanelTitleBar*> (newpanel->titleBarWidget());
    bar->StartRename();
}

QDockWidget* CAttributeView::ShowEmptyCustomPanel(QString panelname, QString groupvisibility)
{
    CAttributeItem* widget = new CAttributeItem(this, this, "Fill the panel by dragging parameter in.");

    m_custompanels.push_back(panelname);
    CAttributeListView* dWidget = new CAttributeListView(m_scrollAreaWidget, true, groupvisibility);
    dWidget->setWidget(widget);
    widget->setSizePolicy(QSizePolicy(widget->sizePolicy().horizontalPolicy(), QSizePolicy::Maximum));
    connect(dWidget, &QDockWidget::topLevelChanged, dWidget, [dWidget](bool isTopLevel)
        {
            dWidget->setWindowOpacity(isTopLevel ? 0.5 : 1.0);
        });

    dWidget->setObjectName(QString("obj_") + panelname);
    dWidget->setWindowTitle(panelname);
    ItemPanelTitleBar* bar = new ItemPanelTitleBar(dWidget, widget, true);
    dWidget->setTitleBarWidget(bar);

    m_panels.push_back(dWidget);
    m_scrollAreaWidget->AddCustomPanel(dWidget);

    bar->setCollapsed(true);
    m_children.push_back(widget);
    return dWidget;
}

void CAttributeView::ClosePanel(QDockWidget* panel)
{
    removeAllParams(panel);
    for (int i = 0; i < m_custompanels.count(); i++)
    {
        if (m_custompanels[i] == panel->windowTitle())
        {
            m_custompanels.remove(i);
            break;
        }
    }

    for (int i = 0; i < m_panels.count(); i++)
    {
        if (m_panels[i] == panel)
        {
            m_panels.remove(i);
            break;
        }
    }
    panel->setObjectName("ClosedPanel");

    for (int i = 0; i < m_children.count(); i++)
    {
        if (m_children[i]->getAttributePath(false) == panel->windowTitle())
        {
            m_children.remove(i);
            break;
        }
    }
}

void CAttributeView::SetConfiguration(CAttributeViewConfig& configuration, CVarBlock* block)
{
    m_config = configuration;
    m_uiVars = block;
}

void CAttributeView::CreateDefaultConfigFile(CVarBlock* vars)
{
    AZStd::string defaultAttributeViewLayoutPath(DEFAULT_ATTRIBUTE_VIEW_LAYOUT_PATH);
    AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::ResolveEnginePath, defaultAttributeViewLayoutPath);

    m_defaultConfig = *CreateConfigFromFile(QString(defaultAttributeViewLayoutPath.c_str()));

    if (vars)
    {
        m_configVariableMap.clear();

        // build a list of variables in the CVarBlock
        WalkVariables(*vars, [this](const QString& name, IVariable* var)
            {
                m_configVariableMap[name] = var;
            });
    }
    m_variableIgnoreMap.clear();
    for (auto ignored : m_defaultConfig.GetRoot().ignored)
    {
        m_variableIgnoreMap.insert(ignored.path, true);
    }
}

void CAttributeView::BuildDefaultUI()
{
    //m_children.clear();
    m_importedConfigs.clear();
    m_children.clear();
    m_defaultChildren.clear();
    m_panels.clear();
    m_custompanels.clear();
    m_scrollAreaWidget->ClearCustomPanels();

    /*m_children.clear();
    m_custompanels.clear();
    m_panels.clear();
    m_scrollAreaWidget->ClearAllPanels();*/

    for (auto& panel : m_defaultConfig.GetRoot().panels)
    {
        if (PanelExist(panel.innerGroup.name))
        {
            continue;
        }
        // If it is a custom group and there is no items under group, show default panel
        if (panel.innerGroup.isCustom == true &&
            (panel.innerGroup.groups.size() == 0 && panel.innerGroup.items.size() == 0))
        {
            ShowEmptyCustomPanel(panel.innerGroup.name, panel.innerGroup.isGroupAttribute);
        }
        else
        {
            CAttributeItem* widget = new CAttributeItem(this, this, nullptr, panel.innerGroup.name, &panel.innerGroup);
            m_defaultChildren.push_back(widget);
            m_panels.push_back(m_scrollAreaWidget->addItemPanel(panel.innerGroup.name.toUtf8().data(), widget, false, panel.innerGroup.isCustom, panel.innerGroup.isGroupAttribute));
            if (panel.innerGroup.isCustom)
            {
                m_custompanels.push_back(panel.innerGroup.name);
            }
        }
    }
}

void CAttributeView::ZoomToAttribute(QPoint pos)
{
    QPoint finalPos = m_scrollArea->mapFromGlobal(pos);
    int posY = finalPos.y();
    int attributeviewheight = this->height();
    if (posY > 0 && posY < this->height())
    {
        // Maybe collapse the panel then return?
        return;
    }
    int scrollBarValue = m_scrollArea->verticalScrollBar()->value();
    int deltaY = scrollBarValue + posY;
    if (deltaY < 0)
    {
        deltaY = 0;
    }
    m_scrollArea->verticalScrollBar()->setValue(deltaY);
}

// Create Default Config File must be called before this function;
CAttributeViewConfig* CAttributeView::CreateConfigFromFile(QString filepath)
{
    QMessageLogger log;
    QFile file(filepath);
    file.open(QIODevice::ReadOnly);
    QByteArray data = file.readAll();
    QDomDocument doc;

    if (!doc.setContent(data))
    {
        // if fail loading, load default panel xml
        if (filepath != DEFAULT_ATTRIBUTE_VIEW_LAYOUT_PATH)
        {
            return &m_defaultConfig;
        }
        return nullptr;
    }
    return new CAttributeViewConfig(doc);
}

void CAttributeView::OnStartReload()
{
    m_isReloading = true;
    AttributeItemLogicCallbacks::SetCallbacksEnabled(false);
    AttributeItemLogicCallback resetCallback;
    if (AttributeItemLogicCallbacks::GetCallback("ResetPersistentState", &resetCallback))
    {
        resetCallback(nullptr);
    }
}

void CAttributeView::OnEndReload()
{
    m_isReloading = false;
    AttributeItemLogicCallbacks::SetCallbacksEnabled(true);
    //Make sure all attribute items trigger the update callback

    std::function<void(CAttributeItem*)> func = [&func](CAttributeItem* item)
    {
        item->TriggerUpdateCallback();
        item->ForEachChild(func);
    };
    for (int i = 0; i < m_children.size(); i++)
    {
        func(m_children[i]);
    }
}


void CAttributeView::ResolveVisibility()
{
    for (CAttributeItem* child : m_children)
    {
        child->ResolveVisibility(m_defaultConfig.GetRoot().visibilityVars);
    }
}

void CAttributeView::RestoreScrollPosition()
{
    QScrollBar* vscrollBar = m_scrollArea->verticalScrollBar();
    const int scrollValue = atoi(getValue(m_currLibraryName + "_scroll", QString().asprintf("%d", vscrollBar->value())).toStdString().c_str());
    vscrollBar->setValue(scrollValue);
}

void CAttributeView::BuildCollapseUncollapseAllMenu()
{
    // Create context menu bar to collapse/uncollapse of main attribute groups
    {
        QAction* action = nullptr;

        // Create collapse all action
        action = m_contextMenuCollapse->addAction(NAME_COLLAPSE_ALL);
        action->connect(action, &QAction::triggered, this, [this](bool state)
            {
                for (QAction* a : m_contextMenuCollapseSinglePanelActions)
                {
                    QCollapsePanel* panel = a->property("collapse_panel").value<QCollapsePanel*>();
                    if (!panel)
                    {
                        continue;
                    }

                    panel->setCollapsed(true);
                    a->setChecked(!panel->isCollapsed());
                }
            });

        // Create uncollapse all action
        action = m_contextMenuCollapse->addAction(NAME_UNCOLLAPSE_ALL);
        action->connect(action, &QAction::triggered, this, [this](bool state)
            {
                for (QAction* a : m_contextMenuCollapseSinglePanelActions)
                {
                    QCollapsePanel* panel = a->property("collapse_panel").value<QCollapsePanel*>();
                    if (!panel)
                    {
                        continue;
                    }

                    panel->setCollapsed(false);
                    a->setChecked(!panel->isCollapsed());
                }
            });

        m_contextMenuCollapse->addSeparator();
    }
}

void CAttributeView::updateCallbacks(CAttributeItem* item)
{
    if (m_isReloading)
    {
        return;
    }

    if (item)
    {
        item->updateCallbacks(true);
        return;
    }

    for (CAttributeItem* i : m_children)
    {
        i->updateCallbacks(true);
    }
}

void CAttributeView::setValue(const QString& key, const QString& value)
{
    m_valueMap[key] = value;
}

QString CAttributeView::getValue(const QString& key, const QString& defaultValue)
{
    return m_valueMap.value(key, defaultValue);
}

void CAttributeView::openCollapseMenu(const QPoint& pos)
{
    if (!m_contextMenuCollapse)
    {
        return;
    }

    m_contextMenuCollapse->setFocus();
    m_contextMenuCollapse->exec(pos);
}

void CAttributeView::addToCollapseContextMenu(QAction* action)
{
    if (!m_contextMenuCollapse)
    {
        return;
    }

    // Set parentship, this makes sure when m_contextMenuCollapse
    // is deleted, the actions are also removed
    action->setParent(m_contextMenuCollapse);
    m_contextMenuCollapse->addAction(action);
    m_contextMenuCollapseSinglePanelActions.push_back(action);
}

//Get variable settings infos
CAttributeViewConfig::config::item CAttributeView::GetConfigItem(QString as, CAttributeViewConfig::config::group& group, bool& found)
{
    CAttributeViewConfig::config::item returnItem;
    for (CAttributeViewConfig::config::item item : group.items)
    {
        returnItem = GetConfigItem(as, item, found);
        if (found)
        {
            return returnItem;
        }
    }
    for (CAttributeViewConfig::config::group itemgroup : group.groups)
    {
        returnItem = GetConfigItem(as, itemgroup, found);
        if (found)
        {
            return returnItem;
        }
    }

    found = false;
    return returnItem;
}

CAttributeViewConfig::config::item CAttributeView::GetConfigItem(QString as, CAttributeViewConfig::config::item& item, bool& found)
{
    CAttributeViewConfig::config::item returnitem;
    if (item.as == as)
    {
        found = true;
        return item;
    }

    for (CAttributeViewConfig::config::item subitem : item.items)
    {
        returnitem = GetConfigItem(as, subitem, found);
        if (found)
        {
            return returnitem;
        }
    }
    found = false;
    return returnitem;
}

QString CAttributeView::GetConfigItemName(QString as)
{
    bool found = false;
    for (CAttributeViewConfig::config::panel config : m_defaultConfig.GetRoot().panels)
    {
        CAttributeViewConfig::config::item returnitem = GetConfigItem(as, config.innerGroup, found);
        if (found)
        {
            return returnitem.name;
        }
    }

    return "";
}

const CAttributeViewConfig* CAttributeView::GetDefaultConfig()
{
    return &m_defaultConfig;
}

QString CAttributeView::GetConfigItemVisibility(QString as)
{
    bool found = false;
    for (CAttributeViewConfig::config::panel config : m_defaultConfig.GetRoot().panels)
    {
        CAttributeViewConfig::config::item returnitem = GetConfigItem(as, config.innerGroup, found);
        if (found)
        {
            return returnitem.visibility;
        }
    }

    return "";
}

QString CAttributeView::GetConfigItemCallback(QString as)
{
    bool found = false;
    for (CAttributeViewConfig::config::panel config : m_defaultConfig.GetRoot().panels)
    {
        CAttributeViewConfig::config::item returnitem = GetConfigItem(as, config.innerGroup, found);
        if (found)
        {
            return returnitem.onUpdateCallback;
        }
    }

    return "";
}

std::vector<CAttributeViewConfig::config::relation> CAttributeView::GetConfigItemRelations(QString as)
{
    bool found = false;
    for (CAttributeViewConfig::config::panel config : m_defaultConfig.GetRoot().panels)
    {
        CAttributeViewConfig::config::item returnitem = GetConfigItem(as, config.innerGroup, found);
        if (found)
        {
            return returnitem.relations;
        }
    }
    // return empty vector
    std::vector<CAttributeViewConfig::config::relation> relations;
    return relations;
}

QString CAttributeView::GetConfigGroupVisibility(QString name)
{
    bool found = false;
    for (CAttributeViewConfig::config::panel config : m_defaultConfig.GetRoot().panels)
    {
        CAttributeViewConfig::config::group returnedgroup = GetConfigGroup(name, config.innerGroup, found);
        if (found)
        {
            return returnedgroup.visibility;
        }
    }

    return "";
}

CAttributeViewConfig::config::group CAttributeView::GetConfigGroup(QString name, CAttributeViewConfig::config::group& group, bool& found)
{
    CAttributeViewConfig::config::group returnitem;
    if (group.name == name)
    {
        found = true;
        return group;
    }

    for (CAttributeViewConfig::config::group subgroup : group.groups)
    {
        returnitem = GetConfigGroup(name, subgroup, found);
        if (found)
        {
            return returnitem;
        }
    }
    found = false;
    return returnitem;
}

bool CAttributeView::isVariableIgnored(QString path)
{
    if (m_variableIgnoreMap.contains(path))
    {
        return true;
    }
    return false;
}

IVariable* CAttributeView::getVariableFromName(QString name)
{
    IVariable* item = nullptr;
    if (getConfigVariableMap().contains(name))
    {
        item = getConfigVariableMap().find(name).value();
    }
    return item;
}

void CAttributeView::paintEvent(QPaintEvent*)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void CAttributeView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control)
    {
        m_bMultiSelectionOn = true;
    }
}

void CAttributeView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control)
    {
        m_bMultiSelectionOn = false;
    }
}

void CAttributeView::ClearAttributeSelection()
{
    for (CAttributeGroup* selectedGroup : m_selectedGroups)
    {
        if (selectedGroup)
        {
            selectedGroup->SetDragStyle("None");
        }
    }
    m_selectedGroups.clear();
}

// Add attributes for multiselection
void CAttributeView::AddSelectedAttribute(CAttributeGroup* group)
{
    if (!m_selectedGroups.contains(group))
    {
        if (!m_bMultiSelectionOn)
        {
            ClearAttributeSelection();
        }
        else
        {
            m_selectedGroups.push_back(group);
            group->SetDragStyle("StartToDrag");
        }
    }
    else
    {
        // if selected item already in the group, clear selection
        if (m_bMultiSelectionOn)
        {
            for (int i = 0; i < m_selectedGroups.size(); i++)
            {
                if (m_selectedGroups[i] == group)
                {
                    m_selectedGroups.remove(i);
                    group->SetDragStyle("None");
                    break;
                }
            }
        }
    }
}

// Add attribute Start to Drag
void CAttributeView::AddDragAttribute(CAttributeGroup* group)
{
    if (!m_selectedGroups.contains(group))
    {
        m_selectedGroups.push_back(group);
        group->SetDragStyle("StartToDrag");
    }
}

QVector<CAttributeGroup*> CAttributeView::GetSelectedGroups()
{
    return m_selectedGroups;
}

void CAttributeView::SetEnabledFromPath(QString path, bool enable)
{
    IVariable* c;
    QVector<CAttributeItem*> itemList;
    c = GetVarFromPath(path);
    if (c)
    {
        itemList = findItemsByVar(c);
    }
    for (CAttributeItem* item : itemList)
    {
        if (item)
        {
            if (enable)
            {
                item->enable();
            }
            else
            {
                item->disable();
            }
        }
    }
}

void CAttributeView::UpdateLogicalChildren(IVariable* var)
{
    SelfCallFence(m_updateLogicalChildrenFence);
    QMapIterator<QString, IVariable*> map(m_configVariableMap);

    QString varPath = (GetVariablePath(var));

    //is this the continuous property
    if (varPath.compare("Timing.Continuous", Qt::CaseInsensitive) == 0)
    {
        //is this enabled?
        bool value = true;
        var->Get(value);
        if (value)
        {
            SetEnabledFromPath("Timing.Emitter_Life_Time", true);//then enable logical children
        }
        else
        {
            SetEnabledFromPath("Timing.Emitter_Life_Time", false);//then disable logical children
        }
    }
    //is this the texture property or material property
    else if (varPath.compare("Appearance.Texture", Qt::CaseInsensitive) == 0 || varPath.compare("Appearance.Material", Qt::CaseInsensitive) == 0)
    {
        // Get the text fields for texture and material
        IVariable* textureVariable = GetVarFromPath("Appearance.Texture");
        QString textureString = textureVariable->GetDisplayValue();

        IVariable* materialVariable = GetVarFromPath("Appearance.Material");
        QString materialString = materialVariable->GetDisplayValue();

        // If both are empty, grey out the texture tiling parameters
        if (textureString.isEmpty() && materialString.isEmpty())
        {
            SetEnabledFromPath("Appearance.Texture_Tiling", false);//then disable logical children
            SetEnabledFromPath("Appearance.GlowTexture_Tiling", false);//then disable logical children
        }
        else
        {
            SetEnabledFromPath("Appearance.Texture_Tiling", true);//then enable logical children
            SetEnabledFromPath("Appearance.GlowTexture_Tiling", true);//then enable logical children
        }
    }
    // is this the glow texture
    else if (varPath.compare("Appearance.GlowTexture", Qt::CaseInsensitive) == 0)
    {
        // Get the text fields for texture and material
        IVariable* textureVariable = GetVarFromPath("Appearance.GlowTexture");
        QString textureString = textureVariable->GetDisplayValue();

        if (textureString.isEmpty())
        {
            SetEnabledFromPath("Appearance.GlowTexture_Tiling", false);//then disable logical children
        }
        else
        {
            SetEnabledFromPath("Appearance.GlowTexture_Tiling", true);//then enable logical children
        }
    }
    //is this the physics type property
    else if (varPath.compare("Collision.Physics_Type", Qt::CaseInsensitive) == 0)
    {
        //is this enabled? - grab edit box if !empty it is enabled

        QString value = "";
        value = var->GetDisplayValue();

        IVariable* emitterTypeVar = GetVarFromPath("Emitter.Emitter_Type");
        bool isGPU = emitterTypeVar->GetDisplayValue() == "GPU";

        //enable/disable collision attributes based on physics type
        bool enableOnCollideTerrain = false;
        bool enableOnCollideStaticObjects = false;
        bool enableOnCollideDynamicObjects = false;
        bool enableMaxCollisionEvents = false;
        bool enableBounciness = false;
        bool enableSurfaceType = false;
        bool enableDynamicFriction = false;
        bool enableThickness = false;
        bool enableDensity = false;
        bool isPhysics = false;
        bool isCollision = false;

        if (isGPU)
        {
            enableThickness = true;
            enableBounciness = true;
        }
        else
        {
#if PARTICLES_USE_CRY_PHYSICS
            if (value.compare("SimpleCollision") == 0)
            {
                enableOnCollideTerrain = true;
                enableOnCollideStaticObjects = true;
                enableOnCollideDynamicObjects = true;
                enableBounciness = true;
                enableDynamicFriction = true;
                isCollision = true;
                isPhysics = false;
            }
            else if (value.compare("SimplePhysics") == 0)
            {
                enableSurfaceType = true;
                enableThickness = true;
                enableDensity = true;
                isCollision = true;
                isPhysics = true;
            }
            else if (value.compare("RigidBody") == 0)
            {
                enableMaxCollisionEvents = true;
                enableSurfaceType = true;
                enableDensity = true;
                isCollision = true;
                isPhysics = true;
            }
#else
            if (value.compare("SimpleCollision") == 0)
            {
                enableOnCollideTerrain = true;
                enableOnCollideStaticObjects = true;
                enableOnCollideDynamicObjects = true;
                isCollision = true;
                isPhysics = false;
            }
            else if (value.compare("RigidBody") == 0)
            {
                enableThickness = true;
                enableMaxCollisionEvents = true;
                enableSurfaceType = true;
                enableDensity = true;
                isCollision = true;
                isPhysics = true;
            }
#endif // PARTICLES_USE_CRY_PHYSICS
        }

        SetEnabledFromPath("Collision.Collide_Terrain", enableOnCollideTerrain);
        SetEnabledFromPath("Collision.Collide_Static_Objects", enableOnCollideStaticObjects);
        SetEnabledFromPath("Collision.Collide_Dynamic_Objects", enableOnCollideDynamicObjects);
        SetEnabledFromPath("Collision.Max_Collision_Events", isPhysics);
        SetEnabledFromPath("Collision.Elasticity", enableBounciness);
        SetEnabledFromPath("Collision.Surface_Type", enableSurfaceType);
        SetEnabledFromPath("Collision.Dynamic_Friction", enableDynamicFriction);
        SetEnabledFromPath("Collision.Thickness", enableThickness);
        SetEnabledFromPath("Collision.Density", enableDensity);
        SetEnabledFromPath("Collision.Collision_Fraction", isCollision);
        SetEnabledFromPath("Collision.Collision_Cutoff_Distance", isCollision);
        SetEnabledFromPath("Collision.Final_Collision", isCollision);
#if !PARTICLES_USE_CRY_PHYSICS
        SetEnabledFromPath("Collision.Use_Self_Collision", isPhysics);
        SetEnabledFromPath("Collision.Collision_Layer", isPhysics);
        SetEnabledFromPath("Collision.Collision_Group", isPhysics);
        SetEnabledFromPath("Collision.Physics_Material", isCollision);
#endif
    }
    //is this the physics type property
    else if (varPath.compare("Appearance.Geometry", Qt::CaseInsensitive) == 0)
    {
        //is this enabled? - grab edit box if !empty it is enabled

        QString value = "";
        value = var->GetDisplayValue();

        if (value.length() == 0)
        {
            SetEnabledFromPath("Appearance.Geometry_Pieces", false);//then disable logical children
            SetEnabledFromPath("Appearance.No_Offset", false);//then disable logical children
        }
        else
        {
            SetEnabledFromPath("Appearance.Geometry_Pieces", true);//then disable logical children
            SetEnabledFromPath("Appearance.No_Offset", true);//then disable logical children
        }
    }
    else if (varPath.compare("Emitter.Enabled", Qt::CaseInsensitive) == 0)
    {
        bool state = false;
        var->Get(state);
        if (m_callbackEmitterEnableToggled)
        {
            m_callbackEmitterEnableToggled(state);
        }
    }
    else if (varPath.compare("Emitter.Emitter_Type", Qt::CaseInsensitive) == 0)
    {
        //enable/disable collision attributes based on physics type
        bool enableOnCollideTerrain = false;
        bool enableOnCollideStaticObjects = false;
        bool enableOnCollideDynamicObjects = false;
        bool enableMaxCollisionEvents = false;
        bool enableBounciness = false;
        bool enableSurfaceType = false;
        bool enableDynamicFriction = false;
        bool enableThickness = false;
        bool enableDensity = false;
        bool isPhysics = false;

        IVariable* physType = GetVarFromPath("Collision.Physics_Type");

        ResolveVisibility();
        //this will synchronize the emitter shape between cpu and gpu when swapping between them.
        QString value = "";
        value = var->GetDisplayValue();
        if (value == "GPU")
        {
            IVariable* cpuVar = GetVarFromPath("Emitter.Emitter_Shape");
            IVariable* gpuVar = GetVarFromPath("Emitter.Emitter_Gpu_Shape");
            value = cpuVar->GetDisplayValue();
            if (value.compare("BEAM", Qt::CaseInsensitive) == 0)
            {
                value = "ANGLE";
            }
            gpuVar->SetDisplayValue(value);

            SetMaxParticleCount(PARTICLE_PARAMS_MAX_COUNT_GPU);


            enableThickness = true;
            enableBounciness = true;
        }
        else
        {
            IVariable* cpuVar = GetVarFromPath("Emitter.Emitter_Shape");
            IVariable* gpuVar = GetVarFromPath("Emitter.Emitter_Gpu_Shape");
            value = cpuVar->GetDisplayValue();
            cpuVar->SetDisplayValue(value);

            SetMaxParticleCount(PARTICLE_PARAMS_MAX_COUNT_CPU);

            QString physTypeValue = physType->GetDisplayValue();

            if (physTypeValue.compare("SimpleCollision") == 0)
            {
                enableOnCollideTerrain = true;
                enableOnCollideStaticObjects = true;
                enableOnCollideDynamicObjects = true;
                enableBounciness = true;
                enableDynamicFriction = true;
                isPhysics = false;
            }
            else if (physTypeValue.compare("SimplePhysics") == 0)
            {
                enableSurfaceType = true;
                enableThickness = true;
                enableDensity = true;
                isPhysics = true;
            }
            else if (physTypeValue.compare("RigidBody") == 0)
            {
                enableMaxCollisionEvents = true;
                enableSurfaceType = true;
                enableDensity = true;
                isPhysics = true;
            }
        }

        SetEnabledFromPath("Collision.Collide_Terrain", enableOnCollideTerrain);
        SetEnabledFromPath("Collision.Collide_Static_Objects", enableOnCollideStaticObjects);
        SetEnabledFromPath("Collision.Collide_Dynamic_Objects", enableOnCollideDynamicObjects);
        SetEnabledFromPath("Collision.Max_Collision_Events", enableMaxCollisionEvents);
        SetEnabledFromPath("Collision.Elasticity", enableBounciness);
        SetEnabledFromPath("Collision.Surface_Type", enableSurfaceType);
        SetEnabledFromPath("Collision.Dynamic_Friction", enableDynamicFriction);
        SetEnabledFromPath("Collision.Thickness", enableThickness);
        SetEnabledFromPath("Collision.Density", enableDensity);
#if !PARTICLES_USE_CRY_PHYSICS
        SetEnabledFromPath("Collision.Use_Self_Collision", isPhysics);
        SetEnabledFromPath("Collision.Collision_Layer", isPhysics);
        SetEnabledFromPath("Collision.Collision_Group", isPhysics);
#endif
    }
    else if (varPath.compare("Beam.Segment_Type", Qt::CaseInsensitive) == 0)
    {
        QString value = "";
        value = var->GetDisplayValue();
        if (value.compare("FIXED", Qt::CaseInsensitive) == 0)
        {
            SetEnabledFromPath("Beam.Segment_Length", false);
            SetEnabledFromPath("Beam.Segment_Count", true);
        }
        else
        {
            SetEnabledFromPath("Beam.Segment_Length", true);
            SetEnabledFromPath("Beam.Segment_Count", false);
        }
    }
    else if (varPath.compare("Timing.Emitter_Life_Time", Qt::CaseInsensitive) == 0)
    {
        float value = 0;
        var->Get(value);
        bool enabled = value != 0.0f;
        WalkVariables(*m_uiVars, [=](const QString& name, IVariable* var)
            {
                if (name.contains("Emitter_Strength"))
                {
                    SetEnabledFromPath(name, enabled);
                }
            });
    }
    else if (varPath.compare("Appearance.Sort_Method", Qt::CaseInsensitive) == 0)
    {
        //Grey out Sorting Convergence when None or Bitonic are chosen
        QString value = var->GetDisplayValue();
        bool hide = ParticleParams::ESortMethod::TypeInfo().MatchName(ParticleParams::ESortMethod::Bitonic, value.toUtf8().data())
            || ParticleParams::ESortMethod::TypeInfo().MatchName(ParticleParams::ESortMethod::None, value.toUtf8().data());
        SetEnabledFromPath("Appearance.Sort_Convergance_Per_Frame", !hide);
    }
    else if (varPath.compare("Emitter.Spawn_Indirection", Qt::CaseInsensitive) == 0)
    {
        //will need modify continuous attribute to false and disable it
        IVariable* varContinuous = GetVarFromPath("Timing.Continuous");
        QString value = var->GetDisplayValue();
        bool disable = ParticleParams::ESpawn::TypeInfo().MatchName(ParticleParams::ESpawn::ParentDeath, value.toUtf8().data())
            || ParticleParams::ESpawn::TypeInfo().MatchName(ParticleParams::ESpawn::ParentCollide, value.toUtf8().data());
        SetEnabledFromPath("Timing.Continuous", !disable);
        SetEnabledFromPath("Timing.Emitter_Life_Time", !disable);
        if (disable)
        {
            varContinuous->Set(false);
        }
    }
    else if (varPath.compare("Angles.Facing", Qt::CaseInsensitive) == 0)
    {
        //Grey out Advanced.Spherical_Approximation if particle facing is not camera or cameraX
        QString value = var->GetDisplayValue();
        bool show = ParticleParams::EFacing::TypeInfo().MatchName(ParticleParams::EFacing::Camera, value.toUtf8().data())
            || ParticleParams::EFacing::TypeInfo().MatchName(ParticleParams::EFacing::CameraX, value.toUtf8().data());
        SetEnabledFromPath("Advanced.Spherical_Approximation", show);
    }
    else if (varPath.compare("Size.Maintain_Aspect_Ratio", Qt::CaseInsensitive) == 0)
    {
        bool value = 0;
        var->Get(value);
        //disable size Y and size Z if maintain aspect ratio was checked
        SetEnabledFromPath("Size.Size_Y", !value);
        SetEnabledFromPath("Size.Size_Z", !value);
    }
#if !PARTICLES_USE_CRY_PHYSICS
    // Not supported yet
    else if (varPath.compare("Advanced.Force_Generation", Qt::CaseInsensitive) == 0)
    {
        SetEnabledFromPath("Advanced.Force_Generation", false);
    }
#endif
}

void CAttributeView::SetMaxParticleCount(int maxCount)
{
    //Get widgets of "Emitter.Count" to setup limit later.
    CAttributeItem* item = findItemByPath("Emitter.Count");
    //We may have more than one widgets for one cvar because of the customized panel. 
    QVector<CAttributeItem*> items = findItemsByVar(item->getVar());
    Prop::Description desc(item->getVar());
    float count = 0;
    item->getVar()->Get(count);

    //Set the particle count limit all the widgets for the "fCount" variable
    for (int i = 0; i < items.size(); i++)
    {
        QWidgetVector* widget = items[i]->findChild<QWidgetVector*>("");
        if (widget)
        {
            widget->SetComponent(0, count,
                desc.m_rangeMin, maxCount,
                desc.m_bHardMin, true, desc.m_step);
        }
    }

}

void CAttributeView::InsertChildren(CAttributeItem* item)
{
    m_children.push_back(item);
}

bool CAttributeView::PanelExist(QString name)
{
    for (QWidget* panel : m_panels)
    {
        QDockWidget* dockpanel = static_cast<QDockWidget*>(panel);
        if (dockpanel->windowTitle() == name)
        {
            return true;
        }
    }
    return false;
}

bool CAttributeView::ValidateInheritance(const QString& inheritance)
{
    //get inheritance
    //return if it's not "Parent" inheritance
    if (inheritance.compare("Parent") != 0)
    {
        return true;
    }

    //if it's parent then compare emitter shape for this particle and parent particle
    IParticleEffect* effect = m_currentItem->GetEffect();
    IParticleEffect* parent = effect->GetParent();
    if (parent)
    {
        QString parentShape = ParticleParams::EEmitterShapeType::TypeInfo().ToName(parent->GetParticleParams().GetEmitterShape());
        QString childShape = GetVarFromPath("Emitter.Emitter_Shape")->GetDisplayValue();
        if (parentShape != childShape)
        {
            //message
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Emitter shape conflict"));
            msgBox.setText(tr("Child emitter must have same emitter shape as parent if use \"Parent\" inheritance."));
            msgBox.addButton(QMessageBox::Ok);
            msgBox.exec();
            return false;
        }
    }
    return true;
}

bool CAttributeView::ValidateEmitterShapeChange()
{
    //needs currently selected emitter
    if (!m_currentItem)
    {
        AZ_Assert(false, "m_currentItem shouldn't be null");
        return false;
    }
    else
    {
        return true;
    }
}


QString CAttributeView::GetVariablePath(IVariable* var)
{
    return m_configVariableMap.key(var, "");
}

QString CAttributeView::GetVariablePath(const IVariable* var)
{
    return m_configVariableMap.key(const_cast<IVariable*>(var), "");
}

IVariable* CAttributeView::GetVarFromPath(QString path)
{
    IVariable* var = nullptr;
    if (getConfigVariableMap().contains(path))
    {
        var = getConfigVariableMap().find(path).value();
    }

    if (var == nullptr)
    {
        CAttributeItem* _item = findItemByPath(path);
        if (_item)
        {
            return _item->getVar();
        }
        return nullptr;
    }
    return var;
}

void CAttributeView::hideChildren()
{
    for (auto item : m_panels)
    {
        item->hide();
    }
    for (CAttributeItem* item : m_children)
    {
        item->hide();
    }
}

void CAttributeView::showChildren()
{
    for (auto item : m_panels)
    {
        item->show();
    }
    for (CAttributeItem* item : m_children)
    {
        item->show();
    }
}

void CAttributeView::FocusVar(QString path)
{
    IVariable* var = GetVarFromPath(path);

    if (var != nullptr)
    {
        CAttributeItem* item = findItemByVar(var);
        ShowFromAttributePath(item->getAttributePath(false));
        if (item != nullptr)
        {
            item->nextInFocusChain()->setFocus();
        }
    }
}

void CAttributeView::ShowFromAttributePath(QString path)
{
    QStringList items = path.split('.');
    QString current = items.takeFirst();
    //first item should be the panel

    for (unsigned int i = 0; i < m_panels.count(); i++)
    {
        if (m_panels[i] == nullptr)
        {
            continue;
        }
        QDockWidget* panel = static_cast<QDockWidget*>(m_panels[i]);
        if (panel == nullptr)
        {
            continue;
        }

        QString title = panel->windowTitle();
        if (title.compare(current, Qt::CaseInsensitive) == 0)
        {
            PanelTitleBar* titleBar = static_cast<PanelTitleBar*> (panel->titleBarWidget());
            if (titleBar != nullptr)
            {
                titleBar->setCollapsed(false);
            }
        }
    }
}

void CAttributeView::SetCallbackEmitterEnableToggle(std::function<void(bool)> callback)
{
    m_callbackEmitterEnableToggled = callback;
}

void CAttributeView::DecorateDefaultView()
{
    CRY_ASSERT(m_defaultView);
    m_defaultView->SetSpaceAtTop(100);
    m_defaultView->SetLabel(tr("Select an individual emitter to edit attributes"));
}

void CAttributeView::ShowDefaultView()
{
    m_scrollArea->hide();
    m_defaultView->show();
}

void CAttributeView::HideDefaultView()
{
    CRY_ASSERT(m_defaultView);
    CRY_ASSERT(m_scrollArea);
    CRY_ASSERT(m_scrollAreaWidget);
    m_defaultView->hide();
    m_scrollArea->show();
}

void CAttributeView::AddLayoutMenu(QMenu* menu)
{
    menu->setMouseTracking(true);
    QAction* actLoad = menu->addAction(tr("Import..."));
    connect(actLoad, &QAction::triggered, this, [this]()
        {
            QString fileName = QFileDialog::getOpenFileName(this, tr("Load attribute layout"), QString(), tr("Layout file (*.attribute_layout)"));
            if (fileName.size() > 0)
            {
                LoadAttributeConfig(fileName);
            }
        });
    ///////////////////////////
    // Custom panel menus
    QMenu* actCustomMenu = menu->addMenu(tr("Custom attributes"));
    // New Attribute
    QAction* newAttribute = actCustomMenu->addAction(tr("New attribute"));
    connect(newAttribute, &QAction::triggered, this, &CAttributeView::CreateNewCustomPanel);
    QAction* importAttribute = actCustomMenu->addAction(tr("Import attribute..."));
    connect(importAttribute, &QAction::triggered, this, [this]()
        {
            QString filename = QFileDialog::getOpenFileName(this, tr("Import custom attribute"), QString(), tr("Attribute file (*.custom_attribute)"));
            if (filename.size() > 0)
            {
                ImportPanel(filename);
            }
        });

    // Build Custom panel list
    emit SignalBuildCustomAttributeList(actCustomMenu);

    // Reset to Default Attributes List
    QAction* resetList = actCustomMenu->addAction(tr("Reset list"));
    connect(resetList, &QAction::triggered, this, &CAttributeView::SignalResetPresetList);

    if (m_defaultView->isVisible())
    {
        actCustomMenu->setDisabled(true);
    }

    // End Custom Panel menu
    /////////////////////////////////////////////////////

    // Export
    QAction* actSave = menu->addAction(tr("Export..."));
    connect(actSave, &QAction::triggered, this, [this]()
        {
            // get save filename
            QString fileName = QFileDialog::getSaveFileName(this, tr("Save attribute layout"), QString(), tr("Layout file (*.attribute_layout)"));
            if (fileName.size() > 0)
            {
                SaveAttributeConfig(fileName);
            }
        });

    QAction* actReset = menu->addAction(tr("Reset to default"));
    connect(actReset, &QAction::triggered, this, &CAttributeView::ResetToDefaultLayout);
}

void CAttributeView::SaveAttributeConfig(QString filepath)
{
    QByteArray out;
    SaveAttributeConfig(out);
    QFile file(filepath);
    file.open(QIODevice::WriteOnly);
    file.write(out);
    file.close();
}

void CAttributeView::SaveAttributeConfig(QByteArray& out)
{
    //this is the xml describing the panels that need to be built on load
    QByteArray xml;
    WriteToBuffer(xml);

    //this is the binary data representing position and size of panels
    QByteArray panelLayout = m_scrollAreaWidget->saveAll();
    qint64 size = panelLayout.length();
    quint32 fileVersion = PARTICLE_EDITOR_LAYOUT_VERSION;
    out.resize(sizeof(size) + xml.size() + panelLayout.size());
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << fileVersion;
    stream << size;
    stream.writeRawData(xml, xml.size());
    stream.writeRawData(panelLayout, panelLayout.size());
}

bool CAttributeView::LoadAttributeConfig(QByteArray data)
{
    QDataStream in(data);    // simplifies handling the data
    in.setByteOrder(QDataStream::BigEndian);

    qint64 binarySize;
    quint32 fileVersion;
    in >> fileVersion;

    switch (fileVersion)
    {
    case 0x1:
    case 0x2:
    case 0x3:
    {
        if (fileVersion != PARTICLE_EDITOR_LAYOUT_VERSION)
        {
            GetIEditor()->GetLogFile()->Warning("The attribute layout format is out of date. Please export a new one.\n");
        }

        in >> binarySize;
        int dataSize = data.size();
        int xmlsize = dataSize - binarySize - sizeof(binarySize) - sizeof(fileVersion);

        if (xmlsize <= 0 || dataSize == 0)
        {
            return false;
        }

        // Read configuration
        QByteArray rawdata;
        rawdata.resize(xmlsize);
        int readSize = in.readRawData(rawdata.data(), xmlsize);

        if (!LoadAttributeXml(rawdata))
        {
            return false;
        }

        // Load Layout
        QByteArray binData;
        binData.resize(binarySize);
        in.readRawData(binData.data(), binarySize);
        m_scrollAreaWidget->loadAll(binData);

        // Refresh Attribute View
        emit SignalRefreshAttributeView();
        return true;
        break;
    }
    default:
        CorrectLayout(data);
        return false;
        break;
    }
}

void CAttributeView::CorrectLayout(QByteArray& data)
{
    QMessageBox::warning(this, "Warning", tr("The attribute layout format is out of date. Please export a new one.\n"), QMessageBox::Button::Close);
}

void CAttributeView::LoadAttributeConfig(QString filepath)
{
    if (!filepath.contains(".attribute_layout"))
    {
        // ErrorMsg Wrong fileformat;
        QString text = "The editor can only load attribute layout file with extension \".attribute_layout\".\n";
        QMessageBox::warning(this, "Warning", text, "Ok");
        return;
    }

    QMessageLogger log;
    QFile file(filepath);
    file.open(QIODevice::ReadOnly);
    QByteArray data = file.readAll();

    if (!LoadAttributeConfig(data))
    {
        QString text = "Fail to load Layout Config file \"" + filepath + "\".\n";
        QMessageBox::warning(this, "Warning", text, "Ok");
        return;
    }
}

//shows attribute groups with a window title == groupVar,
//check multiple attributes with && i.e. "group&&comment"
void CAttributeView::ShowAllButGroup(const QString groupVar)
{
    if (m_attributeViewStatus == 0) // if status not changed, return
    {
        return;
    }

    QStringList groupNames = groupVar.split(QTUI_UTILS_NAME_SEPARATOR);

    //show all groups, then hide the ones we don't want to see.
    for (unsigned int i = 0; i < m_panels.count(); i++)
    {
        CAttributeListView* view = static_cast<CAttributeListView*>(m_panels[i]);
        if (view->isHideInEmitter() || groupNames.lastIndexOf(view->windowTitle()) != -1)
        {
            view->hide();
        }
        else
        {
            view->show();
        }
    }


    m_attributeViewStatus = 0;
}

//hides attribute groups with a window title == groupVar,
//check multiple attributes with && i.e. "group&&comment"
void CAttributeView::HideAllButGroup(const QString groupVar)
{
    if (m_attributeViewStatus == 1) // if status not changed, return
    {
        return;
    }
    QStringList groupNames = groupVar.split(QTUI_UTILS_NAME_SEPARATOR);

    //hide all groups, then show the ones we want to see.
    //show all groups, then hide the ones we don't want to see.
    for (unsigned int i = 0; i < m_panels.count(); i++)
    {
        CAttributeListView* view = static_cast<CAttributeListView*>(m_panels[i]);
        if (view->isShowInGroup() || groupNames.lastIndexOf(view->windowTitle()) != -1)
        {
            view->show();
        }
        else
        {
            view->hide();
        }
    }
    m_attributeViewStatus = 1;
}

bool CAttributeView::LoadAttributeXml(const QByteArray& data)
{
    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorColumn;
    if (!doc.setContent(data, &errorMsg, &errorLine, &errorColumn))
    {
        qDebug() << data;
        return false;
    }
    // Load Panel Attribute
    OnStartReload();
    CAttributeViewConfig viewconfig(doc);
    // Reset Configuration
    SetConfiguration(viewconfig, m_uiVars);

    // Fill From configuration
    RestoreScrollPosition();
    BuildCollapseUncollapseAllMenu();

    FillFromConfiguration();

    m_fillCount++;

    OnEndReload();

    showChildren();
    return true;
}

void CAttributeView::ResetToDefaultLayout()
{
    //remove custom panels
    m_scrollAreaWidget->ClearCustomPanels();

    //reset geometry
    CRY_ASSERT(m_scrollAreaWidget);
    m_scrollAreaWidget->ResetGeometry();

    //force status change, so the Group panel will be show/hide properly.
    m_attributeViewStatus = 1 - m_attributeViewStatus;

    emit SignalRefreshAttributeView();
}

// Rename Custom Panel
bool CAttributeView::RenamePanel(QDockWidget* panel, QString origName, QString newName)
{
    ItemPanelTitleBar* bar = static_cast<ItemPanelTitleBar*> (panel->titleBarWidget());
    if (PanelExist(newName) && origName != newName)
    {
        // TODO: ErrorDialog
        QString text = "Custom attributes panel with name \"" + newName + "\" already exists\n";
        QMessageBox::warning(this, "Warning", text, "Ok");
        newName = origName;
        bar->StartRename();
        return false;
    }
    else
    {
        panel->setWindowTitle(newName);
        for (int i = 0; i < m_custompanels.size(); i++)
        {
            if (m_custompanels[i] == origName)
            {
                m_custompanels.replace(i, newName);
                break;
            }
        }
    }

    bar->RenamePanelName(newName);
    return true;
}

void CAttributeView::SetCurrentItem(CParticleItem* currentSelectedItem)
{
    m_currentItem = currentSelectedItem;
}

CParticleItem* CAttributeView::GetCurrentItem()
{
    return m_currentItem;
}

bool CAttributeView::ValidateUserChangeEnum(IVariable* var, const QString& newText)
{
    if (var == GetVarFromPath("Emitter.Emitter_Shape") || var == GetVarFromPath("Emitter.Emitter_Gpu_Shape"))
    {
        return ValidateEmitterShapeChange();
    }
    else if (var == GetVarFromPath("Emitter.Inheritance"))
    {
        return ValidateInheritance(newText);
    }
    return true;
}


bool CAttributeView::HandleUserChangedEnum(IVariable* var, const QString& newVar)
{
    const bool isCpuShape = (var == GetVarFromPath("Emitter.Emitter_Shape"));
    const bool isGpuShape = (var == GetVarFromPath("Emitter.Emitter_Gpu_Shape"));

    if (isCpuShape || isGpuShape)
    {
        // In order to preserve user changes and apply them to the new shape...
        // First serialize the current state, with the shape default as the defaultParams. This will give use just the original shape-specific diffs.
        XmlNodeRef node = GetIEditor()->GetSystem()->CreateXmlNode("Particles");
        CBaseLibraryItem::SerializeContext context(node, false);
        context.bIgnoreChilds = true;
        context.bLoading = false;
        const ParticleParams& defaultParams = GetIEditor()->GetParticleManager()->GetEmitterDefaultParams(m_currentItem->GetEffect()->GetParticleParams().eEmitterShape);
        m_currentItem->Serialize(context, &defaultParams);

        // Apply the new shape value and load the defaults for that shape.
        var->SetDisplayValue(newVar.toUtf8().data());
        ResolveVisibility();
        GetIEditor()->GetParticleManager()->LoadDefaultEmitter(m_currentItem->GetEffect());

        // Finally, deserialize the diff we saved above, over top the new parameter state.
        context.bLoading = true;
        m_currentItem->Serialize(context, &m_currentItem->GetEffect()->GetParticleParams());


        m_currentItem->Update();

        // Call this refresh to update the m_lod pointers in LODLevelWidget
        EBUS_EVENT(EditorUIPlugin::LibraryItemUIRequests::Bus, RefreshItemUI);

        emit SignalRefreshAttributes();
        OnAttributeItemUndoPoint();
        return true;
    }
    else if (var == GetVarFromPath("Angles.Facing"))
    {
        //keep gpu facing sync
        IVariable* gpuVar = GetVarFromPath("Angles.Facing_Gpu");
        gpuVar->SetDisplayValue(newVar.toUtf8().constData());
        return false;

    }
    else if (var == GetVarFromPath("Angles.Facing_Gpu"))
    {
        //keep cpu facing sync
        IVariable* cpuVar = GetVarFromPath("Angles.Facing");
        cpuVar->SetDisplayValue(newVar.toUtf8().constData());
        return false;
    }
    else
    {
        return false;
    }

}

void CAttributeView::OnAttributeItemUndoPoint()
{
    bool isSuspend = false;
    EBUS_EVENT_RESULT(isSuspend, EditorLibraryUndoRequestsBus, IsSuspend);

    if (isSuspend || !m_currentItem)
    {
        return;
    }

    emit SignalItemUndoPoint(m_currentItem->GetFullName());
}
