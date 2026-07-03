#include "MapStaticMesh.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "Collision_Define.h"
#include "MapStaticCollisionBuilder.h"
#include "ModelCom.h"

#include "Camera.h"
#include "StaticCollisionData.h"
#include "ShaderPassFinder.h"

namespace
{
    constexpr float kCollisionDebugMaxDistance = 35.f;
    constexpr const wchar_t* kMapStaticModelComponentTag = L"Com_Model";

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
    : GameObject{ prototype },
      _sourceStaticMeshIndex{ prototype._sourceStaticMeshIndex },
      _sourceStaticMeshId{ prototype._sourceStaticMeshId },
      _sourceStaticMeshModelPath{ prototype._sourceStaticMeshModelPath },
      _sourceMapPath{ prototype._sourceMapPath }
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

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

    auto pDesc = static_cast<MAPSTATICMESH_DESC*>(arg);
    if (pDesc == nullptr)
    {
        if (FAILED(Ready_EmptyModelComponent()))
        {
            return E_FAIL;
        }

        return S_OK;
    }

    _collisionData = pDesc->collisionData;
    _usesStaticCollision = (_collisionData != nullptr);
    _sourceStaticMeshIndex = pDesc->sourceStaticMeshIndex;
    _visible = pDesc->visible;
    _cullEnabled = pDesc->cullEnabled;
    _castShadow = pDesc->castShadow;
    _sourceStaticMeshId = pDesc->name;
    _sourceStaticMeshModelPath = pDesc->modelPath;
    _sourceMapPath = pDesc->sourceMapPath;

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

    _transformCom->Set_Position(pDesc->position);
    _transformCom->Set_RotationQuaternion(pDesc->rotation);
    _transformCom->Set_Scale(pDesc->scale);

    if (_collisionData != nullptr)
    {
        _physxStaticHandle = GAME->Register_PhysXStaticCollision(
            *_collisionData,
            _transformCom->Get_WorldMatrix(),
            Collision_Preset::Environment);
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
    Refresh_HasEclipseGlow();

    UNREFERENCED_PARAMETER(timeDelta);

    if (!_visible || _model == nullptr)
        return;

    if (ClientInstance::Is_DebugDisableMapStaticMeshRender())
        return;

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

        if (!ClientInstance::Should_RenderMapObjectInPotatoBagArena(worldBox))
            return;

        const bool suppressPotatoBagArenaMapShadow = ClientInstance::Is_PotatoBagArenaMapRenderGateActive();

        if (_cullEnabled && !GAME->IsVisible(worldBox))
        {
            if (!suppressPotatoBagArenaMapShadow && _castShadow && Casts_DynamicShadow() && !ClientInstance::Is_DebugDisableMapStaticMeshShadow())
                GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
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

        //if (g_showCollisionDebug)
        //{
        //    DebugBoxDesc boundsDesc{};
        //    boundsDesc.center = worldBox.Center;
        //    boundsDesc.extents = worldBox.Extents;
        //    boundsDesc.style.color = Color(1.f, 1.f, 1.f, 1.f);
        //    boundsDesc.style.depthEnabled = false;
        //    GAME->Draw_DebugBox(boundsDesc);
        //}
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (_hasEclipseGlow)
        GAME->Add_RenderGroup(RenderGroup::Glow, GetSharedPtr<GameObject>());

    if (!ClientInstance::Is_PotatoBagArenaMapRenderGateActive() &&
        _castShadow && Casts_DynamicShadow() && !ClientInstance::Is_DebugDisableMapStaticMeshShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT MapStaticMesh::Render()
{
    if (_shader == nullptr || _model == nullptr)
    {
        return S_OK;
    }

    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Reset_DrawFeatures()))
    {
        return E_FAIL;
    }

    const bool isGlowPass = (GAME->Get_CurrentRenderGroup() == RenderGroup::Glow);
    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    if (isGlowPass)
    {
        for (uint32 i = 0; i < iNumMeshes; i++)
        {
            const Shared<Material> material = _model->Get_Material(_model->Get_MeshMaterialIndex(i));
            if (material)
            {
                if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
                {
                    return E_FAIL;
                }
            }
            if (FAILED(_shader->Begin(static_cast<int32>(VtxMeshPassIndex::EclipseSpherePass))))
            {
                return E_FAIL;
            }
            _model->Render(i);
        }
        return S_OK;
    }

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

        uint32 passIndex = static_cast<uint32>(material->Get_PassIndex());
        if (passIndex == static_cast<uint32>(VtxMeshPassIndex::EclipseSpherePass))
        {
            passIndex = static_cast<uint32>(VtxMeshPassIndex::DefaultPass);
        }

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
    if (_shader == nullptr || _model == nullptr || _transformCom == nullptr)
    {
        return S_OK;
    }

    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_ShadowCB(_shader)))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(1)))
            return E_FAIL;
        _model->Render(i);
    }

    return S_OK;
}

HRESULT MapStaticMesh::RenderPickingID(uint32 id)
{
    if (!_model || !_shader || _transformCom == nullptr)
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

    if (FAILED(_shader->Reset_DrawFeatures()))
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
        _model = make_shared<ModelCom>(*sharedModel);
        if (_model == nullptr || FAILED(_model->Initialize(nullptr)))
            return E_FAIL;
    }
    else
    {
        string pathA = String::ToString(modelPath);
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.001f));
        if (!_model)
            return E_FAIL;
    }

    Register_ModelComponent();

    return S_OK;
}

HRESULT MapStaticMesh::Ready_EmptyModelComponent()
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader)))
    {
        return E_FAIL;
    }

    _model = make_shared<ModelCom>(_device, _context);
    if (_model == nullptr || FAILED(_model->Initialize(nullptr)))
    {
        return E_FAIL;
    }

    Register_ModelComponent();

    return S_OK;
}

void MapStaticMesh::Register_ModelComponent()
{
    if (_model == nullptr)
        return;

    _model->Set_Owner(GetSharedPtr<GameObject>());

    if (_components.find(kMapStaticModelComponentTag) == _components.end())
        _components.emplace(kMapStaticModelComponentTag, static_pointer_cast<Component>(_model));
}

void MapStaticMesh::Rebuild_StaticCollisionFromModel()
{
    if (_physxStaticHandle.Is_Valid())
    {
        GAME->Unregister_PhysXStaticActor(_physxStaticHandle);
        _physxStaticHandle = {};
    }

    _collisionData.reset();

    if (!_usesStaticCollision || _model == nullptr)
        return;

    _collisionData = MapStaticCollisionBuilder::Build(_model);
    if (_collisionData == nullptr)
        return;

    _physxStaticHandle = GAME->Register_PhysXStaticCollision(
        *_collisionData,
        _transformCom->Get_WorldMatrix(),
        Collision_Preset::Environment);
}

HRESULT MapStaticMesh::Bind_ShaderResources()
{
    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    return S_OK;
}

json MapStaticMesh::To_Json() const
{
    json root = __super::To_Json();
    root["map_static_collision_enabled"] = _usesStaticCollision;
    root["Visible"] = _visible;
    root["CullEnabled"] = _cullEnabled;
    root["CastShadow"] = _castShadow;

    bool hasModelComponentJson = false;
    if (root.contains("components") && root["components"].is_array())
    {
        for (const json& componentJson : root["components"])
        {
            if (!componentJson.is_object())
                continue;

            if (componentJson.value("prototype_tag", string{}) == String::ToString(kMapStaticModelComponentTag))
            {
                hasModelComponentJson = true;
                break;
            }
        }
    }

    if (!hasModelComponentJson && _model != nullptr)
    {
        json modelJson = _model->To_Json();
        modelJson["prototype_tag"] = String::ToString(kMapStaticModelComponentTag);

        if (!root.contains("components") || !root["components"].is_array())
            root["components"] = json::array();

        root["components"].emplace_back(modelJson);
    }

    return root;
}

void MapStaticMesh::From_Json(const json& data)
{
    _usesStaticCollision = data.value("map_static_collision_enabled", false);
    _visible = data.value("Visible", true);
    _cullEnabled = data.value("CullEnabled", true);
    _castShadow = data.value("CastShadow", true);

    __super::From_Json(data);

    if (_model != nullptr)
    {
        const wstring& sourceModelPath = _model->Get_SourceModelPath();
        if (!sourceModelPath.empty())
            Set_Name(fs::path(sourceModelPath).stem().wstring());
    }

    Rebuild_StaticCollisionFromModel();
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

void MapStaticMesh::Refresh_HasEclipseGlow()
{
    _hasEclipseGlow = false;
    if (!_model)
    {
        return;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const Shared<Material> mat = _model->Get_Material(_model->Get_MeshMaterialIndex(i));
        if (mat && mat->Get_PassIndex() == VtxMeshPassIndex::EclipseSpherePass)
        {
            _hasEclipseGlow = true;
            return;
        }
    }
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
