#include "MapStaticMesh.h"
#include "GameInstance.h"
#include "Collision_Define.h"
#include "MapStaticCollisionBuilder.h"

#include "Camera.h"
#include "StaticCollisionData.h"

namespace
{
    constexpr float kCollisionDebugMaxDistance = 35.f;

    bool g_showCollisionDebug = false;
    const MapStaticMesh* g_toggleOwner = nullptr;

    Vec3 MultiplyAbsComponents(const Vec3& value, const Vec3& scale)
    {
        return Vec3(
            fabsf(value.x) * fabsf(scale.x),
            fabsf(value.y) * fabsf(scale.y),
            fabsf(value.z) * fabsf(scale.z));
    }

    void DecomposeMatrix(const Matrix& matrix, Vec3& outScale, Quat& outRotation, Vec3& outTranslation)
    {
        Matrix temp = matrix;
        temp.Decompose(outScale, outRotation, outTranslation);
    }

    void DrawBoundsFromWorldPoints(const vector<Vec3>& worldPoints, const Color& color)
    {
        if (worldPoints.empty())
            return;

        Vec3 minPoint(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());
        Vec3 maxPoint(
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max());

        for (const Vec3& point : worldPoints)
        {
            minPoint.x = (std::min)(minPoint.x, point.x);
            minPoint.y = (std::min)(minPoint.y, point.y);
            minPoint.z = (std::min)(minPoint.z, point.z);
            maxPoint.x = (std::max)(maxPoint.x, point.x);
            maxPoint.y = (std::max)(maxPoint.y, point.y);
            maxPoint.z = (std::max)(maxPoint.z, point.z);
        }

        DebugBoxDesc desc{};
        desc.center = (minPoint + maxPoint) * 0.5f;
        desc.extents = (maxPoint - minPoint) * 0.5f;
        desc.style.color = color;
        desc.style.depthEnabled = false;
        GAME->Draw_DebugBox(desc);
    }

    void DrawStaticCollisionDebug(const Shared<StaticCollisionData>& collisionData, const Matrix& worldMatrix)
    {
        if (collisionData == nullptr)
            return;

        Vec3 preScale = Vec3::One;
        Quat preRotation = Quat::Identity;
        Vec3 preTranslation = Vec3::Zero;
        DecomposeMatrix(collisionData->preLocalTransform, preScale, preRotation, preTranslation);

        Vec3 worldScale = Vec3::One;
        Quat worldRotation = Quat::Identity;
        Vec3 worldTranslation = Vec3::Zero;
        DecomposeMatrix(worldMatrix, worldScale, worldRotation, worldTranslation);

        const Matrix combinedMatrix = collisionData->preLocalTransform * worldMatrix;

        for (const ModelCollisionProxy& proxy : collisionData->primitiveProxies)
        {
            switch (proxy.type)
            {
            case ModelCollisionProxyType::Box:
            {
                DebugBoxDesc desc{};
                desc.center = Vec3::Transform(proxy.center, combinedMatrix);
                desc.extents = MultiplyAbsComponents(
                    MultiplyAbsComponents(proxy.extents, preScale),
                    worldScale);
                desc.rotation = worldRotation * (preRotation * proxy.rotation);
                desc.style.color = Color(0.f, 1.f, 1.f, 1.f);
                desc.style.depthEnabled = false;
                GAME->Draw_DebugBox(desc);
                break;
            }

            case ModelCollisionProxyType::Sphere:
            {
                const Vec3 combinedScale = MultiplyAbsComponents(preScale, worldScale);

                DebugSphereDesc desc{};
                desc.center = Vec3::Transform(proxy.center, combinedMatrix);
                desc.radius = proxy.radius * (std::max)(combinedScale.x, (std::max)(combinedScale.y, combinedScale.z));
                desc.style.color = Color(0.f, 1.f, 0.f, 1.f);
                desc.style.depthEnabled = false;
                GAME->Draw_DebugSphere(desc);
                break;
            }

            case ModelCollisionProxyType::Capsule:
            {
                const Vec3 combinedScale = MultiplyAbsComponents(preScale, worldScale);
                const float scaledHalfHeight = proxy.halfHeight * fabsf(combinedScale.y);
                const float scaledRadius = proxy.radius * (std::max)(fabsf(combinedScale.x), fabsf(combinedScale.z));

                DebugBoxDesc desc{};
                desc.center = Vec3::Transform(proxy.center, combinedMatrix);
                desc.extents = Vec3(scaledRadius, scaledHalfHeight + scaledRadius, scaledRadius);
                desc.rotation = worldRotation * (preRotation * proxy.rotation);
                desc.style.color = Color(1.f, 1.f, 0.f, 1.f);
                desc.style.depthEnabled = false;
                GAME->Draw_DebugBox(desc);
                break;
            }

            default:
                break;
            }
        }

        for (const ModelCollisionMesh& mesh : collisionData->triangleMeshes)
        {
            vector<Vec3> worldPoints;
            worldPoints.reserve(mesh.positions.size());

            for (const Vec3& localPoint : mesh.positions)
                worldPoints.push_back(Vec3::Transform(localPoint, combinedMatrix));

            DrawBoundsFromWorldPoints(worldPoints, Color(1.f, 0.f, 0.f, 1.f));
        }
    }
}

IMPLEMENT_REFLECTION(MapStaticMesh)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MapStaticMesh";
    info.category = "World";
    return true;
}

MapStaticMesh::MapStaticMesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MapStaticMesh::MapStaticMesh(const MapStaticMesh& prototype)
    : GameObject{ prototype }
{
}

HRESULT MapStaticMesh::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MapStaticMesh::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
    {
        return E_FAIL;
    }

    auto pDesc = static_cast<MAPSTATICMESH_DESC*>(arg);
    _collisionData = pDesc->collisionData;

    Set_ShadowCasterType(ShadowCasterType::Static);

    if (FAILED(Ready_Components(pDesc->modelPath, pDesc->model)))
    {
        return E_FAIL;
    }

    wstring objectName = pDesc->name;
    if (objectName.empty() && _model != nullptr)
    {
        const wstring& sourceModelPath = _model->Get_SourceModelPath();
        if (!sourceModelPath.empty())
            objectName = fs::path(sourceModelPath).stem().wstring();
    }
    if (objectName.empty() && !pDesc->modelPath.empty())
        objectName = fs::path(pDesc->modelPath).stem().wstring();
    if (!objectName.empty())
        Set_Name(objectName);

    GAME->Register_StaticShadowCaster(GetSharedPtr<GameObject>());

    _transformCom->Set_Position(pDesc->position);
    _transformCom->Set_RotationQuaternion(pDesc->rotation);
    _transformCom->Set_Scale(pDesc->scale);

    if (_collisionData == nullptr && _model != nullptr)
    {
        _collisionData = MapStaticCollisionBuilder::Build(_model);
    }

    if (_collisionData != nullptr)
    {
        _physxStaticHandle = GAME->Register_PhysXStaticCollision(
            *_collisionData, _transformCom->Get_WorldMatrix(), Collision_Preset::Enviroment);
    }

    return S_OK;
}

void MapStaticMesh::Priority_Update(float timeDelta)
{
}

void MapStaticMesh::Update(float timeDelta)
{
}

void MapStaticMesh::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (g_toggleOwner == nullptr)
        g_toggleOwner = this;

    if (this == g_toggleOwner && GAME->KeyDown(KEY_TYPE::F2))
        g_showCollisionDebug = !g_showCollisionDebug;

    Vec3 localMin{}, localMax{};
    if (_model->Try_Get_LocalBounds(localMin, localMax))
    {
        BoundingBox localBox;
        BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));

        BoundingBox worldBox{};
        localBox.Transform(worldBox, _model->Get_PreLocalTransformMatrix() * _transformCom->Get_WorldMatrix());

        if (!GAME->IsVisible(worldBox))
        {
            return;
        }

        if (g_showCollisionDebug && _collisionData != nullptr)
        {
            Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
            if (activeCamera != nullptr)
            {
                const Vec3 cameraPos = activeCamera->Get_Transform()->Get_Position();
                if ((worldBox.Center - cameraPos).Length() <= kCollisionDebugMaxDistance)
                    DrawStaticCollisionDebug(_collisionData, _transformCom->Get_WorldMatrix());
            }
        }
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MapStaticMesh::Render()
{
    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }


    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
        {
            continue;
        }

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
        {
            continue;
        }

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
        {
            return E_FAIL;
        }

        const uint32 passIndex = static_cast<uint32>(material->Get_PassIndex());
        if (FAILED(_shader->Begin(passIndex)))
        {
            return E_FAIL;
        }

        _model->Render(i);
    }

    return S_OK;
}

HRESULT MapStaticMesh::Render_Shadow()
{
    if (_shader == nullptr || _model == nullptr)
    {
        return S_OK;
    }

    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_StaticShadowCB(_shader)))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(1)))
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT MapStaticMesh::RenderPickingID(uint32 id)
{
    if (!_model || !_shader)
    {
        return S_OK;
    }

    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_CameraCB(_shader)))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_RawValue("g_iObjectID", &id, sizeof(uint32))))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(6)))
        {
            return E_FAIL;
        }
        _model->Render(i);
    }

    return S_OK;
}

bool MapStaticMesh::Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const
{
    if (_model == nullptr || _transformCom == nullptr)
        return false;

    Vec3 localMin{}, localMax{};
    if (!_model->Try_Get_LocalBounds(localMin, localMax))
        return false;

    BoundingBox localBox{};
    BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));

    localBox.Transform(
        outWorldBounds,
        _model->Get_PreLocalTransformMatrix() * _transformCom->Get_WorldMatrix());

    return true;
}

HRESULT MapStaticMesh::Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader)))
    {
        return E_FAIL;
    }

    if (sharedModel)
    {
        _model = sharedModel;
    }
    else
    {
        string pathA = String::ToString(modelPath);
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.001f));
        if (!_model)
            return E_FAIL;
    }

    return S_OK;
}

HRESULT MapStaticMesh::Bind_ShaderResources()
{
    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    return S_OK;
}

Shared<MapStaticMesh> MapStaticMesh::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MapStaticMesh>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MapStaticMesh");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MapStaticMesh::Clone(void* arg)
{
    auto clone = make_shared<MapStaticMesh>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        return nullptr;
    }

    return clone;
}

void MapStaticMesh::Free()
{
    if (_physxStaticHandle.Is_Valid())
    {
        GAME->Unregister_PhysXStaticActor(_physxStaticHandle);
        _physxStaticHandle = {};
    }

    __super::Free();
}
