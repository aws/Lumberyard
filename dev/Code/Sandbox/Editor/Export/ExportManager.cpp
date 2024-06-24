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

#include "Objects/SelectionGroup.h"
#include "Objects/BaseObject.h"
#include "Objects/BrushObject.h"
#include "Objects/CameraObject.h"
#include "Geometry/EdGeometry.h"
#include "IIndexedMesh.h"
#include "I3DEngine.h"
#include "IAttachment.h"
#include "Objects/EntityObject.h"
#include "Material/Material.h"
#include "Terrain/TerrainManager.h"
#include "VegetationMap.h"
#include "VegetationObject.h"
#include "ViewManager.h"
#include "ExportManager.h"
#include "OBJExporter.h"
#include "OCMExporter.h"
#include "FBXExporterDialog.h"
#include "RenderViewport.h"
#include "TrackViewExportKeyTimeDlg.h"
#include "AnimationContext.h"
#include "TrackView/TrackViewCameraNode.h"
#include "TrackView/DirectorNodeAnimator.h"
#include "Components/IComponentCamera.h"
#include "Util/AutoDirectoryRestoreFileDialog.h"
#include "QtUI/WaitCursor.h"
#include "Maestro/Types/AnimParamType.h"
#include "Material/MaterialManager.h"

#include "QtUtil.h"

#include <QMessageBox>

namespace
{
    void SetTexture(Export::TPath& outName, IRenderShaderResources* pRes, int nSlot)
    {
        SEfResTexture* pTex = pRes->GetTextureResource(nSlot);
        if (pTex)
        {
            cry_strcat(outName, Path::GamePathToFullPath(pTex->m_Name.c_str()).toUtf8().data());
        }
    }

    inline Export::Vector3D Vec3ToVector3D(const Vec3& vec)
    {
        Export::Vector3D ret;
        ret.x = vec.x;
        ret.y = vec.y;
        ret.z = vec.z;
        return ret;
    }

    const float kTangentDelta = 0.01f;
    const float kAspectRatio = 1.777778f;
    const int kReserveCount = 7; // x,y,z,rot_x,rot_y,rot_z,fov
    const QString kMasterCameraName = "MasterCamera";
} // namespace



//////////////////////////////////////////////////////////
// CMesh
Export::CMesh::CMesh()
{
    ::ZeroMemory(&material, sizeof(material));
    material.opacity = 1.0f;
}


void Export::CMesh::SetMaterial(CMaterial* pMtl, CBaseObject* pBaseObj)
{
    if (!pMtl)
    {
        cry_strcpy(material.name, pBaseObj->GetName().toUtf8().data());
        return;
    }

    cry_strcpy(material.name, pMtl->GetFullName().toUtf8().data());

    _smart_ptr<IMaterial> matInfo = pMtl->GetMatInfo();
    IRenderShaderResources* pRes = matInfo->GetShaderItem().m_pShaderResources;
    if (!pRes)
    {
        return;
    }

    ColorF difColor = pRes->GetColorValue(EFTT_DIFFUSE);
    material.diffuse.r = difColor.r;
    material.diffuse.g = difColor.g;
    material.diffuse.b = difColor.b;
    material.diffuse.a = difColor.a;

    ColorF specColor = pRes->GetColorValue(EFTT_SPECULAR);
    material.specular.r = specColor.r;
    material.specular.g = specColor.g;
    material.specular.b = specColor.b;
    material.specular.a = specColor.a;

    material.opacity = pRes->GetStrengthValue(EFTT_OPACITY);
    material.smoothness = pRes->GetStrengthValue(EFTT_SMOOTHNESS);

    SetTexture(material.mapDiffuse, pRes, EFTT_DIFFUSE);
    SetTexture(material.mapSpecular, pRes, EFTT_SPECULAR);
    SetTexture(material.mapOpacity, pRes, EFTT_OPACITY);
    SetTexture(material.mapNormals, pRes, EFTT_NORMALS);
    SetTexture(material.mapDecal, pRes, EFTT_DECAL_OVERLAY);
    SetTexture(material.mapDisplacement, pRes, EFTT_HEIGHT);
}


//////////////////////////////////////////////////////////
// CObject
Export::CObject::CObject(const char* pName)
    : m_MeshHash(0)
{
    pos.x = pos.y = pos.z = 0;
    rot.v.x = rot.v.y = rot.v.z = 0;
    rot.w = 1.0f;
    scale.x = scale.y = scale.z = 1.0f;

    nParent = -1;

    cry_strcpy(name, pName);

    materialName[0] = '\0';

    entityType = Export::eEntity;

    cameraTargetNodeName[0] = '\0';

    m_pLastObject = 0;
}


void Export::CObject::SetMaterialName(const char* pName)
{
    cry_strcpy(materialName, pName);
}


// CExportData
void Export::CData::Clear()
{
    m_objects.clear();
}


// CExportManager
CExportManager::CExportManager()
    : m_isPrecaching(false)
    , m_pBaseObj(0)
    , m_FBXBakedExportFPS(0.0f)
    , m_fScale(100.0f)
    ,                 // this scale is used by CryEngine RC
    m_bAnimationExport(false)
    , m_bExportLocalCoords(false)
    , m_numberOfExportFrames(0)
    , m_pivotEntityObject(0)
    , m_bBakedKeysSequenceExport(true)
    , m_animTimeExportMasterSequenceCurrentTime(0.0f)
    , m_animKeyTimeExport(true)
    , m_soundKeyTimeExport(true)
    , m_bExportOnlyMasterCamera(false)
{
    RegisterExporter(new COBJExporter());
    RegisterExporter(new COCMExporter());
}


CExportManager::~CExportManager()
{
    m_data.Clear();
    for (TExporters::iterator ppExporter = m_exporters.begin(); ppExporter != m_exporters.end(); ++ppExporter)
    {
        (*ppExporter)->Release();
    }
}


bool CExportManager::RegisterExporter(IExporter* pExporter)
{
    if (!pExporter)
    {
        return false;
    }

    m_exporters.push_back(pExporter);
    return true;
}

inline f32 Sandbox2MayaFOVDeg(const f32 fov, const f32 ratio) { return RAD2DEG(2.0f * atan_tpl(tan(DEG2RAD(fov) / 2.0f) * ratio)); };
inline f32 Sandbox2MayaFOVRad2Deg(const f32 fov, const f32 ratio) { return RAD2DEG(2.0f * atan_tpl(tan(fov / 2.0f) * ratio)); };

void CExportManager::AddEntityAnimationData(const CTrackViewTrack* pTrack, Export::CObject* pObj, AnimParamType entityTrackParamType)
{
    int keyCount = pTrack->GetKeyCount();
    pObj->m_entityAnimData.reserve(keyCount * kReserveCount);

    for (int keyNumber = 0; keyNumber < keyCount; keyNumber++)
    {
        const CTrackViewKeyConstHandle currentKeyHandle = pTrack->GetKey(keyNumber);
        const float fCurrentKeytime = currentKeyHandle.GetTime();

        float trackValue = 0.0f;
        pTrack->GetValue(fCurrentKeytime, trackValue);

        ISplineInterpolator* pSpline = pTrack->GetSpline();

        if (pSpline)
        {
            Export::EntityAnimData entityData;
            entityData.dataType = (Export::AnimParamType)pTrack->GetParameterType().GetType();
            entityData.keyValue = trackValue;
            entityData.keyTime = fCurrentKeytime;

            if (entityTrackParamType == AnimParamType::Position)
            {
                entityData.keyValue *= 100.0f;
            }
            else if (entityTrackParamType == AnimParamType::FOV)
            {
                entityData.keyValue = Sandbox2MayaFOVDeg(entityData.keyValue, kAspectRatio);
            }


            ISplineInterpolator::ValueType tin;
            ISplineInterpolator::ValueType tout;
            ZeroStruct(tin);
            ZeroStruct(tout);
            pSpline->GetKeyTangents(keyNumber, tin, tout);

            float fInTantentX = tin[0];
            float fInTantentY = tin[1];

            float fOutTantentX = tout[0];
            float fOutTantentY = tout[1];

            float fTangentDelta = 0.01f;

            if (fInTantentX == 0.0f)
            {
                fInTantentX = kTangentDelta;
            }

            if (fOutTantentX == 0.0f)
            {
                fOutTantentX = kTangentDelta;
            }

            entityData.leftTangent = fInTantentY / fInTantentX;
            entityData.rightTangent = fOutTantentY / fOutTantentX;

            if (entityTrackParamType == AnimParamType::Position)
            {
                entityData.leftTangent *= 100.0f;
                entityData.rightTangent *= 100.0f;
            }

            float fPrevKeyTime = 0.0f;
            float fNextKeyTime = 0.0f;

            bool bIsFirstKey = false;
            bool bIsMiddleKey = false;
            bool bIsLastKey = false;

            if (keyNumber == 0 && keyNumber < (keyCount - 1))
            {
                const CTrackViewKeyConstHandle nextKeyHandle = pTrack->GetKey(keyNumber + 1);
                fNextKeyTime = nextKeyHandle.GetTime();

                if (fNextKeyTime != 0.0f)
                {
                    bIsFirstKey = true;
                }
            }
            else if (keyNumber > 0)
            {
                const CTrackViewKeyConstHandle prevKeyHandle = pTrack->GetKey(keyNumber - 1);
                fPrevKeyTime = prevKeyHandle.GetTime();

                if (keyNumber < (keyCount - 1))
                {
                    const CTrackViewKeyConstHandle nextKeyHandle = pTrack->GetKey(keyNumber + 1);
                    fNextKeyTime = nextKeyHandle.GetTime();

                    if (fNextKeyTime != 0.0f)
                    {
                        bIsMiddleKey = true;
                    }
                }
                else
                {
                    bIsLastKey = true;
                }
            }

            float fLeftTangentWeightValue = 0.0f;
            float fRightTangentWeightValue = 0.0f;

            if (bIsFirstKey)
            {
                fRightTangentWeightValue = fOutTantentX / fNextKeyTime;
            }
            else if (bIsMiddleKey)
            {
                fLeftTangentWeightValue = fInTantentX / (fCurrentKeytime - fPrevKeyTime);
                fRightTangentWeightValue = fOutTantentX / (fNextKeyTime - fCurrentKeytime);
            }
            else if (bIsLastKey)
            {
                fLeftTangentWeightValue = fInTantentX / (fCurrentKeytime - fPrevKeyTime);
            }

            entityData.leftTangentWeight = fLeftTangentWeightValue;
            entityData.rightTangentWeight = fRightTangentWeightValue;

            pObj->m_entityAnimData.push_back(entityData);
        }
    }
}


void CExportManager::ProcessEntityAnimationTrack(const CBaseObject* pBaseObj, Export::CObject* pObj, AnimParamType entityTrackParamType)
{
    CTrackViewAnimNode* pEntityNode = nullptr;
    if (qobject_cast<const CEntityObject*>(pBaseObj))
    {
        pEntityNode = GetIEditor()->GetSequenceManager()->GetActiveAnimNode(static_cast<const CEntityObject*>(pBaseObj));
    }

    CTrackViewTrack* pEntityTrack = (pEntityNode ? pEntityNode->GetTrackForParameter(entityTrackParamType) : 0);

    bool bIsCameraTargetObject = qobject_cast<const CCameraObjectTarget*>(pBaseObj);

    if (!pEntityTrack)
    {
        // In case its CameraTarget which does not have track
        if (bIsCameraTargetObject && entityTrackParamType != AnimParamType::FOV)
        {
            AddPosRotScale(pObj, pBaseObj);
        }

        return;
    }

    if (pEntityTrack->GetParameterType() == AnimParamType::FOV)
    {
        AddEntityAnimationData(pEntityTrack, pObj, entityTrackParamType);
        return;
    }


    bool bIsCameraObject = qobject_cast<const CCameraObject*>(pBaseObj);

    if (pEntityTrack->GetParameterType() == AnimParamType::Rotation && bIsCameraObject)
    {
        bool bShakeTrackPresent = false;
        CTrackViewTrack* pParentTrack = static_cast<CTrackViewTrack*>(pEntityTrack->GetParentNode());

        if (pParentTrack)
        {
            for (int trackID = 0; trackID < pParentTrack->GetChildCount(); ++trackID)
            {
                CTrackViewTrack* pSubTrack = static_cast<CTrackViewTrack*>(pParentTrack->GetChild(trackID));
                if (pSubTrack->GetParameterType() == AnimParamType::ShakeMultiplier)
                {
                    bShakeTrackPresent = true;
                    break;
                }
            }
        }

        if (bShakeTrackPresent)
        {
            CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

            if (!pSequence)
            {
                return;
            }

            if (m_FBXBakedExportFPS == 0.0f)
            {
                CFBXExporterDialog fpsDialog(true);

                while (fpsDialog.exec() != QDialog::Accepted)
                {
                    QMessageBox::warning(QApplication::activeWindow(), QString(), QObject::tr("You need to select fps for baked camera shake tracks. Select and click OK"));
                }

                m_FBXBakedExportFPS = fpsDialog.GetFPS();
                m_numberOfExportFrames = pSequence->GetTimeRange().end * m_FBXBakedExportFPS;
            }

            float fpsTimeInterval = 1.0f / m_FBXBakedExportFPS;

            CTrackViewAnimNode* pCameraNode = nullptr;
            if (qobject_cast<const CEntityObject*>(pBaseObj))
            {
                pCameraNode = GetIEditor()->GetSequenceManager()->GetActiveAnimNode(static_cast<const CEntityObject*>(pBaseObj));
            }

            float timeValue = 0.0f;

            int sizeinfo = pObj->m_entityAnimData.size();

            pObj->m_entityAnimData.reserve(m_numberOfExportFrames * kReserveCount);

            for (int frameID = 0; frameID < m_numberOfExportFrames; ++frameID)
            {
                Quat rotation(Ang3(0.0f, 0.0f, 0.0f));
                static_cast<CTrackViewCameraNode*>(pCameraNode)->GetShakeRotation(timeValue, rotation);
                Ang3 worldAngles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(rotation)));

                Export::EntityAnimData entityData;
                entityData.keyTime = timeValue;
                entityData.leftTangentWeight = 0.0f;
                entityData.rightTangentWeight = 0.0f;
                entityData.leftTangent = 0.0f;
                entityData.rightTangent = 0.0f;

                entityData.keyValue = worldAngles.x;
                entityData.dataType = (Export::AnimParamType)AnimParamType::RotationX;
                pObj->m_entityAnimData.push_back(entityData);

                entityData.keyValue = worldAngles.y;
                entityData.dataType = (Export::AnimParamType)AnimParamType::RotationY;
                pObj->m_entityAnimData.push_back(entityData);

                entityData.dataType = (Export::AnimParamType)AnimParamType::RotationZ;
                entityData.keyValue = worldAngles.z;
                pObj->m_entityAnimData.push_back(entityData);

                timeValue += fpsTimeInterval;
            }

            return;
        }
    }

    for (int trackNumber = 0; trackNumber < pEntityTrack->GetChildCount(); ++trackNumber)
    {
        CTrackViewTrack* pSubTrack = static_cast<CTrackViewTrack*>(pEntityTrack->GetChild(trackNumber));

        if (pSubTrack)
        {
            AddEntityAnimationData(pSubTrack, pObj, entityTrackParamType);
        }
    }
}


void CExportManager::AddEntityAnimationData(CBaseObject* pBaseObj)
{
    Export::CObject* pObj = new Export::CObject(m_pBaseObj->GetName().toUtf8().data());

    if (qobject_cast<CCameraObject*>(pBaseObj))
    {
        pObj->entityType = Export::eCamera;

        CBaseObjectPtr pLookAt = pBaseObj->GetLookAt();

        if (pLookAt)
        {
            if (qobject_cast<CCameraObjectTarget*>(pLookAt))
            {
                cry_strcpy(pObj->cameraTargetNodeName, pLookAt->GetName().toUtf8().data());
            }
        }

        ProcessEntityAnimationTrack(pBaseObj, pObj, AnimParamType::FOV);
    }
    else if (qobject_cast<CCameraObjectTarget*>(pBaseObj))
    {
        pObj->entityType = Export::eCameraTarget;
    }

    ProcessEntityAnimationTrack(pBaseObj, pObj, AnimParamType::Position);
    ProcessEntityAnimationTrack(pBaseObj, pObj, AnimParamType::Rotation);

    m_data.m_objects.push_back(pObj);
    m_objectMap[pBaseObj] = int(m_data.m_objects.size() - 1);
}


void CExportManager::AddMesh(Export::CObject* pObj, const IIndexedMesh* pIndMesh, Matrix34A* pTm)
{
    if (m_isPrecaching || !pObj)
    {
        return;
    }

    pObj->m_MeshHash    =   reinterpret_cast<size_t>(pIndMesh);
    IIndexedMesh::SMeshDescription meshDesc;
    pIndMesh->GetMeshDescription(meshDesc);

    // if we have subset of meshes we need to duplicate vertices,
    // keep transformation of submesh,
    // and store new offset for indices
    int newOffsetIndex = pObj->GetVertexCount();

    if (meshDesc.m_nVertCount)
    {
        pObj->m_vertices.reserve(meshDesc.m_nVertCount + newOffsetIndex);
        pObj->m_normals.reserve(meshDesc.m_nVertCount + newOffsetIndex);
    }

    for (int v = 0; v < meshDesc.m_nVertCount; ++v)
    {
        Vec3 n = meshDesc.m_pNorms[v].GetN();
        Vec3 tmp = (meshDesc.m_pVerts ? meshDesc.m_pVerts[v] : meshDesc.m_pVertsF16[v].ToVec3());
        if (pTm)
        {
            tmp = pTm->TransformPoint(tmp);
        }

        pObj->m_vertices.push_back(Vec3ToVector3D(tmp * m_fScale));
        pObj->m_normals.push_back(Vec3ToVector3D(n));
    }

    if (meshDesc.m_nCoorCount)
    {
        pObj->m_texCoords.reserve(meshDesc.m_nCoorCount + newOffsetIndex);
    }

    for (int v = 0; v < meshDesc.m_nCoorCount; ++v)
    {
        Export::UV tc;
        meshDesc.m_pTexCoord[v].ExportTo(tc.u, tc.v);
        tc.v = 1.0f - tc.v;
        pObj->m_texCoords.push_back(tc);
    }

    CMaterial* pMtl = 0;

    if (m_pBaseObj)
    {
        pMtl = m_pBaseObj->GetRenderMaterial();
    }

    if (pMtl)
    {
        pObj->SetMaterialName(pMtl->GetFullName().toUtf8().data());
    }

    if (pIndMesh->GetSubSetCount() && !(pIndMesh->GetSubSetCount() == 1 && pIndMesh->GetSubSet(0).nNumIndices == 0))
    {
        for (int i = 0; i < pIndMesh->GetSubSetCount(); ++i)
        {
            Export::CMesh* pMesh = new Export::CMesh();

            const SMeshSubset& sms = pIndMesh->GetSubSet(i);
            const vtx_idx* pIndices = &meshDesc.m_pIndices[sms.nFirstIndexId];
            int nTris = sms.nNumIndices / 3;
            pMesh->m_faces.reserve(nTris);
            for (int f = 0; f < nTris; ++f)
            {
                Export::Face face;
                face.idx[0] = *(pIndices++) + newOffsetIndex;
                face.idx[1] = *(pIndices++) + newOffsetIndex;
                face.idx[2] = *(pIndices++) + newOffsetIndex;
                pMesh->m_faces.push_back(face);
            }

            if (pMtl)
            {
                if (pMtl->IsMultiSubMaterial())
                {
                    CMaterial* pSubMtl = 0;
                    if (sms.nMatID < pMtl->GetSubMaterialCount())
                    {
                        pSubMtl = pMtl->GetSubMaterial(sms.nMatID);
                    }
                    pMesh->SetMaterial(pSubMtl, m_pBaseObj);
                }
                else
                {
                    pMesh->SetMaterial(pMtl, m_pBaseObj);
                }
            }

            pObj->m_meshes.push_back(pMesh);
        }
    }
    else
    {
        Export::CMesh* pMesh = new Export::CMesh();
        if (meshDesc.m_nFaceCount == 0 && meshDesc.m_nIndexCount != 0 && meshDesc.m_pIndices != 0)
        {
            const vtx_idx* pIndices = &meshDesc.m_pIndices[0];
            int nTris = meshDesc.m_nIndexCount / 3;
            pMesh->m_faces.reserve(nTris);
            for (int f = 0; f < nTris; ++f)
            {
                Export::Face face;
                face.idx[0] = *(pIndices++) + newOffsetIndex;
                face.idx[1] = *(pIndices++) + newOffsetIndex;
                face.idx[2] = *(pIndices++) + newOffsetIndex;
                pMesh->m_faces.push_back(face);
            }
        }
        else
        {
            pMesh->m_faces.reserve(meshDesc.m_nFaceCount);
            for (int f = 0; f < meshDesc.m_nFaceCount; ++f)
            {
                Export::Face face;
                face.idx[0] = meshDesc.m_pFaces[f].v[0];
                face.idx[1] = meshDesc.m_pFaces[f].v[1];
                face.idx[2] = meshDesc.m_pFaces[f].v[2];
                pMesh->m_faces.push_back(face);
            }
        }

        if (m_pBaseObj && pMtl)
        {
            pMesh->SetMaterial(pMtl, m_pBaseObj);
        }
        pObj->m_meshes.push_back(pMesh);
    }
}


bool CExportManager::AddStatObj(Export::CObject* pObj, IStatObj* pStatObj, Matrix34A* pTm)
{
    IIndexedMesh* pIndMesh = 0;

    if (pStatObj->GetSubObjectCount())
    {
        for (int i = 0; i < pStatObj->GetSubObjectCount(); i++)
        {
            IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(i);
            if (pSubObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH && pSubObj->pStatObj)
            {
                pIndMesh = 0;
                if (m_isOccluder)
                {
                    if (pSubObj->pStatObj->GetLodObject(2))
                    {
                        pIndMesh = pSubObj->pStatObj->GetLodObject(2)->GetIndexedMesh(true);
                    }
                    if (!pIndMesh && pSubObj->pStatObj->GetLodObject(1))
                    {
                        pIndMesh = pSubObj->pStatObj->GetLodObject(1)->GetIndexedMesh(true);
                    }
                }
                if (!pIndMesh)
                {
                    pIndMesh = pSubObj->pStatObj->GetIndexedMesh(true);
                }
                if (pIndMesh)
                {
                    AddMesh(pObj, pIndMesh, pTm);
                }
            }
        }
    }

    if (!pIndMesh)
    {
        if (m_isOccluder)
        {
            if (pStatObj->GetLodObject(2))
            {
                pIndMesh = pStatObj->GetLodObject(2)->GetIndexedMesh(true);
            }
            if (!pIndMesh && pStatObj->GetLodObject(1))
            {
                pIndMesh = pStatObj->GetLodObject(1)->GetIndexedMesh(true);
            }
        }
        if (!pIndMesh)
        {
            pIndMesh = pStatObj->GetIndexedMesh(true);
        }
        if (pIndMesh)
        {
            AddMesh(pObj, pIndMesh, pTm);
        }
    }

    return true;
}


bool CExportManager::AddCharacter(Export::CObject* pObj, ICharacterInstance* pCharacter, Matrix34A* pTm)
{
    if (!pCharacter || m_isOccluder)
    {
        return false;
    }

    IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
    ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
    if (pSkeletonPose)
    {
        uint32 nJointCount = rIDefaultSkeleton.GetJointCount();
        for (uint32 i = 0; i < nJointCount; ++i)
        {
            IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(i);
            if (pStatObj)
            {
                const QuatT& quat = pSkeletonPose->GetAbsJointByID(i);
                Matrix34A tm(quat);
                AddStatObj(pObj, pStatObj, &tm);
            }
        }
    }

    IAttachmentManager* AttachmentManager = pCharacter->GetIAttachmentManager();
    if (AttachmentManager)
    {
        uint32 nAttachmentCount = AttachmentManager->GetAttachmentCount();
        for (uint32 i = 0; i < nAttachmentCount; ++i)
        {
            IAttachment* pAttachment = AttachmentManager->GetInterfaceByIndex(i);
            if (pAttachment)
            {
                IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
                if (pAttachmentObject)
                {
                    if (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_StatObj)
                    {
                        IStatObj* pStatObj = pAttachmentObject->GetIStatObj();
                        if (pStatObj)
                        {
                            QuatT quat = pAttachment->GetAttModelRelative();
                            Matrix34A tm(quat);
                            AddStatObj(pObj, pStatObj, &tm);
                        }
                    }
                    else if (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
                    {
                    }
                    else if (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Skeleton)
                    {
                        ICharacterInstance* pCharacter = pAttachmentObject->GetICharacterInstance();
                        if (pCharacter)
                        {
                            AddCharacter(pObj, pCharacter);
                        }
                    }
                }
            }
        }
    }

    // TODO: Add functionality after a code will be fixed on Engine side
    /*
    IRenderMesh* pRenderMesh = pCharacter->GetRenderMesh();
    if (pRenderMesh)
    {
        // should return indexed mesh if render mesh exists
        IIndexedMesh* pIndMesh = pRenderMesh->GetIndexedMesh();
    }
    */
    return true;
}


bool CExportManager::AddMeshes(Export::CObject* pObj)
{
    CEdGeometry* pEdGeom = m_pBaseObj->GetGeometry();
    IIndexedMesh* pIndMesh = 0;

    if (m_pBaseObj->GetType() == OBJTYPE_SOLID)
    {
        IStatObj* pStatObj = m_pBaseObj->GetIStatObj();
        if (pStatObj)
        {
            pIndMesh = pStatObj->GetIndexedMesh();
        }
    }

    if (pIndMesh)
    {
        AddMesh(pObj, pIndMesh);
        return true;
    }

    if (pEdGeom)
    {
        size_t idx = 0;
        size_t nextIdx = 0;
        do
        {
            pIndMesh = 0;
            if (m_isOccluder)
            {
                if (pEdGeom->GetIStatObj() && pEdGeom->GetIStatObj()->GetLodObject(2))
                {
                    pIndMesh    =   pEdGeom->GetIStatObj()->GetLodObject(2)->GetIndexedMesh(true);
                }
                if (!pIndMesh && pEdGeom->GetIStatObj() && pEdGeom->GetIStatObj()->GetLodObject(1))
                {
                    pIndMesh    =   pEdGeom->GetIStatObj()->GetLodObject(1)->GetIndexedMesh(true);
                }
            }

            if (!pIndMesh)
            {
                pIndMesh = pEdGeom->GetIndexedMesh(idx);
                nextIdx++;
            }

            if (!pIndMesh)
            {
                break;
            }

            Matrix34 tm;
            pEdGeom->GetTM(&tm, idx);
            Matrix34A objTM = tm;
            AddMesh(pObj, pIndMesh, &objTM);
            idx = nextIdx;
        }
        while (pIndMesh && idx);

        if (idx > 0)
        {
            return true;
        }
    }

    if ((m_pBaseObj->GetType() == OBJTYPE_ENTITY) || (m_pBaseObj->GetType() == OBJTYPE_AZENTITY))
    {
        CEntityObject* pEntityObject = (CEntityObject*)m_pBaseObj;
        IRenderNode* pEngineNode = pEntityObject->GetEngineNode();
        IEntity* pEntity = pEntityObject->GetIEntity();

        if (pEngineNode)
        {
            if (m_isPrecaching)
            {
                GetIEditor()->Get3DEngine()->PrecacheRenderNode(pEngineNode, 0);
            }
            else
            {
                for (int i = 0; i < pEngineNode->GetSlotCount(); ++i)
                {
                    Matrix34A tm;
                    IStatObj* pStatObj = pEngineNode->GetEntityStatObj(i, 0, &tm);
                    if (pStatObj)
                    {
                        Matrix34A objTM = m_pBaseObj->GetWorldTM();
                        objTM.Invert();
                        tm = objTM * tm;
                        AddStatObj(pObj, pStatObj, &tm);
                    }

                    if (pEntity)
                    {
                        ICharacterInstance* pCharacter = pEntity->GetCharacter(i);
                        if (pCharacter)
                        {
                            AddCharacter(pObj, pCharacter);
                        }
                    }
                }
            }
        }
    }

    return true;
}


bool CExportManager::AddObject(CBaseObject* pBaseObj)
{
    if (pBaseObj->GetType() == OBJTYPE_GROUP)
    {
        for (int i = 0; i < pBaseObj->GetChildCount(); i++)
        {
            AddObject(pBaseObj->GetChild(i));
        }
    }

    if (m_isOccluder)
    {
        const ObjectType OT =   pBaseObj->GetType();
        IRenderNode* pRenderNode = pBaseObj->GetEngineNode();
        if ((OT == OBJTYPE_BRUSH || OT == OBJTYPE_SOLID) && pRenderNode)
        {
            if ((pRenderNode->GetRndFlags() & ERF_GOOD_OCCLUDER) == 0)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    m_pBaseObj = pBaseObj;

    if (m_bAnimationExport && qobject_cast<CEntityObject*>(pBaseObj))
    {
        AddEntityAnimationData(pBaseObj);
        return true;
    }


    if (m_isPrecaching)
    {
        AddMeshes(0);
        return true;
    }

    Export::CObject* pObj = new Export::CObject(m_pBaseObj->GetName().toUtf8().data());

    AddPosRotScale(pObj, pBaseObj);
    m_data.m_objects.push_back(pObj);

    m_objectMap[pBaseObj] = int(m_data.m_objects.size() - 1);

    AddMeshes(pObj);
    m_pBaseObj = 0;

    return true;
}


void CExportManager::AddPosRotScale(Export::CObject* pObj, const CBaseObject* pBaseObj)
{
    Vec3 pos = pBaseObj->GetPos();
    pObj->pos.x = pos.x * m_fScale;
    pObj->pos.y = pos.y * m_fScale;
    pObj->pos.z = pos.z * m_fScale;

    Quat rot = pBaseObj->GetRotation();
    pObj->rot.v.x = rot.v.x;
    pObj->rot.v.y = rot.v.y;
    pObj->rot.v.z = rot.v.z;
    pObj->rot.w = rot.w;

    Vec3 scale = pBaseObj->GetScale();
    pObj->scale.x = scale.x;
    pObj->scale.y = scale.y;
    pObj->scale.z = scale.z;

    if (qobject_cast<const CCameraObjectTarget*>(pBaseObj))
    {
        AddEntityData(pObj, Export::AnimParamType::PositionX, pBaseObj->GetPos().x, 0.0f);
        AddEntityData(pObj, Export::AnimParamType::PositionY, pBaseObj->GetPos().y, 0.0f);
        AddEntityData(pObj, Export::AnimParamType::PositionZ, pBaseObj->GetPos().z, 0.0f);
        AddEntityData(pObj, Export::AnimParamType::RotationX, pBaseObj->GetRotation().v.x, 0.0f);
        AddEntityData(pObj, Export::AnimParamType::RotationY, pBaseObj->GetRotation().v.y, 0.0f);
        AddEntityData(pObj, Export::AnimParamType::RotationZ, pBaseObj->GetRotation().v.z, 0.0f);
    }
}

void CExportManager::AddEntityData(Export::CObject* pObj, Export::AnimParamType dataType, const float fValue, const float fTime)
{
    Export::EntityAnimData entityData;
    entityData.dataType = dataType;
    entityData.leftTangent = kTangentDelta;
    entityData.rightTangent = kTangentDelta;
    entityData.rightTangentWeight = 0.0f;
    entityData.leftTangentWeight = 0.0f;
    entityData.keyValue = fValue;
    entityData.keyTime = fTime;
    pObj->m_entityAnimData.push_back(entityData);
}


void CExportManager::SolveHierarchy()
{
    for (TObjectMap::iterator it = m_objectMap.begin(); it != m_objectMap.end(); ++it)
    {
        CBaseObject* pObj = it->first;
        int index = it->second;
        if (pObj && pObj->GetParent())
        {
            CBaseObject* pParent = pObj->GetParent();
            TObjectMap::iterator itFind = m_objectMap.find(pParent);
            if (itFind != m_objectMap.end())
            {
                int indexOfParent = itFind->second;
                if (indexOfParent >= 0 && index >= 0)
                {
                    m_data.m_objects[index]->nParent = indexOfParent;
                }
            }
        }
    }

    m_objectMap.clear();
}

bool CExportManager::ShowFBXExportDialog()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    if (!pSequence)
    {
        return false;
    }

    CFBXExporterDialog fpsDialog;

    CTrackViewNode* pivotObjectNode = pSequence->GetFirstSelectedNode();

    if (pivotObjectNode && !pivotObjectNode->IsGroupNode())
    {
        m_pivotEntityObject = static_cast<CEntityObject*>(GetIEditor()->GetObjectManager()->FindObject(pivotObjectNode->GetName()));

        if (m_pivotEntityObject)
        {
            fpsDialog.SetExportLocalCoordsCheckBoxEnable(true);
        }
    }

    if (fpsDialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    SetFBXExportSettings(fpsDialog.GetExportCoordsLocalToTheSelectedObject(), fpsDialog.GetExportOnlyMasterCamera(), fpsDialog.GetFPS());

    return true;
}

bool CExportManager::ProcessObjectsForExport()
{
    Export::CObject* pObj = new Export::CObject(kMasterCameraName.toUtf8().data());
    pObj->entityType = Export::eCamera;
    m_data.m_objects.push_back(pObj);

    float fpsTimeInterval = 1.0f / m_FBXBakedExportFPS;
    float timeValue = 0.0f;

    IObjectManager* pObjMgr = GetIEditor()->GetObjectManager();

    GetIEditor()->GetAnimation()->SetRecording(false);
    GetIEditor()->GetAnimation()->SetPlaying(false);

    CViewport* vp = GetIEditor()->GetViewManager()->GetSelectedViewport();
    if (CRenderViewport* rvp = viewport_cast<CRenderViewport*>(vp))
    {
        rvp->SetSequenceCamera();
    }

    int startFrame = 0;
    timeValue = startFrame * fpsTimeInterval;

    for (int frameID = startFrame; frameID <= m_numberOfExportFrames; ++frameID)
    {
        GetIEditor()->GetAnimation()->SetTime(timeValue);

        for (size_t objectID = 0; objectID < m_data.m_objects.size(); ++objectID)
        {
            Export::CObject* pObj =  m_data.m_objects[objectID];
            CBaseObject* pObject = 0;

            if (QString::compare(pObj->name, kMasterCameraName) == 0)
            {
                REFGUID camGUID = GetIEditor()->GetViewManager()->GetCameraObjectId();
                pObject = GetIEditor()->GetObjectManager()->FindObject(GetIEditor()->GetViewManager()->GetCameraObjectId());
            }
            else
            {
                if (m_bExportOnlyMasterCamera && pObj->entityType != Export::eCameraTarget)
                {
                    continue;
                }

                pObject = pObj->GetLastObjectPtr();
            }

            if (!pObject)
            {
                continue;
            }

            Quat rotation(pObject->GetRotation());

            if (pObject->GetParent())
            {
                CBaseObject* pParentObject = pObject->GetParent();
                Quat parentWorldRotation;

                const Vec3& parentScale = pParentObject->GetScale();
                float threshold = 0.0003f;

                bool bParentScaled = false;

                if ((fabsf(parentScale.x - 1.0f) + fabsf(parentScale.y - 1.0f) + fabsf(parentScale.z - 1.0f)) >= threshold)
                {
                    bParentScaled = true;
                }

                if (bParentScaled)
                {
                    Matrix34 tm = pParentObject->GetWorldTM();
                    tm.OrthonormalizeFast();
                    parentWorldRotation = Quat(tm);
                }
                else
                {
                    parentWorldRotation = Quat(pParentObject->GetWorldTM());
                }

                rotation = parentWorldRotation * rotation;
            }

            Vec3 objectPos = pObject->GetWorldPos();

            if (m_bExportLocalCoords && m_pivotEntityObject && m_pivotEntityObject != pObject)
            {
                Matrix34 currentObjectTM = pObject->GetWorldTM();
                Matrix34 invParentTM = m_pivotEntityObject->GetWorldTM();
                invParentTM.Invert();
                currentObjectTM = invParentTM * currentObjectTM;

                objectPos = currentObjectTM.GetTranslation();
                rotation = Quat(currentObjectTM);
            }

            Ang3 worldAngles = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(rotation)));

            Export::EntityAnimData entityData;
            entityData.keyTime = timeValue;
            entityData.leftTangentWeight = 0.0f;
            entityData.rightTangentWeight = 0.0f;
            entityData.leftTangent = 0.0f;
            entityData.rightTangent = 0.0f;

            entityData.keyValue = objectPos.x;
            entityData.keyValue *= 100.0f;
            entityData.dataType = (Export::AnimParamType)AnimParamType::PositionX;
            pObj->m_entityAnimData.push_back(entityData);

            entityData.keyValue = objectPos.y;
            entityData.keyValue *= 100.0f;
            entityData.dataType = (Export::AnimParamType)AnimParamType::PositionY;
            pObj->m_entityAnimData.push_back(entityData);

            entityData.keyValue = objectPos.z;
            entityData.keyValue *= 100.0f;
            entityData.dataType = (Export::AnimParamType)AnimParamType::PositionZ;
            pObj->m_entityAnimData.push_back(entityData);

            entityData.keyValue = worldAngles.x;
            entityData.dataType = (Export::AnimParamType)AnimParamType::RotationX;
            pObj->m_entityAnimData.push_back(entityData);

            entityData.keyValue = worldAngles.y;
            entityData.dataType = (Export::AnimParamType)AnimParamType::RotationY;
            pObj->m_entityAnimData.push_back(entityData);

            entityData.dataType = (Export::AnimParamType)AnimParamType::RotationZ;
            entityData.keyValue = worldAngles.z;
            pObj->m_entityAnimData.push_back(entityData);

            if (qobject_cast<CCameraObject*>(pObject) || pObj->entityType == Export::eCamera)
            {
                entityData.dataType = (Export::AnimParamType)AnimParamType::FOV;
                CCameraObject* pCameraObject = (CCameraObject*)pObject;
                IEntity* entity = pCameraObject->GetIEntity();
                IComponentCameraPtr pCameraComponent = entity->GetComponent<IComponentCamera>();

                if (pCameraComponent)
                {
                    CCamera& cam = pCameraComponent->GetCamera();
                    entityData.keyValue = Sandbox2MayaFOVRad2Deg(cam.GetFov(), kAspectRatio);
                    pObj->m_entityAnimData.push_back(entityData);
                }
            }
        }

        timeValue += fpsTimeInterval;
    }

    return true;
}

bool CExportManager::IsDuplicateObjectBeingAdded(const QString& newObjectName)
{
    for (int objectID = 0; objectID < m_data.m_objects.size(); ++objectID)
    {
        if (newObjectName.compare(m_data.m_objects[objectID]->name, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }

    return false;
}

bool CExportManager::AddCameraTargetObject(CBaseObjectPtr pLookAt)
{
    if (qobject_cast<CCameraObjectTarget*>(pLookAt))
    {
        if (IsDuplicateObjectBeingAdded(pLookAt->GetName()))
        {
            return false;
        }

        Export::CObject* pCameraTargetObject = new Export::CObject(pLookAt->GetName().toUtf8().data());
        pCameraTargetObject->entityType = Export::eCameraTarget;
        pCameraTargetObject->m_entityAnimData.reserve(m_numberOfExportFrames * kReserveCount);
        pCameraTargetObject->SetLastPtr(pLookAt);

        m_data.m_objects.push_back(pCameraTargetObject);
        m_objectMap[pLookAt] = int(m_data.m_objects.size() - 1);

        return true;
    }

    return false;
}

QString CExportManager::CleanXMLText(const QString& text)
{
    QString outText(text);
    outText.replace("\\", "_");
    outText.replace("/", "_");
    outText.replace(" ", "_");
    outText.replace(":", "-");
    outText.replace(";", "-");
    return outText;
}

void CExportManager::FillAnimTimeNode(XmlNodeRef writeNode, CTrackViewAnimNode* pObjectNode, CTrackViewSequence* currentSequence)
{
    if (!writeNode)
    {
        return;
    }

    CTrackViewTrackBundle allTracks = pObjectNode->GetAllTracks();
    const unsigned int numAllTracks = allTracks.GetCount();
    bool bCreatedSubNodes = false;

    if (numAllTracks > 0)
    {
        XmlNodeRef objNode = writeNode->createNode(CleanXMLText(pObjectNode->GetName()).toUtf8().data());
        writeNode->setAttr("time", m_animTimeExportMasterSequenceCurrentTime);

        for (unsigned int trackID = 0; trackID < numAllTracks; ++trackID)
        {
            CTrackViewTrack* childTrack = allTracks.GetTrack(trackID);

            AnimParamType trackType = childTrack->GetParameterType().GetType();

            if (trackType == AnimParamType::Animation || trackType == AnimParamType::Sound)
            {
                QString childName = CleanXMLText(childTrack->GetName());

                if (childName.isEmpty())
                {
                    continue;
                }

                XmlNodeRef subNode = objNode->createNode(childName.toUtf8().data());
                CTrackViewKeyBundle keyBundle = childTrack->GetAllKeys();
                uint keysNumber = keyBundle.GetKeyCount();

                for (uint keyID = 0; keyID < keysNumber; ++keyID)
                {
                    const CTrackViewKeyHandle& keyHandle = keyBundle.GetKey(keyID);

                    QString keyContentName;
                    float keyStartTime = 0.0f;
                    float keyEndTime = 0.0f;
                    ;
                    float keyTime = 0.0f;
                    float keyDuration = 0.0f;

                    if (trackType == AnimParamType::Animation)
                    {
                        if (!m_animKeyTimeExport)
                        {
                            continue;
                        }

                        ICharacterKey animationKey;
                        keyHandle.GetKey(&animationKey);
                        keyStartTime = animationKey.m_startTime;
                        keyEndTime = animationKey.m_endTime;
                        keyTime = animationKey.time;
                        keyContentName = CleanXMLText(animationKey.m_animation.c_str());
                        keyDuration = animationKey.GetActualDuration();
                    }
                    else if (trackType == AnimParamType::Sound)
                    {
                        if (!m_soundKeyTimeExport)
                        {
                            continue;
                        }

                        ISoundKey soundKey;
                        keyHandle.GetKey(&soundKey);
                        keyTime = soundKey.time;
                        keyContentName = CleanXMLText(soundKey.sStartTrigger.c_str());
                        keyDuration = soundKey.fDuration;
                    }

                    if (keyContentName.isEmpty())
                    {
                        continue;
                    }

                    XmlNodeRef keyNode = subNode->createNode(keyContentName.toUtf8().data());

                    float keyGlobalTime = m_animTimeExportMasterSequenceCurrentTime + keyTime;
                    keyNode->setAttr("keyTime", keyGlobalTime);

                    if (keyStartTime > 0)
                    {
                        keyNode->setAttr("startTime", keyStartTime);
                    }

                    if (keyEndTime > 0)
                    {
                        keyNode->setAttr("endTime", keyEndTime);
                    }

                    if (keyDuration > 0)
                    {
                        keyNode->setAttr("duration", keyDuration);
                    }

                    subNode->addChild(keyNode);
                    objNode->addChild(subNode);
                    bCreatedSubNodes = true;
                }
            }
        }

        if (bCreatedSubNodes)
        {
            writeNode->addChild(objNode);
        }
    }
}


bool CExportManager::AddObjectsFromSequence(CTrackViewSequence* pSequence, XmlNodeRef seqNode)
{
    CTrackViewAnimNodeBundle allNodes = pSequence->GetAllAnimNodes();
    const unsigned int numAllNodes = allNodes.GetCount();

    for (unsigned int nodeID = 0; nodeID < numAllNodes; ++nodeID)
    {
        CTrackViewAnimNode* pAnimNode = allNodes.GetNode(nodeID);
        CEntityObject* pEntityObject = pAnimNode->GetNodeEntity();

        if (seqNode && pAnimNode)
        {
            FillAnimTimeNode(seqNode, pAnimNode, pSequence);
        }

        if (pEntityObject)
        {
            QString addObjectName = pEntityObject->GetName();

            if (IsDuplicateObjectBeingAdded(addObjectName))
            {
                continue;
            }

            Export::CObject* pObj = new Export::CObject(pEntityObject->GetName().toUtf8().data());

            if (qobject_cast<CCameraObject*>(pEntityObject))
            {
                pObj->entityType = Export::eCamera;

                CBaseObjectPtr pLookAt = pEntityObject->GetLookAt();

                if (pLookAt)
                {
                    if (AddCameraTargetObject(pLookAt))
                    {
                        cry_strcpy(pObj->cameraTargetNodeName, pLookAt->GetName().toUtf8().data());
                    }
                }
            }
            else if (qobject_cast<CCameraObjectTarget*>(pEntityObject))
            {
                // Add camera target through and before its camera object.
                continue;
            }

            pObj->m_entityAnimData.reserve(m_numberOfExportFrames * kReserveCount);
            pObj->SetLastPtr(pEntityObject);
            m_data.m_objects.push_back(pObj);
            m_objectMap[pEntityObject] = int(m_data.m_objects.size() - 1);
        }
    }

    CTrackViewTrackBundle trackBundle = pSequence->GetTracksByParam(AnimParamType::Sequence);

    const uint numSequenceTracks = trackBundle.GetCount();
    for (uint i = 0; i < numSequenceTracks; ++i)
    {
        CTrackViewTrack* pSequenceTrack = trackBundle.GetTrack(i);
        if (pSequenceTrack->IsDisabled())
        {
            continue;
        }

        const uint numKeys = pSequenceTrack->GetKeyCount();
        for (int keyIndex = 0; keyIndex < numKeys; ++keyIndex)
        {
            const CTrackViewKeyHandle& keyHandle = pSequenceTrack->GetKey(keyIndex);
            ISequenceKey sequenceKey;
            keyHandle.GetKey(&sequenceKey);

            CTrackViewSequence* pSubSequence = CDirectorNodeAnimator::GetSequenceFromSequenceKey(sequenceKey);

            if (pSubSequence)
            {
                if (pSubSequence && !pSubSequence->IsDisabled())
                {
                    XmlNodeRef subSeqNode = 0;

                    if (!seqNode)
                    {
                        AddObjectsFromSequence(pSubSequence);
                    }
                    else
                    {
                        // In case of exporting animation/sound times data
                        const QString sequenceName = pSubSequence->GetName();
                        XmlNodeRef subSeqNode = seqNode->createNode(sequenceName.toUtf8().data());

                        if (sequenceName == m_animTimeExportMasterSequenceName)
                        {
                            m_animTimeExportMasterSequenceCurrentTime = sequenceKey.time;
                        }
                        else
                        {
                            m_animTimeExportMasterSequenceCurrentTime += sequenceKey.time;
                        }

                        AddObjectsFromSequence(pSubSequence, subSeqNode);
                        seqNode->addChild(subSeqNode);
                    }
                }
            }
        }
    }

    return false;
}

bool CExportManager::AddSelectedEntityObjects()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
    if (!pSequence)
    {
        return false;
    }

    CTrackViewAnimNodeBundle selectedNodes = pSequence->GetSelectedAnimNodes();
    const unsigned int numSelectedNodes = selectedNodes.GetCount();

    for (unsigned int nodeNumber = 0; nodeNumber < numSelectedNodes; ++nodeNumber)
    {
        CTrackViewAnimNode* pAnimNode = selectedNodes.GetNode(nodeNumber);
        CEntityObject* pEntityObject = pAnimNode->GetNodeEntity();

        if (pEntityObject)
        {
            // Don't add selected cameraTarget nodes directly. Add them through GetLookAt, before you add camera object.
            if (qobject_cast<CCameraObjectTarget*>(pEntityObject))
            {
                if (numSelectedNodes == 1)
                {
                    QMessageBox::warning(QApplication::activeWindow(), QString(), QObject::tr("Please select camera object, to export its camera target."));
                    return false;
                }

                continue;
            }

            CBaseObjectPtr pLookAt = pEntityObject->GetLookAt();

            if (qobject_cast<CCameraObjectTarget*>(pLookAt))
            {
                AddObject(pLookAt);
            }

            AddObject(pEntityObject);
        }
    }

    return true;
}


bool CExportManager::AddSelectedObjects()
{
    CSelectionGroup* pSelection = GetIEditor()->GetSelection();

    int numObjects = pSelection->GetCount();
    if (numObjects > m_data.m_objects.size())
    {
        m_data.m_objects.reserve(numObjects + 1); // +1 for terrain
    }
    // First run pipeline to precache geometry
    m_isPrecaching = true;
    for (int i = 0; i < numObjects; i++)
    {
        AddObject(pSelection->GetObject(i));
    }

    GetIEditor()->Get3DEngine()->ProposeContentPrecache();

    // Repeat pipeline to collect geometry
    m_isPrecaching = false;
    for (int i = 0; i < numObjects; i++)
    {
        AddObject(pSelection->GetObject(i));
    }

    return true;
}


bool CExportManager::IsNotChildOfGroup(CBaseObject* pObj)
{
    if (pObj->GetParent())
    {
        CBaseObject* pParentObj = pObj->GetParent();
        while (pParentObj)
        {
            if (pParentObj->GetType() == OBJTYPE_GROUP)
            {
                return false;
            }
            pParentObj = pParentObj->GetParent();
        }
    }

    return true; // not a child
}


bool CExportManager::AddSelectedRegionObjects()
{
    AABB box;
    GetIEditor()->GetSelectedRegion(box);
    if (box.IsEmpty())
    {
        return false;
    }

    std::vector<CBaseObject*> objects;
    GetIEditor()->GetObjectManager()->FindObjectsInAABB(box, objects);

    int numObjects = objects.size();
    if (numObjects > m_data.m_objects.size())
    {
        m_data.m_objects.reserve(numObjects + 1); // +1 for terrain
    }
    // First run pipeline to precache geometry
    m_isPrecaching = true;
    for (size_t i = 0; i < numObjects; ++i)
    {
        if (IsNotChildOfGroup(objects[i])) // skip childs of groups
        {
            AddObject(objects[i]);
        }
    }

    GetIEditor()->Get3DEngine()->ProposeContentPrecache();

    // Repeat pipeline to collect geometry
    m_isPrecaching = false;
    for (size_t i = 0; i < numObjects; ++i)
    {
        if (IsNotChildOfGroup(objects[i])) // skip childs of groups
        {
            AddObject(objects[i]);
        }
    }

    return true;
}


bool CExportManager::AddTerrain()
{
    AABB box;
    GetIEditor()->GetSelectedRegion(box);
    if (box.IsEmpty())
    {
        return false;
    }

    CHeightmap& heightmap = *GetIEditor()->GetHeightmap();
    t_hmap* pHeightmapData = heightmap.GetData();
    assert(pHeightmapData);
    int nHeightmapWidth = heightmap.GetWidth();
    int nHeightmapHeight = heightmap.GetHeight();
    float fUnitSize = float(heightmap.GetUnitSize());
    float fUnitSizeScaled = fUnitSize * m_fScale;

    QRect rc;
    rc.setLeft(box.min.y / fUnitSize); // x and y swapped in Heightmap
    rc.setTop(box.min.x / fUnitSize);

    // QRect's right bottom is left/top + width/height - 1/1
    rc.setRight((box.max.y / fUnitSize) - 1);
    rc.setBottom((box.max.x / fUnitSize) - 1);

    if (rc.height() <= 0 && rc.width() <= 0)
    {
        return false;
    }

    // Adjust the rectangle to be valid
    if (rc.right() < 0)
    {
        rc.setRight(-1);
    }
    if (rc.left() < 0)
    {
        rc.setLeft(0);
    }
    if (rc.top() < 0)
    {
        rc.setTop(0);
    }
    if (rc.bottom() < 0)
    {
        rc.setBottom(-1);
    }

    if (rc.right() + 1 >= nHeightmapWidth)
    {
        rc.setRight(nHeightmapWidth - 2);
    }
    if (rc.bottom() + 1 >= nHeightmapHeight)
    {
        rc.setBottom(nHeightmapHeight - 2);
    }

    int nExportWidth = rc.width();
    int nExportHeight = rc.height();

    if (rc.isEmpty())
    {
        return false;
    }

    Export::CObject* pObj = new Export::CObject("Sandbox_Terrain");
    m_data.m_objects.push_back(pObj);

    Export::CMesh* pMesh = new Export::CMesh();

    pObj->m_vertices.reserve((nExportWidth + 1) * (nExportHeight + 1));
    pObj->m_texCoords.reserve((nExportWidth + 1) * (nExportHeight + 1));

    for (int y = rc.top(); y <= rc.bottom() + 1; ++y)
    {
        for (int x = rc.left(); x <= rc.right() + 1; ++x)
        {
            Export::Vector3D vec;
            vec.y = float(x) * fUnitSizeScaled; // x and y swapped in Heightmap
            vec.x = float(y) * fUnitSizeScaled;
            vec.z = pHeightmapData[x + y * nHeightmapWidth] * m_fScale;
            pObj->m_vertices.push_back(vec);

            Export::UV tc;
            tc.u = float(x) / nExportWidth;
            tc.v = 1.0f - float(y) / nExportHeight;
            pObj->m_texCoords.push_back(tc);
        }
    }

    CWaitProgress progress("Export Terrain");

    pMesh->m_faces.reserve(nExportWidth * nExportWidth * 2); // quad has 2 triangles
    for (int y = 0; y < nExportHeight; ++y)
    {
        for (int x = 0; x < nExportWidth; ++x)
        {
            int nExtendedWidth = nExportWidth + 1;
            Export::Face face;
            face.idx[0] = x   + (y) * nExtendedWidth;
            face.idx[1] = x   + (y + 1) * nExtendedWidth;
            face.idx[2] = x + 1 + (y) * nExtendedWidth;
            pMesh->m_faces.push_back(face);

            face.idx[0] = x + 1 + (y) * nExtendedWidth;
            face.idx[1] = x   + (y + 1) * nExtendedWidth;
            face.idx[2] = x + 1 + (y + 1) * nExtendedWidth;
            pMesh->m_faces.push_back(face);
        }

        if (nExportHeight >= 10 && y % (nExportHeight / 10) == 0 && !progress.Step(100 * y / nExportHeight))
        {
            QMessageBox::critical(QApplication::activeWindow(), QString(), QObject::tr("Export Terrain was skipped by user."));
            return false;
        }
    }

    // Adjust material
    Export::Material& material = pMesh->material;
    material.opacity = 1.0f;
    material.diffuse.r = 1.0f;
    material.diffuse.g = 1.0f;
    material.diffuse.b = 1.0f;
    azstrcpy(material.name, AZ_ARRAY_SIZE(material.name), "Sandbox_Terrain");

    pObj->m_meshes.push_back(pMesh);


    /*
    // Not sure do we need this info, just keep code from old version
    // Export Info
    {
        char szInfoPath[_MAX_PATH];
        cry_strcpy(szInfoPath, pszFileName);
        PathRemoveExtension(szInfoPath);
        cry_strcat(szInfoPath,".inf");
        if (CFileUtil::OverwriteFile( szInfoPath ))
        {
            FILE *infoFile = fopen( szInfoPath,"wt" );
            if (infoFile)
            {
                fprintf( infoFile,"x=%d,y=%d,width=%d,height=%d",rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top );
                fclose( infoFile );
            }
        }
    }
    */

    return true;
}


bool CExportManager::AddVegetation()
{
    if (m_isOccluder)
    {
        return false;
    }
    AABB box;
    GetIEditor()->GetSelectedRegion(box);
    if (box.IsEmpty())
    {
        return false;
    }

    CVegetationMap* pVegetationMap = GetIEditor()->GetVegetationMap();
    if (!pVegetationMap)
    {
        return false;
    }

    std::vector<CVegetationInstance*> vegetationInstances;
    pVegetationMap->GetObjectInstances(box.min.x, box.min.y, box.max.x, box.max.y, vegetationInstances);


    // Precaching render nodes
    for (std::vector<CVegetationInstance*>::const_iterator ppVegetationInstance = vegetationInstances.begin();
         ppVegetationInstance != vegetationInstances.end(); ++ppVegetationInstance)
    {
        const CVegetationInstance* pVegetationInstance = *ppVegetationInstance;
        IRenderNode* pRenderNode = pVegetationInstance->pRenderNode;
        if (!pRenderNode)
        {
            continue;
        }
        GetIEditor()->Get3DEngine()->PrecacheRenderNode(pRenderNode, 0);
    }

    GetIEditor()->Get3DEngine()->ProposeContentPrecache();

    int iName = 0;
    for (std::vector<CVegetationInstance*>::const_iterator ppVegetationInstance = vegetationInstances.begin();
         ppVegetationInstance != vegetationInstances.end(); ++ppVegetationInstance)
    {
        const CVegetationInstance* pVegetationInstance = *ppVegetationInstance;
        IRenderNode* pRenderNode = pVegetationInstance->pRenderNode;
        if (!pRenderNode)
        {
            continue;
        }

        CVegetationObject* pVegetationObject = pVegetationInstance->object;
        IStatObj*   pStatObj = pVegetationObject->GetObject();

        QString name;
        name = QStringLiteral("VegetationInstance_%1").arg(iName++);

        Export::CObject* pObj = new Export::CObject(name.toUtf8().data());

        Vec3 pos = pVegetationInstance->pos;
        pObj->pos.x = pos.x * m_fScale;
        pObj->pos.y = pos.y * m_fScale;
        pObj->pos.z = pos.z * m_fScale;

        Quat rot = Quat::CreateIdentity();
        Matrix34A mat;
        if (pRenderNode->GetEntityStatObj(0, 0, &mat) && mat.IsOrthonormal())
        {
            rot = Quat(mat);
        }
        else
        {
            float z = pVegetationInstance->angle * g_PI2 / 256.f;
            rot.SetRotationZ(z);
        }
        pObj->rot.v.x = rot.v.x;
        pObj->rot.v.y = rot.v.y;
        pObj->rot.v.z = rot.v.z;
        pObj->rot.w = rot.w;

        Vec3 scale(pVegetationInstance->scale);
        pObj->scale.x = scale.x;
        pObj->scale.y = scale.y;
        pObj->scale.z = scale.z;

        m_data.m_objects.push_back(pObj);

        if (pStatObj)
        {
            AddStatObj(pObj, pStatObj);
        }

        // pVegetationObject->GetMaterial()  // keep for future
    }

    return true;
}


bool CExportManager::AddOcclusionObjects()
{
    std::vector<CBaseObject*> objects, objectsTmp;
    GetIEditor()->GetObjectManager()->FindObjectsOfType(&CBrushObject::staticMetaObject, objects);
    GetIEditor()->GetObjectManager()->FindObjectsOfType(OBJTYPE_SOLID, objectsTmp);
    objects.insert(objects.end(), objectsTmp.begin(), objectsTmp.end());



    int numObjects = objects.size();
    if (numObjects > m_data.m_objects.size())
    {
        m_data.m_objects.reserve(numObjects); // +1 for terrain
    }
    // First run pipeline to precache geometry
    m_isPrecaching = true;
    for (size_t i = 0; i < numObjects; ++i)
    {
        if (IsNotChildOfGroup(objects[i])) // skip childs of groups
        {
            AddObject(objects[i]);
        }
    }

    GetIEditor()->Get3DEngine()->ProposeContentPrecache();

    // Repeat pipeline to collect geometry
    m_isPrecaching = false;
    for (size_t i = 0; i < numObjects; ++i)
    {
        if (IsNotChildOfGroup(objects[i])) // skip childs of groups
        {
            AddObject(objects[i]);
        }
    }

    return true;
}


bool CExportManager::ExportToFile(const char* filename, bool bClearDataAfterExport)
{
    bool bRet = false;
    QString ext = PathUtil::GetExt(filename);

    if (m_data.GetObjectCount() == 0)
    {
        QMessageBox::warning(QApplication::activeWindow(), QString(), QObject::tr("Track View selection does not exist as an object."));
        return false;
    }

    for (int i = 0; i < m_exporters.size(); ++i)
    {
        IExporter* pExporter = m_exporters[i];
        if (!QString::compare(ext, pExporter->GetExtension(), Qt::CaseInsensitive))
        {
            bRet = pExporter->ExportToFile(filename, &m_data);
            break;
        }
    }

    if (bClearDataAfterExport)
    {
        m_data.Clear();
    }
    return bRet;
}


bool CExportManager::Export(const char* defaultName, const char* defaultExt, const char* defaultPath, bool isSelectedObjects, bool isSelectedRegionObjects, bool isTerrain, bool isOccluder, bool bAnimationExport)
{
    m_bAnimationExport = bAnimationExport;

    m_isOccluder    =   isOccluder;
    const float OldScale    =   m_fScale;
    if (isOccluder)
    {
        m_fScale    =   1.f;
    }

    m_data.Clear();
    m_objectMap.clear();

    QString filters;
    for (TExporters::iterator ppExporter = m_exporters.begin(); ppExporter != m_exporters.end(); ++ppExporter)
    {
        IExporter* pExporter = (*ppExporter);
        if (pExporter)
        {
            const QString ext = pExporter->GetExtension();
            const QString newFilter = QString("%1 (*.%2)").arg(pExporter->GetShortDescription(), ext);
            if (filters.isEmpty())
            {
                filters = newFilter;
            }
            else if (ext == defaultExt) // the default extension should be the first item in the list, so it's the default export options
            {
                filters = newFilter + ";;" + filters;
            }
            else
            {
                filters = filters + ";;" + newFilter;
            }
        }
    }
    filters += ";;All files (*)";

    bool returnRes = false;

    QString newFilename(defaultName);
    if (m_bAnimationExport || CFileUtil::SelectSaveFile(filters, defaultExt, defaultPath, newFilename))
    {
        WaitCursor wait;
        if (isSelectedObjects)
        {
            AddSelectedObjects();
        }
        if (isSelectedRegionObjects)
        {
            AddSelectedRegionObjects();
        }

        if (!bAnimationExport)
        {
            SolveHierarchy();
        }

        if (isSelectedRegionObjects)
        {
            AddVegetation();
        }
        if (isTerrain)
        {
            AddTerrain();
        }

        if (isOccluder)
        {
            AddOcclusionObjects();
        }

        if (m_bAnimationExport)
        {
            CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

            if (pSequence)
            {
                // Save To FBX custom selected nodes
                if (!m_bBakedKeysSequenceExport)
                {
                    returnRes = AddSelectedEntityObjects();
                }
                else
                {
                    // Export the whole sequence with baked keys
                    if (ShowFBXExportDialog())
                    {
                        m_numberOfExportFrames = pSequence->GetTimeRange().end * m_FBXBakedExportFPS;

                        if (!m_bExportOnlyMasterCamera)
                        {
                            AddObjectsFromSequence(pSequence);
                        }

                        returnRes = ProcessObjectsForExport();
                        SolveHierarchy();
                    }
                }
            }

            if (returnRes)
            {
                returnRes = ExportToFile(defaultName);
            }
        }
        else
        {
            returnRes = ExportToFile(newFilename.toStdString().c_str());
        }
    }

    m_fScale = OldScale;
    m_bBakedKeysSequenceExport = true;
    m_FBXBakedExportFPS = 0.0f;

    return returnRes;
}

void CExportManager::SetFBXExportSettings(bool bLocalCoordsToSelectedObject, bool bExportOnlyMasterCamera, const float fps)
{
    m_bExportLocalCoords = bLocalCoordsToSelectedObject;
    m_bExportOnlyMasterCamera = bExportOnlyMasterCamera;
    m_FBXBakedExportFPS = fps;
}

Export::Object* Export::CData::AddObject(const char* objectName)
{
    for (size_t objectID = 0; objectID < m_objects.size(); ++objectID)
    {
        if (strcmp(m_objects[objectID]->name, objectName) == 0)
        {
            return m_objects[objectID];
        }
    }

    CObject* pObj = new CObject(objectName);
    m_objects.push_back(pObj);
    return m_objects[m_objects.size() - 1];
}

bool CExportManager::ImportFromFile(const char* filename)
{
    bool bRet = false;
    QString ext = PathUtil::GetExt(filename);

    m_data.Clear();

    for (size_t handlerID = 0; handlerID < m_exporters.size(); ++handlerID)
    {
        IExporter* pExporter = m_exporters[handlerID];
        if (!QString::compare(ext, pExporter->GetExtension(), Qt::CaseInsensitive))
        {
            bRet = pExporter->ImportFromFile(filename, &m_data);
            break;
        }
    }

    return bRet;
}

bool CExportManager::ExportSingleStatObj(IStatObj* pStatObj, const char* filename)
{
    Export::CObject* pObj = new Export::CObject(Path::GetFileName(filename).toUtf8().data());

    // the exporter uses m_pBaseObj to find the material and submesh settings
    // so we create a temporary brush editor object and set the material we want to use 
    CBaseObjectPtr editorObject = GetIEditor()->NewObject("Brush", "", "TempExportObject", 0.f, 0.f, 0.f, false);
    if (editorObject)
    {
        auto material = pStatObj->GetMaterial();
        CMaterial* editorMaterial = GetIEditor()->GetMaterialManager()->LoadMaterial(material->GetName());
        editorObject->SetMaterial(editorMaterial);
        m_pBaseObj = editorObject;
    }
    AddStatObj(pObj, pStatObj);
    m_data.m_objects.push_back(pObj);
    ExportToFile(filename, true);

    if (editorObject)
    {
        m_pBaseObj = nullptr;
        GetIEditor()->DeleteObject(editorObject);
    }

    return true;
}

void CExportManager::SaveNodeKeysTimeToXML()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
    if (!pSequence)
    {
        return;
    }

    CTrackViewExportKeyTimeDlg exportDialog;

    if (exportDialog.exec() == QDialog::Accepted)
    {
        m_animKeyTimeExport = exportDialog.IsAnimationExportChecked();
        m_soundKeyTimeExport = exportDialog.IsSoundExportChecked();

        QString filters = "All files (*.xml)";
        QString defaultName = QString(pSequence->GetName()) + ".xml";

        QtUtil::QtMFCScopedHWNDCapture cap;
        CAutoDirectoryRestoreFileDialog dlg(QFileDialog::AcceptSave, QFileDialog::AnyFile, "xml", defaultName, filters, {}, {}, cap);
        if (dlg.exec())
        {
            m_animTimeNode = XmlHelpers::CreateXmlNode(pSequence->GetName());
            m_animTimeExportMasterSequenceName = pSequence->GetName();

            m_data.Clear();
            m_animTimeExportMasterSequenceCurrentTime = 0.0;

            AddObjectsFromSequence(pSequence, m_animTimeNode);

            m_animTimeNode->saveToFile(dlg.selectedFiles().constFirst().toStdString().c_str());
            QMessageBox::information(QApplication::activeWindow(), QString(), QObject::tr("Export Finished"));
        }
    }
}
