#include "stdafx.h"
#include "SceneTreeView.h"

#include <QtCore/QStack>
#include <algorithm>
#include <vector>
#include "files.h"
#include "glm_code.h"
#include "r_model.h"
#include "SceneTreeItem.h"
#include "SceneTreeModel.h"
#include "sequence.h"

struct BoneTreeApplication
{
    ModelContainer_t *container;
    mdxaSkelOffsets_t *pSkeletonOffsets;
    QStack<SceneTreeItem *> nodes;
};

void BeforeBoneChildrenAdded ( mdxaSkel_t *bone, int index, void *userData )
{
    BoneTreeApplication *app = static_cast<BoneTreeApplication *>(userData);

    SceneTreeItem *item = new SceneTreeItem (MODELRESOURCE_BONE, bone, index, bone->name, app->container->hModel, app->nodes.back());
    app->nodes.back()->AddChild (item);
    app->nodes.append (item);
}

void AfterBoneChildrenAdded ( mdxaSkel_t *, int index, void *userData )
{
    BoneTreeApplication *app = static_cast<BoneTreeApplication *>(userData);

    app->nodes.pop();
}

void BoneChildrenAdded ( mdxaSkel_t *bone, int boneIndex, void *userData )
{
    BoneTreeApplication *app = static_cast<BoneTreeApplication *>(userData);

    R_GLM_BoneRecursiveApply (
        app->container->hModel,
        boneIndex,
        app->pSkeletonOffsets,
        BeforeBoneChildrenAdded,
        BoneChildrenAdded,
        AfterBoneChildrenAdded,
        static_cast<void *>(app));
}

struct SurfaceTreeApplication
{
    ModelContainer_t *container;
    mdxmHierarchyOffsets_t *pHierarchyOffsets;
    QStack<SceneTreeItem *> nodes;
};

void BeforeSurfaceChildrenAdded ( mdxmSurfHierarchy_t *surface, int index, void *userData )
{
    SurfaceTreeApplication *app = static_cast<SurfaceTreeApplication *>(userData);

    SceneTreeItem *item = new SceneTreeItem (MODELRESOURCE_SURFACE, surface, index, surface->name, app->container->hModel, app->nodes.back());
    app->nodes.back()->AddChild (item);
    app->nodes.append (item);
}

void AfterSurfaceChildrenAdded ( mdxmSurfHierarchy_t *surface, int index, void *userData )
{
    SurfaceTreeApplication *app = static_cast<SurfaceTreeApplication *>(userData);

    app->nodes.pop();
}

void SurfaceChildrenAdded ( mdxmSurfHierarchy_t *surface, int surfaceIndex, void *userData )
{
    SurfaceTreeApplication *app = static_cast<SurfaceTreeApplication *>(userData);

    R_GLM_SurfaceRecursiveApply (
        app->container->hModel,
        surfaceIndex,
        app->pHierarchyOffsets,
        BeforeSurfaceChildrenAdded,
        SurfaceChildrenAdded,
        AfterSurfaceChildrenAdded,
        static_cast<void *>(app));
}

void BeforeTagChildrenAdded ( mdxmSurfHierarchy_t *surface, int index, void *userData )
{
    SurfaceTreeApplication *app = static_cast<SurfaceTreeApplication *>(userData);

    if ( surface->flags & G2SURFACEFLAG_ISBOLT )
    {
        app->nodes.back()->AddChild (new SceneTreeItem (MODELRESOURCE_TAG, surface, index, surface->name, app->container->hModel, app->nodes.back()));
    }
}

void TagChildrenAdded ( mdxmSurfHierarchy_t *surface, int index, void *userData )
{
    SurfaceTreeApplication *app = static_cast<SurfaceTreeApplication *>(userData);

    R_GLM_SurfaceRecursiveApply (
        app->container->hModel,
        index,
        app->pHierarchyOffsets,
        BeforeTagChildrenAdded,
        TagChildrenAdded,
        NULL,
        static_cast<void *>(app));
}

struct CompareSequencesByFrame
{
    const SequenceList_t& sequences;

    CompareSequencesByFrame ( const SequenceList_t& sequences )
        : sequences (sequences)
    {
    }

    bool operator () ( int left, int right )
    {
        return sequences[left].iStartFrame < sequences[right].iStartFrame;
    }
};

struct CompareSequencesByName
{
    const SequenceList_t& sequences;

    CompareSequencesByName ( const SequenceList_t& sequences )
        : sequences (sequences)
    {
    }

    bool operator () ( int left, int right )
    {
        return std::strcmp (sequences[left].sName, sequences[right].sName) < 0;
    }
};

void AddSequencesToTree ( SceneTreeItem *root, const ModelContainer_t& container, const SequenceList_t& sequenceList )
{
    std::vector<int> sequenceIndices (sequenceList.size(), -1);
    for ( std::size_t i = 0; i < sequenceList.size(); i++ )
    {
        sequenceIndices[i] = i;
    }

    std::sort (sequenceIndices.begin(), sequenceIndices.end(), CompareSequencesByFrame (sequenceList));

    for ( std::size_t i = 0; i < sequenceList.size(); i++ )
    {
        root->AddChild (new SceneTreeItem (MODELRESOURCE_ANIMSEQUENCE, &sequenceList[sequenceIndices[i]], sequenceIndices[i], sequenceList[sequenceIndices[i]].sName, container.hModel, root));
    }
}

void AddSkinsToTree ( SceneTreeItem *root, const ModelContainer& container, const OldSkinSets_t& skins )
{
    int skinIndex = 0;
    for ( OldSkinSets_t::const_iterator skin = skins.begin(); skin != skins.end(); skin++, skinIndex++ )
    {
        root->AddChild (new SceneTreeItem (MODELRESOURCE_SKIN, skin->first.c_str(), skinIndex, skin->first.c_str(), container.hModel, root));
    }
}

void ClearSceneTreeModel ( SceneTreeModel& model )
{
	model.clear();
}

void SetupSceneTreeModel(
    const QString& modelName, ModelContainer_t& container,
    SceneTreeModel& model)
{
    SceneTreeItem* root = new SceneTreeItem(
        MODELRESOURCE_NULL, NULL, -1, "", container.hModel, 0);

    SceneTreeItem* modelItem = new SceneTreeItem(
        MODELRESOURCE_NULL,
        NULL,
        -1,
        QString("==> %1 <==")
            .arg(QString::fromLatin1(
                Filename_WithoutPath(modelName.toLatin1()))),
        container.hModel,
        root);

    root->AddChild(modelItem);

    SceneTreeItem* surfacesItem = new SceneTreeItem(
        MODELRESOURCE_SURFACE,
        NULL,
        -1,
        QObject::tr("Surfaces"),
        container.hModel,
        modelItem);
    SceneTreeItem* tagsItem = new SceneTreeItem(
        MODELRESOURCE_TAG,
        NULL,
        -1,
        QObject::tr("Tags"),
        container.hModel,
        modelItem);
    SceneTreeItem* bonesItem = new SceneTreeItem(
        MODELRESOURCE_BONE,
        NULL,
        -1,
        QObject::tr("Bones"),
        container.hModel,
        modelItem);

    mdxmHeader_t* pMDXMHeader =
        (mdxmHeader_t*)RE_GetModelData(container.hModel);
    mdxaHeader_t* pMDXAHeader =
        (mdxaHeader_t*)RE_GetModelData(pMDXMHeader->animIndex);
    mdxmHierarchyOffsets_t* pHierarchyOffsets =
        (mdxmHierarchyOffsets_t*)((byte*)pMDXMHeader + sizeof(*pMDXMHeader));

    SurfaceTreeApplication surfaceApp;
    surfaceApp.container = &container;
    surfaceApp.pHierarchyOffsets = pHierarchyOffsets;

    // Add surfaces
    surfaceApp.nodes.append(surfacesItem);
    R_GLM_SurfaceRecursiveApply(
        container.hModel,
        0,
        pHierarchyOffsets,
        BeforeSurfaceChildrenAdded,
        SurfaceChildrenAdded,
        AfterSurfaceChildrenAdded,
        static_cast<void*>(&surfaceApp));

    int numSurfacesInTree = surfacesItem->ChildCountRecursive();
    if (numSurfacesInTree != pMDXMHeader->numSurfaces)
    {
        ErrorBox(va(
            "Model has %d surfaces, but only %d of them are connected through "
            "the heirarchy, the rest will never be recursed into.\n\n"
            "This model needs rebuilding.",
            pMDXMHeader->numSurfaces,
            numSurfacesInTree));
    }

    // Add tags
    surfaceApp.nodes.clear();
    surfaceApp.nodes.append(tagsItem);
    R_GLM_SurfaceRecursiveApply(
        container.hModel,
        0,
        pHierarchyOffsets,
        BeforeTagChildrenAdded,
        TagChildrenAdded,
        NULL,
        static_cast<void*>(&surfaceApp));

    // Add bones
    mdxaSkelOffsets_t* pSkelOffsets =
        (mdxaSkelOffsets_t*)((byte*)pMDXAHeader + sizeof(*pMDXAHeader));
    BoneTreeApplication boneApp;
    boneApp.container = &container;
    boneApp.nodes.append(bonesItem);
    boneApp.pSkeletonOffsets = pSkelOffsets;

    R_GLM_BoneRecursiveApply(
        container.hModel,
        0,
        pSkelOffsets,
        BeforeBoneChildrenAdded,
        BoneChildrenAdded,
        AfterBoneChildrenAdded,
        static_cast<void*>(&boneApp));

    // Add skins
    SceneTreeItem* skinsItem = new SceneTreeItem(
        MODELRESOURCE_SKIN,
        NULL,
        -1,
        QObject::tr("Skins"),
        container.hModel,
        modelItem);
    AddSkinsToTree(skinsItem, container, container.OldSkinSets);

    // Add animation sequences
    SceneTreeItem* sequencesItem = NULL;
    if (!container.SequenceList.empty())
    {
        sequencesItem = new SceneTreeItem(
            MODELRESOURCE_ANIMSEQUENCE,
            NULL,
            -1,
            QObject::tr("Sequences"),
            container.hModel,
            modelItem);
        AddSequencesToTree(sequencesItem, container, container.SequenceList);
    }

    // And add the items to model!
    modelItem->AddChild(surfacesItem);

    if (tagsItem->ChildCount() > 0)
    {
        modelItem->AddChild(tagsItem);
    }

    modelItem->AddChild(skinsItem);
    modelItem->AddChild(bonesItem);

    if (sequencesItem != NULL)
    {
        modelItem->AddChild(sequencesItem);
    }

    model.setRoot(root);
}
