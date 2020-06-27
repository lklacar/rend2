#include "MainForm.h"
#include "stdafx.h"

#include "ChangeFloorDialog.h"
#include "GUIHelpers.h"
#include "SceneTreeItem.h"
#include "SceneTreeModel.h"
#include "SceneTreeView.h"
#include "generic_stuff.h"
#include "model.h"
#include "text.h"
#include "textures.h"
#include <QClipboard>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

namespace {
void StartRenderTimer(QWidget* parent, RenderWidget* renderWidget)
{
    QTimer* fpsTimer = new QTimer(parent);
    fpsTimer->setInterval(10);
    QObject::connect(
        fpsTimer, SIGNAL(timeout()), parent, SLOT(OnUpdateAnimation()));
    fpsTimer->start();
}
} // namespace

MainForm::MainForm(QSettings& settings, QWidget* parent)
    : QMainWindow(parent), settings(&settings),
      treeModel(new SceneTreeModel(this))
{
    ui.setupUi(this);

    ui.treeView->setModel(treeModel);
    ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        ui.treeView,
        &QTreeView::customContextMenuRequested,
        this,
        &MainForm::OnRightClickTreeView);

    PopulateMRUFiles(settings.value("mru").toStringList());

    CurrentSceneName(tr("Untitled"));

    connect(
        ui.renderWidget,
        SIGNAL(tookScreenshotForClipboard(const QImage&)),
        SLOT(OnScreenshotTakenForClipboard(const QImage&)));

    StartRenderTimer(this, ui.renderWidget);
}

void MainForm::PopulateMRUFiles(const QStringList& mruFiles)
{
    ui.menuRecent_Files->clear();

    if (!mruFiles.empty())
    {
        for (int i = 0, j = 1; i < mruFiles.size(); i++, j++)
        {
            QAction* action = new QAction(
                QString("%1. %2").arg(j).arg(mruFiles[i]), ui.menuRecent_Files);
            action->setData(mruFiles[i]);

            connect(action, SIGNAL(triggered()), SLOT(OnOpenRecentModel()));

            ui.menuRecent_Files->addAction(action);
        }

        ui.menuRecent_Files->addSeparator();
        ui.menuRecent_Files->addAction(
            tr("Clear recent files list"), this, SLOT(OnClearRecentFiles()));
    }
    else
    {
        QAction* action =
            new QAction(tr("No recently used files"), ui.menuRecent_Files);
        action->setDisabled(true);

        ui.menuRecent_Files->addAction(action);
    }
}

void MainForm::OnScreenshotTakenForClipboard(const QImage& image)
{
    QApplication::clipboard()->setImage(image);
}

void MainForm::OnClearRecentFiles()
{
    QStringList emptyList;

    settings->setValue("mru", emptyList);
    PopulateMRUFiles(emptyList);
}

void MainForm::OnOpenRecentModel()
{
    QString filename = static_cast<QAction*>(sender())->data().toString();
    OpenModel(filename);
}

void MainForm::OnUpdateAnimation()
{
    if (ModelList_Animation())
    {
        ui.renderWidget->update();
    }
}

void MainForm::OnAbout()
{
    QMessageBox about(this);
    about.setWindowTitle(tr("About ModView"));
    about.setText(
        tr("<p><b>ModView 3.0</b><br />"
           "Written by Alex 'Xycaleth' Lo.</p>"
           "<p><b>ModView 2.5</b><br />"
           "Written by Ste Cork and Mike Crowns.</p>"
           "<p>Copyright (c) 2000 - 2013, Raven Software.<br />"
           "Released under GNU General Public License, version 2.0.</p>"
           "<p>Current formats supported: Ghoul 2 (.glm, .gla)</p>"));
    about.setIconPixmap(QPixmap(":/images/res/modview.ico"));
    about.exec();
}

void MainForm::OnChangeBackgroundColor(const QColor& color)
{
    AppVars._R = color.red();
    AppVars._G = color.green();
    AppVars._B = color.blue();
}

void MainForm::OnChooseBackgroundColor()
{
    QColorDialog* dialog = new QColorDialog(this);
    QColor color(AppVars._R, AppVars._G, AppVars._B);

    dialog->setCurrentColor(color);
    dialog->open(this, SLOT(OnChangeBackgroundColor(const QColor&)));
}

void MainForm::OnNewScene()
{
    Model_Delete();
    CurrentSceneName(tr("Untitled"));
    ClearSceneTreeModel(*treeModel);

    ModelList_ForceRedraw();
}

void MainForm::OnOpenModel()
{
    const char* directory = Filename_PathOnly(Model_GetFullPrimaryFilename());
    std::string modelName(OpenGLMDialog(this, directory));

    if (modelName.empty())
    {
        return;
    }

    OpenModel(QString::fromStdString(modelName));
}

void MainForm::OpenModel(const QString& modelPath)
{
    if (Model_LoadPrimary(modelPath.toLatin1()))
    {
        CurrentSceneName(modelPath);
        SetupSceneTreeModel(modelPath, AppVars.Container, *treeModel);

        QStringList mruList = settings->value("mru").toStringList();
        int pos = mruList.indexOf(modelPath);

        if (pos == -1)
        {
            // Not in the MRU list. Add it!
            mruList.push_front(modelPath);
            if (mruList.size() > 5)
            {
                mruList.pop_back();
            }
        }
        else if (pos > 0)
        {
            // 0 implies top of the list.
            // Anything more than that, then we want to move the file
            // to the top of the list.

            mruList.move(pos, 0);
        }

        settings->setValue("mru", mruList);

        PopulateMRUFiles(mruList);
    }
}

void MainForm::CurrentSceneName(const QString& sceneName)
{
    setWindowTitle(tr("%1 - ModView").arg(sceneName));
}

void MainForm::OnResetViewPoint()
{
    AppVars_ResetViewParams();
    ModelList_ForceRedraw();
}

void MainForm::OnAnimationStart() { Model_StartAnim(); }

void MainForm::OnAnimationPause() { Model_StopAnim(); }

void MainForm::OnAnimationStartWithLooping() { Model_StartAnim(true); }

void MainForm::OnAnimationRewind()
{
    ModelList_Rewind();
    ModelList_ForceRedraw();
}

void MainForm::OnAnimationGoToEndFrame()
{
    ModelList_GoToEndFrame();
    ModelList_ForceRedraw();
}

void MainForm::OnAnimationSpeedUp()
{
    double fps = 1.0 / AppVars.dAnimSpeed;
    fps += 2.0;

    AppVars.dAnimSpeed = 1.0 / fps;

    // Need to update the FPS text
    ModelList_ForceRedraw();
}

void MainForm::OnAnimationSlowDown()
{
    double fps = 1.0 / AppVars.dAnimSpeed;
    fps -= 2.0;

    if (fps <= 0.5)
    {
        return;
    }

    AppVars.dAnimSpeed = 1.0 / fps;

    // Need to update the FPS text
    ModelList_ForceRedraw();
}

void MainForm::OnToggleInterpolation()
{
    AppVars.bInterpolate = !AppVars.bInterpolate;
    ModelList_ForceRedraw();
}

void MainForm::OnNextFrame() { ModelList_StepFrame(1); }

bool InitFileSystem(
    const std::string& gameDataPath, const std::string& baseDir);
void MainForm::OnPrevFrame()
{
    InitFileSystem("/Users/alex/Games/JKA", "base");
    ModelList_StepFrame(-1);
}

void MainForm::OnScreenshotToFile()
{
    if (Model_Loaded())
    {
        std::string baseName(Filename_WithoutPath(
            Filename_PathOnly(Model_GetFullPrimaryFilename())));
        std::string screenshotName = baseName + "_" +
                                     QDateTime::currentDateTime()
                                         .toString("yyyy-MM-dd_hh-mm-ss")
                                         .toStdString() +
                                     ".png";

        QString fileToSave = QFileDialog::getSaveFileName(
            this,
            QString(),
            QString::fromStdString(screenshotName),
            "PNG image (*.png)");

        if (!fileToSave.isEmpty())
        {
            AppVars.takeScreenshot = true;
            AppVars.screenshotFileName = fileToSave.toStdString();
            ModelList_ForceRedraw();
        }
    }
}

void MainForm::OnScreenshotToClipboard()
{
    if (Model_Loaded())
    {
        std::string baseName(Filename_WithoutPath(
            Filename_PathOnly(Model_GetFullPrimaryFilename())));

        AppVars.takeScreenshot = true;
        AppVars.takeScreenshotForClipboard = true;
        AppVars.screenshotFileName.clear();
        ModelList_ForceRedraw();
    }
}

void MainForm::OnToggleCleanScreenshot()
{
    AppVars.bCleanScreenShots = !AppVars.bCleanScreenShots;
}

void MainForm::OnToggleBoundingBox()
{
    AppVars.bBBox = !AppVars.bBBox;
    ModelList_ForceRedraw();
}

void MainForm::OnToggleHeightRuler()
{
    AppVars.bRuler = !AppVars.bRuler;
    ModelList_ForceRedraw();
}

void MainForm::OnToggleOriginAxes()
{
    AppVars.bOriginLines = !AppVars.bOriginLines;
    ModelList_ForceRedraw();
}

void MainForm::OnToggleFloor()
{
    AppVars.bFloor = !AppVars.bFloor;
    ModelList_ForceRedraw();
}

void MainForm::OnToggleTwoSidedSurfaces()
{
    AppVars.bShowPolysAsDoubleSided = !AppVars.bShowPolysAsDoubleSided;
    ModelList_ForceRedraw();
}

void MainForm::OnCycleFieldOfView()
{
    const float VALID_FOVS[] = {10.0f, 80.0f, 90.0f};
    const std::size_t NUM_FOVS = sizeof(VALID_FOVS) / sizeof(VALID_FOVS[0]);
    for (int i = 0; i < NUM_FOVS; i++)
    {
        if (AppVars.dFOV == VALID_FOVS[i])
        {
            AppVars.dFOV = VALID_FOVS[(i + 1) % NUM_FOVS];
            break;
        }
    }

    ModelList_ForceRedraw();
}

void MainForm::OnIncreaseLOD()
{
    // Decrement, because lower LOD *number*,
    // means an *increased* level of detail in the model.
    ChangeLOD(AppVars.iLOD - 1);
}

void MainForm::OnDecreaseLOD()
{
    // Increment, because higher LOD *number*,
    // means a *decreased* level of detail in the model.
    ChangeLOD(AppVars.iLOD + 1);
}

void MainForm::OnPicmipTo0() { ChangePicmip(0); }
void MainForm::OnPicmipTo1() { ChangePicmip(1); }
void MainForm::OnPicmipTo2() { ChangePicmip(2); }
void MainForm::OnPicmipTo3() { ChangePicmip(3); }

void MainForm::ChangeLOD(int lod)
{
    if (lod < 0)
    {
        lod = 0;
    }
    else if (lod >= AppVars.Container.iNumLODs)
    {
        lod = AppVars.Container.iNumLODs - 1;
    }

    AppVars.iLOD = lod;
    ModelList_ForceRedraw();
}

void MainForm::ChangePicmip(int level)
{
    TextureList_ReMip(level);
    ModelList_ForceRedraw();
}

void MainForm::OnRefreshTextures()
{
    TextureList_Refresh();
    ModelList_ForceRedraw();
}

void MainForm::OnDoubleClickedTreeView(const QModelIndex& index)
{
    if (!index.isValid())
    {
        return;
    }

    SceneTreeItem* item = static_cast<SceneTreeItem*>(index.internalPointer());
    item->DoubleClick();
}

void MainForm::OnClickedTreeView(const QModelIndex& index)
{
    if (!index.isValid())
    {
        return;
    }

    SceneTreeItem* item = static_cast<SceneTreeItem*>(index.internalPointer());

    ModelHandle_t model = item->GetModel();
    Model_SetBoneHighlight(model, iITEMHIGHLIGHT_NONE);
    Model_SetSurfaceHighlight(model, iITEMHIGHLIGHT_NONE);

    item->LeftClick();
}

void MainForm::OnRightClickTreeView(const QPoint& point)
{
    QModelIndex selectedIndex = ui.treeView->indexAt(point);
    if (!selectedIndex.isValid())
    {
        return;
    }

    SceneTreeItem* item =
        static_cast<SceneTreeItem*>(selectedIndex.internalPointer());
    item->RightClick();
}

void MainForm::OnToggleVertexNormals()
{
    AppVars.bVertexNormals = !AppVars.bVertexNormals;
    ModelList_ForceRedraw();
}

void MainForm::OnToggleWireframe()
{
    AppVars.bWireFrame = !AppVars.bWireFrame;
    ModelList_ForceRedraw();
}

void MainForm::OnZoomToFit() {}

void MainForm::OnChangeFloorPosition()
{
    ChangeFloorDialog dlg(AppVars, this);

    connect(&dlg, SIGNAL(FloorYChanged(int)), SLOT(OnFloorHeightChanged(int)));

    dlg.exec();
}
