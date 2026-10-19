#include "MapInstancedMesh.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "InstanceCullParams.h"
#include "Camera.h"
#include "Shadow.h"
#include "StaticCollisionData.h"
#include "Collision_Define.h"
#include "ShaderPassFinder.h"
#include "Renderer_Define.h"

namespace
{
    constexpr float kExtendedGestralCullDistanceMultiplier = 2.f;

    float DistanceSq_ToBounds(const Vec3& point, const BoundingBox& bounds)
    {
        const Vec3 minPoint = bounds.Center - bounds.Extents;
        const Vec3 maxPoint = bounds.Center + bounds.Extents;

        float dx = 0.f;
        if (point.x < minPoint.x)
            dx = minPoint.x - point.x;
        else if (point.x > maxPoint.x)
            dx = point.x - maxPoint.x;

        float dy = 0.f;
        if (point.y < minPoint.y)
            dy = minPoint.y - point.y;
        else if (point.y > maxPoint.y)
            dy = point.y - maxPoint.y;

        float dz = 0.f;
        if (point.z < minPoint.z)
            dz = minPoint.z - point.z;
        else if (point.z > maxPoint.z)
            dz = point.z - maxPoint.z;

        return dx * dx + dy * dy + dz * dz;
    }

    bool Is_GrassModelPath(const wstring& modelPath)
    {
        return modelPath.find(L"Grass") != wstring::npos || modelPath.find(L"grass") != wstring::npos;
    }

    bool Is_GrassOrPetalModelPath(const wstring& modelPath)
    {
        return Is_GrassModelPath(modelPath) ||
            modelPath.find(L"Petal") != wstring::npos ||
            modelPath.find(L"petal") != wstring::npos;
    }

    bool Is_SmallInstancedShadowModelPath(const wstring& modelPath)
    {
        return modelPath.find(L"Flower") != wstring::npos ||
            modelPath.find(L"flower") != wstring::npos ||
            modelPath.find(L"Bush") != wstring::npos ||
            modelPath.find(L"bush") != wstring::npos;
    }

    bool Is_GestralPalisadeModelPath(const wstring& modelPath)
    {
        return modelPath.find(L"Level_Main_GestralVillage_V2") != wstring::npos &&
            modelPath.find(L"Palisade_Spike") != wstring::npos;
    }

    bool Is_GestralTreeModelPath(const wstring& modelPath)
    {
        return modelPath.find(L"Level_Main_GestralVillage_V2") != wstring::npos &&
            (modelPath.find(L"Tree") != wstring::npos ||
                modelPath.find(L"tree") != wstring::npos ||
                modelPath.find(L"EuropeanBeech") != wstring::npos ||
                modelPath.find(L"EuropeanHornbeam") != wstring::npos);
    }

    float Resolve_CullDistance(const wstring& modelPath, float cullDist)
    {
        if (Is_GestralTreeModelPath(modelPath) || Is_GestralPalisadeModelPath(modelPath))
            return cullDist * kExtendedGestralCullDistanceMultiplier;

        return cullDist;
    }

    int Resolve_DirectRenderLodIndex(const Shared<ModelCom> (&lodModels)[3])
    {
        for (int lod = 0; lod < 3; ++lod)
        {
            if (lodModels[lod])
                return lod;
        }

        return -1;
    }
}

IMPLEMENT_REFLECTION(MapInstancedMesh)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MapInstancedMesh";
    info.category = "World";
    return true;
}

MapInstancedMesh::MapInstancedMesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MapInstancedMesh::MapInstancedMesh(const MapInstancedMesh& prototype)
    : GameObject{ prototype }
{
}

HRESULT MapInstancedMesh::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MapInstancedMesh::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
    {
        return E_FAIL;
    }

    if (FAILED(__super::Add_Component(ETOI(LevelType::Static),
        TEXT("ComputeShader_InstanceCull"), _cullShader)))
    {
        return E_FAIL;
    }

    if (FAILED(__super::Add_Component(ETOI(LevelType::Static),
        TEXT("ComputeShader_ShadowCull"), _shadowCullShader)))
    {
        return E_FAIL;
    }

    if (arg != nullptr)
    {
        auto pDesc = static_cast<MAPINSTANCEDMESH_DESC*>(arg);

        _modelPath = pDesc->modelPath;
        _transforms = pDesc->transforms;
        _collisionData = pDesc->collisionData;
        _visible = pDesc->visible;
        _cullEnabled = pDesc->cullEnabled;
        _castShadow = pDesc->castShadow;
        _cullDist = pDesc->cullDist;

        if (FAILED(Ready_Components(pDesc->modelPath, pDesc->model)))
        {
            return E_FAIL;
        }

        if (!pDesc->displayName.empty())
            Set_Name(pDesc->displayName);

        Refresh_HasGlow();

        vector<Matrix> matrices;
        matrices.reserve(pDesc->transforms.size());
        for (auto& t : pDesc->transforms)
        {
            Matrix world = Matrix::CreateScale(t._scale)
                * Matrix::CreateFromQuaternion(Quat(t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w))
                * Matrix::CreateTranslation(t._position);
            matrices.push_back(world);
        }

        if (FAILED(CreateInstanceBuffer(matrices)))
        {
            return E_FAIL;
        }

        CreateCullBuffers(matrices, pDesc->transforms);

        if (_collisionData != nullptr)
        {
            _physxStaticHandles.reserve(matrices.size());

            for (const Matrix& worldMatrix : matrices)
            {
                PhysXStaticActorHandle handle = GAME->Register_PhysXStaticCollision(
                    *_collisionData,
                    worldMatrix,
                    Collision_Preset::Environment);

                if (handle.Is_Valid())
                {
                    _physxStaticHandles.push_back(handle);
                }
            }
        }
    }

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

    return S_OK;
}

void MapInstancedMesh::Priority_Update(float timeDelta)
{
}

void MapInstancedMesh::Update(float timeDelta)
{
    _accTime += timeDelta;
}

void MapInstancedMesh::Late_Update(float timeDelta)
{
    if (!_visible || _instanceCount == 0)
    {
        return;
    }

    if (ClientInstance::Is_DebugDisableMapInstancedMeshRender())
    {
        return;
    }

    if (ClientInstance::Is_DebugDisableGrassInstancedRender() && Is_GrassModelPath(_modelPath))
    {
        return;
    }

    if (!ClientInstance::Should_RenderMapObjectInPotatoBagArena(_groupBounds))
    {
        return;
    }

    const Vec4* cam4 = GAME->Get_CamPosition();
    const bool hasCamPos = cam4 != nullptr;
    const Vec3 camPos = hasCamPos ? Vec3(cam4->x, cam4->y, cam4->z) : Vec3{};

    auto submitShadowIfInRange = [&]()
    {
        if (ClientInstance::Is_PotatoBagArenaMapRenderGateActive())
        {
            return;
        }

        if (!_castShadow || !Casts_DynamicShadow())
        {
            return;
        }

        if (ClientInstance::Is_DebugDisableMapInstancedMeshShadow())
        {
            return;
        }

        if (ClientInstance::Is_DebugDisableGrassInstancedShadow() && Is_GrassOrPetalModelPath(_modelPath))
        {
            return;
        }

        const ShadowLightDesc* shadowDesc = GAME->Get_ShadowLightDesc();
        if (shadowDesc != nullptr && hasCamPos)
        {
            const float shadowRange = shadowDesc->focusFar + shadowDesc->zPadding;
            if (DistanceSq_ToBounds(camPos, _groupBounds) <= shadowRange * shadowRange)
            {
                GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
            }
            return;
        }

        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
    };

    //if (_cullEnabled)
    //{
    //    if (!GAME->IsVisible(_groupBounds))
    //    {
    //        submitShadowIfInRange();
    //        return;
    //    }

    //    if (hasCamPos)
    //    {
    //        const float cullDist = _cullDist;
    //        if (DistanceSq_ToBounds(camPos, _groupBounds) > cullDist * cullDist)
    //        {
    //            submitShadowIfInRange();
    //            return;
    //        }
    //    }
    //}

    if (!GAME->IsVisible(_groupBounds))
    {
        submitShadowIfInRange();
        return;
    }

    if (hasCamPos)
    {
        const float cullDist = Resolve_CullDistance(_modelPath, _cullDist);
        if (DistanceSq_ToBounds(camPos, _groupBounds) > cullDist * cullDist)
        {
            submitShadowIfInRange();
            return;
        }
    }
    /////

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    if (_hasGlow)
    {
        GAME->Add_RenderGroup(RenderGroup::Glow, GetSharedPtr<GameObject>());
    }

    submitShadowIfInRange();
}

HRESULT MapInstancedMesh::Render()
{
    if (_instanceCount == 0 || !_shader)
    {
        return S_OK;
    }

    const int directRenderLodIndex = Resolve_DirectRenderLodIndex(_lodModels);

    Buffer* instanceVB = _instanceVB.Get();
    uint32 renderInstanceCount = _instanceCount;
    const bool usePotatoBagArenaInstances = ClientInstance::Is_PotatoBagArenaMapRenderGateActive();
    if (usePotatoBagArenaInstances)
    {
        if (FAILED(Prepare_PotatoBagArenaInstanceBuffer()))
        {
            return E_FAIL;
        }

        if (_potatoBagArenaInstanceCount == 0 || !_potatoBagArenaInstanceVB)
        {
            return S_OK;
        }

        instanceVB = _potatoBagArenaInstanceVB.Get();
        renderInstanceCount = _potatoBagArenaInstanceCount;
    }

#pragma region for using Renderdoc
    //ComPtr<ID3DUserDefinedAnnotation> annotation;
    //_context->QueryInterface(IID_PPV_ARGS(&annotation));
    //struct ScopedMarker
    //{
    //    ID3DUserDefinedAnnotation* a;
    //    ~ScopedMarker() { if (a) a->EndEvent(); }
    //} marker{ annotation.Get() };
    //if (annotation)
    //{
    //    wstring label = L"InstancedMesh: " + _modelPath;
    //    annotation->BeginEvent(label.c_str());
    //}
#pragma endregion

    if (GAME->Get_CurrentRenderGroup() == RenderGroup::Glow)
    {
        if (FAILED(Bind_ShaderResources()))
        {
            return E_FAIL;
        }

        for (int lod = 0; lod < 3; ++lod)
        {
            if (!_lodModels[lod])
            {
                continue;
            }

            const uint32 numMeshes = static_cast<uint32>(_lodModels[lod]->Get_NumMeshes());
            for (uint32 i = 0; i < numMeshes; ++i)
            {
                const Shared<Material> material = _lodModels[lod]->Get_Material(i);
                if (!material)
                {
                    continue;
                }

                if (material->Get_PassIndex() == VtxInstanceMeshExactPassIndex::UnlitFlamePass)
                {
                    continue;
                }

                if (FAILED(_shader->Bind_MaterialTextures(*material)))
                {
                    return E_FAIL;
                }

                if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
                {
                    return E_FAIL;
                }

                if (FAILED(_shader->Begin(VtxInstanceMeshExactPassIndex::EmissiveGlowPass)))
                {
                    return E_FAIL;
                }

                if (FAILED(_lodModels[lod]->RenderInstanced(i, instanceVB, sizeof(Matrix), renderInstanceCount)))
                {
                    return E_FAIL;
                }
            }
        }
        return S_OK;
    }

    const bool forceGestralPalisadeCull =
        ClientInstance::Is_DebugForceGestralPalisadeInstancedCull() &&
        Is_GestralPalisadeModelPath(_modelPath);
    const bool useCullPath = !usePotatoBagArenaInstances && (_cullEnabled || forceGestralPalisadeCull)
        && !ClientInstance::Is_DebugDisableInstanceComputeCull();

    bool useIndirect = false;
    if (useCullPath && _cullShader && _instanceSrcBuffer && _sphereBuffer && _visibleVB[0] && !_indirectArgsBuffers[0].empty())
    {
        DispatchCull();
        useIndirect = true;
    }
    else
    {
        if (directRenderLodIndex < 0)
        {
            return S_OK;
        }

        _debugLastDrawInstanceCounts = {};
        _debugLastDrawInstanceCounts[directRenderLodIndex] = renderInstanceCount;
        _debugHasDrawInstanceCounts = true;
        _debugLastVisibleCountReadbackRequestId =
            ClientInstance::Get_DebugMapInstancedVisibleCountReadbackRequestId();
    }

    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    for (int lod = 0; lod < 3; ++lod)
    {
        if (!useIndirect && lod != directRenderLodIndex)
        {
            continue;
        }

        if (!_lodModels[lod])
        {
            continue;
        }

        const uint32 numMeshes = static_cast<uint32>(_lodModels[lod]->Get_NumMeshes());
        for (uint32 i = 0; i < numMeshes; ++i)
        {
            const Shared<Material> material = _lodModels[lod]->Get_Material(i);
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

            if (useIndirect)
            {
                if (FAILED(_lodModels[lod]->RenderInstancedIndirect(i, _visibleVB[lod].Get(), sizeof(Matrix), _indirectArgsBuffers[lod][i].Get())))
                {
                    return E_FAIL;
                }
            }
            else
            {
                if (FAILED(_lodModels[lod]->RenderInstanced(i, instanceVB, sizeof(Matrix), renderInstanceCount)))
                {
                    return E_FAIL;
                }
            }
        }
    }

    return S_OK;
}

HRESULT MapInstancedMesh::RenderPickingID(uint32 id)
{
    if (_instanceCount == 0 || !_lodModels[0] || !_shader)
    {
        return S_OK;
    }

    if (FAILED(GAME->Bind_CameraCB(_shader)))
    {
        return E_FAIL;
    }

    Matrix identity = Matrix::Identity;
    if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity)))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_RawValue("g_iObjectID", &id, sizeof(uint32))))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_lodModels[0]->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(VtxInstanceMeshExactPassIndex::IDPass)))
        {
            return E_FAIL;
        }

        if (FAILED(_lodModels[0]->RenderInstanced(i, _instanceVB.Get(), sizeof(Matrix), _instanceCount)))
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT MapInstancedMesh::Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMeshInstanceExact"), _shader)))
    {
        return E_FAIL;
    }

    if (sharedModel)
    {
        _lodModels[0] = sharedModel;
    }
    else
    {
        string pathA = String::ToString(modelPath);
        _lodModels[0] = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.01f));
        if (!_lodModels[0])
        {
            return E_FAIL;
        }
    }

    wstring basePath = modelPath;
    wstring ext = L".model";
    if (basePath.size() > ext.size())
    {
        basePath = basePath.substr(0, basePath.size() - ext.size());
    }

    for (int lod = 1; lod <= 2; ++lod)
    {
        wstring lodPath = basePath + L"_lod" + to_wstring(lod) + ext;
        string lodPathA = String::ToString(lodPath);
        if (GetFileAttributesA(lodPathA.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            continue;
        }

        _lodModels[lod] = ModelCom::Create(_device, _context, ModelType::NonAnim, lodPathA.c_str(), Matrix::CreateScale(0.01f));
    }

    for (int lod = 1; lod <= 2; ++lod)
    {
        if (!_lodModels[lod])
        {
            continue;
        }
        _lodModels[lod]->Get_MaterialBindings() = _lodModels[0]->Get_MaterialBindings();
        _lodModels[lod]->Apply_BaseMaterialFromBundle_AllSlots();
    }

    return S_OK;
}

void MapInstancedMesh::Set_LodModel(int lod, Shared<ModelCom> model)
{
    _lodModels[lod] = model;

    if (!model || _indirectArgsBuffers[lod].empty())
    {
        return;
    }

    const uint32 numMeshes = static_cast<uint32>(model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes && i < _indirectArgsBuffers[lod].size(); ++i)
    {
        uint32 args[5] = { static_cast<uint32>(model->Get_NumIndices(i)), _instanceCount, 0, 0, 0 };
        _context->UpdateSubresource(_indirectArgsBuffers[lod][i].Get(), 0, nullptr, args, 0, 0);
    }
}

bool MapInstancedMesh::Get_RenderHotspotInstancedMeshStats(RenderHotspotInstancedMeshStats& outStats) const
{
    outStats = {};
    outStats.cullEnabled = _cullEnabled;
    outStats.castShadow = _castShadow;
    outStats.hasCullResources =
        _cullShader &&
        _instanceSrcBuffer &&
        _sphereBuffer &&
        _visibleVB[0] &&
        !_indirectArgsBuffers[0].empty();
    const bool forceGestralPalisadeCull =
        ClientInstance::Is_DebugForceGestralPalisadeInstancedCull() &&
        Is_GestralPalisadeModelPath(_modelPath);
    outStats.usesIndirectCull = (_cullEnabled || forceGestralPalisadeCull) && outStats.hasCullResources;
    outStats.sourceInstanceCount = _instanceCount;
    outStats.cullDist = Resolve_CullDistance(_modelPath, _cullDist);
    outStats.lod1Dist = _lod1Dist;
    outStats.lod2Dist = _lod2Dist;

    const int directRenderLodIndex = Resolve_DirectRenderLodIndex(_lodModels);

    for (uint32 lod = 0; lod < 3; ++lod)
    {
        const Shared<ModelCom>& model = _lodModels[lod];
        if (!model)
            continue;

        outStats.lodMask |= (1u << lod);

        const uint32 meshCount = static_cast<uint32>(model->Get_NumMeshes());
        outStats.lodMeshCounts[lod] = meshCount;
        if (outStats.usesIndirectCull || static_cast<int>(lod) == directRenderLodIndex)
        {
            outStats.estimatedMainDrawCount += meshCount;
        }

        uint32 indexCount = 0;
        for (uint32 meshIndex = 0; meshIndex < meshCount; ++meshIndex)
            indexCount += model->Get_NumIndices(meshIndex);

        outStats.lodIndexCounts[lod] = indexCount;
    }

    if (outStats.usesIndirectCull)
    {
        for (uint32 lod = 0; lod < 3; ++lod)
        {
            if (!_lodModels[lod])
            {
                continue;
            }

            ++outStats.estimatedCullCopyResourceCount;
            outStats.estimatedCullCopyCountCount += static_cast<uint32>(_indirectArgsBuffers[lod].size());
        }
    }

    outStats.sourceIndexWorkLod0 =
        static_cast<uint64>(outStats.sourceInstanceCount) *
        static_cast<uint64>(outStats.lodIndexCounts[0]);

    if (outStats.usesIndirectCull)
    {
        outStats.hasDrawInstanceCounts = _debugHasDrawInstanceCounts;
        outStats.drawInstanceCounts = _debugLastDrawInstanceCounts;
        outStats.visibleCountReadbackRequestId = _debugLastVisibleCountReadbackRequestId;
    }
    else
    {
        outStats.hasDrawInstanceCounts = true;
        if (directRenderLodIndex >= 0)
        {
            outStats.drawInstanceCounts[directRenderLodIndex] = _instanceCount;
        }
        outStats.visibleCountReadbackRequestId =
            ClientInstance::Get_DebugMapInstancedVisibleCountReadbackRequestId();
    }

    if (outStats.hasDrawInstanceCounts)
    {
        for (uint32 lod = 0; lod < 3; ++lod)
        {
            outStats.drawInstanceCountTotal += outStats.drawInstanceCounts[lod];
            outStats.visibleIndexWork +=
                static_cast<uint64>(outStats.drawInstanceCounts[lod]) *
                static_cast<uint64>(outStats.lodIndexCounts[lod]);
        }
    }

    return true;
}

HRESULT MapInstancedMesh::CreateInstanceBuffer(const vector<Matrix>& matrices)
{
    const uint32 count = static_cast<uint32>(matrices.size());
    if (count == 0)
    {
        return S_OK;
    }

    _instanceCapacity = count;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(Matrix) * _instanceCapacity;
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = matrices.data();

    _instanceVB.Reset();

    if (FAILED(_device->CreateBuffer(&bd, &sd, _instanceVB.GetAddressOf())))
    {
        return E_FAIL;
    }

    _instanceCount = count;
    return S_OK;
}

HRESULT MapInstancedMesh::Prepare_PotatoBagArenaInstanceBuffer()
{
    if (!ClientInstance::Is_PotatoBagArenaMapRenderGateActive())
    {
        _potatoBagArenaInstanceBufferReady = false;
        _potatoBagArenaInstanceCount = 0;
        _potatoBagArenaInstanceBufferRevision = ClientInstance::Get_PotatoBagArenaMapRenderGateRevision();
        return S_OK;
    }

    const uint32 renderGateRevision = ClientInstance::Get_PotatoBagArenaMapRenderGateRevision();
    if (_potatoBagArenaInstanceBufferReady &&
        _potatoBagArenaInstanceBufferRevision == renderGateRevision)
    {
        return S_OK;
    }

    Vec3 localMin{}, localMax{};
    const bool hasBounds = _lodModels[0] && _lodModels[0]->Try_Get_LocalBounds(localMin, localMax);
    const Matrix preTransform = _lodModels[0] ? _lodModels[0]->Get_PreLocalTransformMatrix() : Matrix::Identity;

    BoundingBox localBox{};
    if (hasBounds)
    {
        BoundingBox::CreateFromPoints(localBox,
            XMVectorSet(localMin.x, localMin.y, localMin.z, 0.f),
            XMVectorSet(localMax.x, localMax.y, localMax.z, 0.f));
    }

    vector<Matrix> filteredMatrices;
    filteredMatrices.reserve(_transforms.size());

    for (const InstanceTransform& transform : _transforms)
    {
        const Matrix world =
            Matrix::CreateScale(transform._scale) *
            Matrix::CreateFromQuaternion(Quat(transform._rotation.x, transform._rotation.y, transform._rotation.z, transform._rotation.w)) *
            Matrix::CreateTranslation(transform._position);

        BoundingSphere worldSphere{};
        if (hasBounds)
        {
            BoundingBox worldBox{};
            localBox.Transform(worldBox, preTransform * world);
            BoundingSphere::CreateFromBoundingBox(worldSphere, worldBox);
        }
        else
        {
            worldSphere.Center = transform._position;
            worldSphere.Radius = 500.f;
        }

        if (ClientInstance::Should_RenderMapInstanceInPotatoBagArena(worldSphere))
        {
            filteredMatrices.push_back(world);
        }
    }

    _potatoBagArenaInstanceCount = static_cast<uint32>(filteredMatrices.size());
    _potatoBagArenaInstanceBufferRevision = renderGateRevision;
    _potatoBagArenaInstanceBufferReady = true;

    if (_potatoBagArenaInstanceCount == 0)
    {
        return S_OK;
    }

    if (!_potatoBagArenaInstanceVB)
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Matrix) * _instanceCapacity;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(_device->CreateBuffer(&bd, nullptr, _potatoBagArenaInstanceVB.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_potatoBagArenaInstanceVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return E_FAIL;
    }

    memcpy(mapped.pData, filteredMatrices.data(), sizeof(Matrix) * _potatoBagArenaInstanceCount);
    _context->Unmap(_potatoBagArenaInstanceVB.Get(), 0);

    return S_OK;
}

HRESULT MapInstancedMesh::Bind_ShaderResources()
{
    if (FAILED(GAME->Bind_CameraCB(_shader)))
    {
        return E_FAIL;
    }

    Matrix identity = Matrix::Identity;
    if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity)))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_RawValue("g_fAccTime", &_accTime, sizeof(float))))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MapInstancedMesh::CreateCullBuffers(const vector<Matrix>& matrices, const vector<InstanceTransform>& transforms)
{
    const uint32 count = static_cast<uint32>(matrices.size());
    if (count == 0)
    {
        return S_OK;
    }

    Vec3 localMin{}, localMax{};
    const bool hasBounds = _lodModels[0] && _lodModels[0]->Try_Get_LocalBounds(localMin, localMax);
    const Matrix preTransform = _lodModels[0] ? _lodModels[0]->Get_PreLocalTransformMatrix() : Matrix::Identity;

    bool hasMergedBounds = false;
    vector<Vec4> spheres;
    spheres.reserve(count);

    for (uint32 i = 0; i < count; ++i)
    {
        BoundingBox worldBox;
        if (hasBounds)
        {
            BoundingBox localBox;
            BoundingBox::CreateFromPoints(localBox,
                XMVectorSet(localMin.x, localMin.y, localMin.z, 0.f),
                XMVectorSet(localMax.x, localMax.y, localMax.z, 0.f));

            localBox.Transform(worldBox, preTransform * matrices[i]);

            BoundingSphere sphere;
            BoundingSphere::CreateFromBoundingBox(sphere, worldBox);
            spheres.push_back({ sphere.Center.x, sphere.Center.y, sphere.Center.z, sphere.Radius });
        }
        else
        {
            worldBox.Center = Vec3(matrices[i].m[3][0], matrices[i].m[3][1], matrices[i].m[3][2]);
            worldBox.Extents = Vec3(500.f, 500.f, 500.f);

            spheres.push_back({ matrices[i].m[3][0], matrices[i].m[3][1], matrices[i].m[3][2], 500.f });
        }

        if (!hasMergedBounds)
        {
            _groupBounds = worldBox;
            hasMergedBounds = true;
        }
        else
        {
            BoundingBox::CreateMerged(_groupBounds, _groupBounds, worldBox);
        }
    }

    if (!_cullShader)
    {
        return S_OK;
    }

    _instanceSrcBuffer = ComputeStructuredBuffer::Create(_device, _context, sizeof(Matrix), count, matrices.data());
    if (!_instanceSrcBuffer)
    {
        return E_FAIL;
    }

    _sphereBuffer = ComputeStructuredBuffer::Create(_device, _context,
        sizeof(Vec4), count, spheres.data());
    if (!_sphereBuffer)
    {
        return E_FAIL;
    }

    for (int lod = 0; lod < 3; ++lod)
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Matrix) * count;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = sizeof(Matrix);

        _visibleStructured[lod].Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _visibleStructured[lod].GetAddressOf())))
        {
            return E_FAIL;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format = DXGI_FORMAT_UNKNOWN;
        uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavd.Buffer.FirstElement = 0;
        uavd.Buffer.NumElements = count;
        uavd.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

        _visibleUAV[lod].Reset();
        if (FAILED(_device->CreateUnorderedAccessView(_visibleStructured[lod].Get(), &uavd, _visibleUAV[lod].GetAddressOf())))
        {
            return E_FAIL;
        }

        D3D11_BUFFER_DESC vbd{};
        vbd.ByteWidth = sizeof(Matrix) * count;
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        _visibleVB[lod].Reset();
        if (FAILED(_device->CreateBuffer(&vbd, nullptr, _visibleVB[lod].GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(CullParams);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        _cullCB.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _cullCB.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    const uint32 numMeshes = _lodModels[0] ? static_cast<uint32>(_lodModels[0]->Get_NumMeshes()) : 1;
    for (int lod = 0; lod < 3; ++lod)
    {
        _indirectArgsBuffers[lod].clear();
        _indirectArgsBuffers[lod].resize(numMeshes);

        for (uint32 i = 0; i < numMeshes; ++i)
        {
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = 5 * sizeof(uint32);
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

            if (FAILED(_device->CreateBuffer(&bd, nullptr, _indirectArgsBuffers[lod][i].GetAddressOf())))
            {
                return E_FAIL;
            }

            uint32 args[5] = { 0, count, 0, 0, 0 };
            if (_lodModels[0])
            {
                args[0] = static_cast<uint32>(_lodModels[0]->Get_NumIndices(i));
            }

            _context->UpdateSubresource(_indirectArgsBuffers[lod][i].Get(), 0, nullptr, args, 0, 0);
        }
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Matrix) * count;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = sizeof(Matrix);

        _shadowVisibleStructured.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _shadowVisibleStructured.GetAddressOf())))
        {
            return E_FAIL;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format = DXGI_FORMAT_UNKNOWN;
        uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavd.Buffer.FirstElement = 0;
        uavd.Buffer.NumElements = count;
        uavd.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

        _shadowVisibleUAV.Reset();
        if (FAILED(_device->CreateUnorderedAccessView(_shadowVisibleStructured.Get(), &uavd, _shadowVisibleUAV.GetAddressOf())))
        {
            return E_FAIL;
        }

        D3D11_BUFFER_DESC vbd{};
        vbd.ByteWidth = sizeof(Matrix) * count;
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        _shadowVisibleVB.Reset();
        if (FAILED(_device->CreateBuffer(&vbd, nullptr, _shadowVisibleVB.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(ShadowCullParams);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        _shadowCullCB.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _shadowCullCB.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    return CreateShadowIndirectArgsBuffers();
}

HRESULT MapInstancedMesh::Render_Shadow()
{
    if (ClientInstance::Is_PotatoBagArenaMapRenderGateActive())
    {
        return S_OK;
    }

    if (GAME->Is_UseInstancedShadowCache() && GAME->Has_InstancedShadowCache())
    {
        return S_OK;
    }

    if (_shader == nullptr || _lodModels[0] == nullptr || _instanceCount == 0)
    {
        return S_OK;
    }

    Buffer* instanceVB = _instanceVB.Get();
    uint32 renderInstanceCount = _instanceCount;
    const bool usePotatoBagArenaInstances = ClientInstance::Is_PotatoBagArenaMapRenderGateActive();
    if (usePotatoBagArenaInstances)
    {
        if (FAILED(Prepare_PotatoBagArenaInstanceBuffer()))
        {
            return E_FAIL;
        }

        if (_potatoBagArenaInstanceCount == 0 || !_potatoBagArenaInstanceVB)
        {
            return S_OK;
        }

        instanceVB = _potatoBagArenaInstanceVB.Get();
        renderInstanceCount = _potatoBagArenaInstanceCount;
    }

    const bool isBaking = GAME->Is_BakingInstancedShadow();

    if (!isBaking && GAME->Get_ActiveShadowCascade() > _maxShadowCascade)
    {
        return S_OK;
    }

    const uint32 cascadeIndex = isBaking ? 0u : GAME->Get_ActiveShadowCascade();
    uint32 shadowLod = Resolve_ShadowLodIndex(cascadeIndex);
    if (!_lodModels[shadowLod])
    {
        shadowLod = 0u;
    }

    const uint32 numMeshes = static_cast<uint32>(_lodModels[shadowLod]->Get_NumMeshes());
    if (numMeshes == 0)
    {
        return S_OK;
    }

    bool useShadowIndirect =
        !usePotatoBagArenaInstances &&
        !isBaking &&
        _shadowCullShader &&
        _shadowVisibleVB &&
        _shadowIndirectArgsBuffers[shadowLod].size() >= numMeshes;
    if (useShadowIndirect)
    {
        DispatchShadowCull(cascadeIndex, shadowLod);
    }

    Matrix identity = Matrix::Identity;
    if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity)))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_ShadowCB(_shader)))
    {
        return E_FAIL;
    }

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(VtxInstanceMeshExactPassIndex::ShadowPass)))
        {
            return E_FAIL;
        }

        if (useShadowIndirect)
        {
            if (FAILED(_lodModels[shadowLod]->RenderInstancedIndirect(i, _shadowVisibleVB.Get(), sizeof(Matrix), _shadowIndirectArgsBuffers[shadowLod][i].Get())))
            {
                return E_FAIL;
            }
        }
        else
        {
            if (FAILED(_lodModels[shadowLod]->RenderInstanced(i, instanceVB, sizeof(Matrix), renderInstanceCount)))
            {
                return E_FAIL;
            }
        }
    }


    //Matrix identity = Matrix::Identity;
    //if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity)))
    //{
    //    return E_FAIL;
    //}

    //if (FAILED(GAME->Bind_ShadowCB(_shader)))
    //{
    //    return E_FAIL;
    //}

    //const uint32 numMeshes = static_cast<uint32>(_lodModels[0]->Get_NumMeshes());

    //for (uint32 i = 0; i < numMeshes; ++i)
    //{
    //    if (FAILED(_shader->Begin(1)))
    //        return E_FAIL;
    //    if (FAILED(_lodModels[0]->RenderInstanced(i, _instanceVB.Get(), sizeof(Matrix), _instanceCount)))
    //        return E_FAIL;
    //}

    return S_OK;
}

bool MapInstancedMesh::Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const
{
    if (_instanceCount == 0)
        return false;

    outWorldBounds = _groupBounds;
    return true;
}

bool MapInstancedMesh::Allows_DynamicShadowCascade(uint32 cascadeIndex) const
{
    if (Is_GrassOrPetalModelPath(_modelPath) && cascadeIndex > 1u)
    {
        return false;
    }

    if (Is_SmallInstancedShadowModelPath(_modelPath) && cascadeIndex > 2u)
    {
        return false;
    }

    return cascadeIndex <= _maxShadowCascade;
}

uint32 MapInstancedMesh::Resolve_ShadowLodIndex(uint32 cascadeIndex) const
{
    if (!ClientInstance::Is_DebugUseMapInstancedMeshShadowLOD())
    {
        return 0u;
    }

    uint32 desiredLod = 0u;
    if (cascadeIndex == 1u || cascadeIndex == 2u)
    {
        desiredLod = 1u;
    }
    else if (cascadeIndex >= 3u)
    {
        desiredLod = 2u;
    }

    while (desiredLod > 0u && !_lodModels[desiredLod])
    {
        --desiredLod;
    }

    return desiredLod;
}

void MapInstancedMesh::DispatchCull()
{
    if (!_cullShader || !_cullCB || !_visibleUAV[0] || !_instanceSrcBuffer || !_sphereBuffer || _indirectArgsBuffers[0].empty())
    {
        return;
    }

    const Matrix* view = GAME->Get_Transform(D3DTS::View);
    const Matrix* proj = GAME->Get_Transform(D3DTS::Proj);
    if (!view || !proj)
    {
        return;
    }

    Matrix vp = (*view) * (*proj);
    auto planes = ExtractFrustumPlanes(vp);
     
    CullParams params{};
    for (int i = 0; i < 6; ++i)
    {
        params.planes[i] = planes[i];
    }
    params.totalCount = _instanceCount;
    params.maxDist = Resolve_CullDistance(_modelPath, _cullDist);
    params.lod1Dist = _lodModels[1] ? _lod1Dist : params.maxDist;
    params.lod2Dist = _lodModels[2] ? _lod2Dist : params.maxDist;

    const Vec4* cam4 = GAME->Get_CamPosition();
    params.camPos = Vec3(cam4->x, cam4->y, cam4->z);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_cullCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }
    memcpy(mapped.pData, &params, sizeof(CullParams));
    _context->Unmap(_cullCB.Get(), 0);

    uint32 initCounts[3] = { 0, 0, 0 };
    ID3D11UnorderedAccessView* uavs[3] = {
        _visibleUAV[0].Get(),
        _visibleUAV[1].Get(),
        _visibleUAV[2].Get()
    };
    _context->CSSetUnorderedAccessViews(0, 3, uavs, initCounts);

    _cullShader->Bind_SRV(0, _instanceSrcBuffer->Get_SRV());
    _cullShader->Bind_SRV(1, _sphereBuffer->Get_SRV());
    _cullShader->Bind_ConstantBuffer(0, _cullCB.Get());

    const uint32 groups = (_instanceCount + 63) / 64;
    _cullShader->Dispatch(groups, 1, 1);

    ID3D11UnorderedAccessView* nullUAVs[3] = {};
    _context->CSSetUnorderedAccessViews(0, 3, nullUAVs, nullptr);

    for (int lod = 0; lod < 3; ++lod)
    {
        if (!_lodModels[lod])
        {
            continue;
        }

        _context->CopyResource(_visibleVB[lod].Get(), _visibleStructured[lod].Get());
        for (auto& buffer : _indirectArgsBuffers[lod])
        {
            _context->CopyStructureCount(buffer.Get(), sizeof(uint32), _visibleUAV[lod].Get());
        }
    }

    const uint32 readbackRequestId =
        ClientInstance::Get_DebugMapInstancedVisibleCountReadbackRequestId();
    if (readbackRequestId != 0 &&
        readbackRequestId != _debugLastVisibleCountReadbackRequestId)
    {
        _debugHasDrawInstanceCounts = Readback_DebugVisibleCounts(readbackRequestId);
        _debugLastVisibleCountReadbackRequestId = readbackRequestId;
    }
}

bool MapInstancedMesh::Readback_DebugVisibleCounts(uint32 requestId)
{
    UNREFERENCED_PARAMETER(requestId);

    _debugLastDrawInstanceCounts = {};
    bool hasAnyCount = false;

    for (uint32 lod = 0; lod < 3; ++lod)
    {
        if (!_lodModels[lod] || _indirectArgsBuffers[lod].empty() || !_indirectArgsBuffers[lod][0])
            continue;

        if (!_debugVisibleCountReadbackBuffers[lod])
        {
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = 5 * sizeof(uint32);
            bd.Usage = D3D11_USAGE_STAGING;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            if (FAILED(_device->CreateBuffer(&bd, nullptr, _debugVisibleCountReadbackBuffers[lod].GetAddressOf())))
                continue;
        }

        _context->CopyResource(
            _debugVisibleCountReadbackBuffers[lod].Get(),
            _indirectArgsBuffers[lod][0].Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(_context->Map(_debugVisibleCountReadbackBuffers[lod].Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            continue;

        const uint32* args = static_cast<const uint32*>(mapped.pData);
        _debugLastDrawInstanceCounts[lod] = min(args[1], _instanceCount);
        hasAnyCount = true;

        _context->Unmap(_debugVisibleCountReadbackBuffers[lod].Get(), 0);
    }

    return hasAnyCount;
}

HRESULT MapInstancedMesh::CreateShadowIndirectArgsBuffers()
{
    ResetShadowIndirectArgsBuffers();

    for (int lod = 0; lod < 3; ++lod)
    {
        if (!_lodModels[lod])
        {
            continue;
        }

        const uint32 numMeshes = static_cast<uint32>(_lodModels[lod]->Get_NumMeshes());
        _shadowIndirectArgsBuffers[lod].resize(numMeshes);

        for (uint32 i = 0; i < numMeshes; ++i)
        {
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = 5 * sizeof(uint32);
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

            if (FAILED(_device->CreateBuffer(&bd, nullptr, _shadowIndirectArgsBuffers[lod][i].GetAddressOf())))
            {
                return E_FAIL;
            }

            uint32 args[5] = { 0, 0, 0, 0, 0 };
            args[0] = static_cast<uint32>(_lodModels[lod]->Get_NumIndices(i));
            _context->UpdateSubresource(_shadowIndirectArgsBuffers[lod][i].Get(), 0, nullptr, args, 0, 0);
        }
    }

    return S_OK;
}

void MapInstancedMesh::ResetShadowIndirectArgsBuffers()
{
    for (int lod = 0; lod < 3; ++lod)
    {
        for (auto& buffer : _shadowIndirectArgsBuffers[lod])
        {
            buffer.Reset();
        }
        _shadowIndirectArgsBuffers[lod].clear();
    }
}

void MapInstancedMesh::DispatchShadowCull(uint32 cascadeIndex, uint32 shadowLod)
{
    if (!_shadowCullShader || !_shadowCullCB || !_shadowVisibleUAV ||
        !_instanceSrcBuffer || !_sphereBuffer || _shadowIndirectArgsBuffers[shadowLod].empty())
    {
        return;
    }

    const ShadowCascadeInfluenceVolume* volumes = GAME->Get_CascadeInfluenceVolumes();
    if (!volumes)
    {
        return;
    }

    const ShadowCascadeInfluenceVolume& vol = volumes[cascadeIndex];

    ShadowCullParams params{};
    params.planeCount = std::min<uint32>(vol.planeCount, 32u);
    params.totalCount = _instanceCount;
    for (uint32 i = 0; i < params.planeCount; ++i)
    {
        params.planes[i] = vol.planes[i];
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_shadowCullCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }
    memcpy(mapped.pData, &params, sizeof(ShadowCullParams));
    _context->Unmap(_shadowCullCB.Get(), 0);

    uint32 initCount = 0;
    ID3D11UnorderedAccessView* uav = _shadowVisibleUAV.Get();
    _context->CSSetUnorderedAccessViews(0, 1, &uav, &initCount);

    _shadowCullShader->Bind_SRV(0, _instanceSrcBuffer->Get_SRV());
    _shadowCullShader->Bind_SRV(1, _sphereBuffer->Get_SRV());
    _shadowCullShader->Bind_ConstantBuffer(1, _shadowCullCB.Get());

    const uint32 groups = (_instanceCount + 63) / 64;
    _shadowCullShader->Dispatch(groups, 1, 1);

    ID3D11UnorderedAccessView* nullUAV = nullptr;
    _context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    _context->CopyResource(_shadowVisibleVB.Get(), _shadowVisibleStructured.Get());
    for (auto& buf : _shadowIndirectArgsBuffers[shadowLod])
    {
        _context->CopyStructureCount(buf.Get(), sizeof(uint32), _shadowVisibleUAV.Get());
    }
}

array<Vec4, 6> MapInstancedMesh::ExtractFrustumPlanes(const Matrix& vp)
{
    array<Vec4, 6> p;

    p[0] = Vec4{ vp.m[0][0] + vp.m[0][3], vp.m[1][0] + vp.m[1][3], vp.m[2][0] + vp.m[2][3], vp.m[3][0] + vp.m[3][3] };
    p[1] = Vec4{ vp.m[0][3] - vp.m[0][0], vp.m[1][3] - vp.m[1][0], vp.m[2][3] - vp.m[2][0], vp.m[3][3] - vp.m[3][0] };
    p[2] = Vec4{ vp.m[0][1] + vp.m[0][3], vp.m[1][1] + vp.m[1][3], vp.m[2][1] + vp.m[2][3], vp.m[3][1] + vp.m[3][3] };
    p[3] = Vec4{ vp.m[0][3] - vp.m[0][1], vp.m[1][3] - vp.m[1][1], vp.m[2][3] - vp.m[2][1], vp.m[3][3] - vp.m[3][1] };
    p[4] = Vec4{ vp.m[0][2], vp.m[1][2], vp.m[2][2], vp.m[3][2] };
    p[5] = Vec4{ vp.m[0][3] - vp.m[0][2], vp.m[1][3] - vp.m[1][2], vp.m[2][3] - vp.m[2][2], vp.m[3][3] - vp.m[3][2] };

    for (auto& plane : p)
    {
        float len = sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        if (len > 1e-6f)
        {
            plane.x /= len;
            plane.y /= len;
            plane.z /= len;
            plane.w /= len;
        }
    }

    return p;
}

void MapInstancedMesh::Refresh_HasGlow()
{
    _hasGlow = false;
    if (!_lodModels[0])
    {
        return;
    }

    const uint32 numMeshes = static_cast<uint32>(_lodModels[0]->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const Shared<Material> mat = _lodModels[0]->Get_Material(i);
        if (!mat || mat->Get_PassIndex() == VtxInstanceMeshExactPassIndex::UnlitFlamePass)
        {
            continue;
        }
        if (mat->Get_PassIndex() == VtxInstanceMeshExactPassIndex::EmissiveGlowPass)
        {
            _hasGlow = true;
            return;
        }
    }
}

Shared<MapInstancedMesh> MapInstancedMesh::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MapInstancedMesh>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MapInstancedMesh");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> MapInstancedMesh::Clone(void* arg)
{
    auto clone = make_shared<MapInstancedMesh>(*this);
    if (FAILED(clone->Initialize(arg)))
    {
        return nullptr;
    }
    return clone;
}

void MapInstancedMesh::Free()
{
    for (const PhysXStaticActorHandle& handle : _physxStaticHandles)
    {
        if (handle.Is_Valid())
        {
            GAME->Unregister_PhysXStaticActor(handle);
        }
    }
    _physxStaticHandles.clear();
    _collisionData.reset();

    for (int lod = 0; lod < 3; ++lod)
    {
        _visibleStructured[lod].Reset();
        _visibleUAV[lod].Reset();
        _visibleVB[lod].Reset();
        for (auto& buffer : _indirectArgsBuffers[lod])
        {
            buffer.Reset();
        }
        _indirectArgsBuffers[lod].clear();
        _debugVisibleCountReadbackBuffers[lod].Reset();
    }
    _cullCB.Reset();

    _shadowVisibleStructured.Reset();
    _shadowVisibleUAV.Reset();
    _shadowVisibleVB.Reset();
    _shadowCullCB.Reset();
    ResetShadowIndirectArgsBuffers();
    _shadowCullShader.reset();

    _instanceSrcBuffer.reset();
    _sphereBuffer.reset();
    _cullShader.reset();

    __super::Free();
}

json MapInstancedMesh::To_Json() const
{
    json root = GameObject::To_Json();

    root["model_path"] = String::ToString(_modelPath);
    root["Visible"] = _visible;
    root["CullEnabled"] = _cullEnabled;
    root["CastShadow"] = _castShadow;
    root["CullDist"] = _cullDist;

    json transformsArray = json::array();
    for (const auto& t : _transforms)
    {
        json transformNode = json::object();
        transformNode["pos"] = { t._position.x, t._position.y, t._position.z };
        transformNode["rot"] = { t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w };
        transformNode["scale"] = { t._scale.x, t._scale.y, t._scale.z };
        transformsArray.push_back(transformNode);
    }
    root["instance_transforms"] = transformsArray;

    return root;
}

void MapInstancedMesh::From_Json(const json& node)
{
    GameObject::From_Json(node);

    _visible = node.value("Visible", true);
    _cullEnabled = node.value("CullEnabled", true);
    _castShadow = node.value("CastShadow", true);
    _cullDist = node.value("CullDist", 125.f);

    if (node.contains("model_path"))
    {
        string pathA = node["model_path"].get<string>();
        _modelPath = String::ToWString(pathA);
        Ready_Components(_modelPath, nullptr);
        Refresh_HasGlow();
    }

    if (node.contains("instance_transforms"))
    {
        _transforms.clear();
        vector<Matrix> matrices;
        for (const auto& tNode : node["instance_transforms"])
        {
            InstanceTransform t{};
            t._position = Vec3(tNode["pos"][0], tNode["pos"][1], tNode["pos"][2]);
            t._rotation = Vec4(tNode["rot"][0], tNode["rot"][1], tNode["rot"][2], tNode["rot"][3]);
            t._scale = Vec3(tNode["scale"][0], tNode["scale"][1], tNode["scale"][2]);
            _transforms.push_back(t);

            Matrix world = Matrix::CreateScale(t._scale)
                * Matrix::CreateFromQuaternion(Quat(t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w))
                * Matrix::CreateTranslation(t._position);
            matrices.push_back(world);
        }

        CreateInstanceBuffer(matrices);
        CreateCullBuffers(matrices, _transforms);
    }
}
