#include "MapInstancedMesh.h"
#include "GameInstance.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "InstanceCullParams.h"
#include "Camera.h"
#include "StaticCollisionData.h"
#include "Collision_Define.h"

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

    if (arg != nullptr)
    {
        auto pDesc = static_cast<MAPINSTANCEDMESH_DESC*>(arg);

        _modelPath = pDesc->modelPath;
        _transforms = pDesc->transforms;
        _collisionData = pDesc->collisionData;

        if (FAILED(Ready_Components(pDesc->modelPath, pDesc->model)))
        {
            return E_FAIL;
        }

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
                    Collision_Preset::Enviroment);

                if (handle.Is_Valid())
                {
                    _physxStaticHandles.push_back(handle);
                }
            }
        }
    }

    Set_ShadowCasterType(ShadowCasterType::Static);

    return S_OK;
}

void MapInstancedMesh::Priority_Update(float timeDelta)
{
}

void MapInstancedMesh::Update(float timeDelta)
{
}

void MapInstancedMesh::Late_Update(float timeDelta)
{
    if (_instanceCount == 0)
    {
        return;
    }

    if (!GAME->IsVisible(_groupBounds))
    {
        return;
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT MapInstancedMesh::Render()
{
    if (_instanceCount == 0 || !_model || !_shader)
    {
        return S_OK;
    }

    Buffer* renderVB = _instanceVB.Get();
    bool useIndirect = false;

    if (_cullShader && _instanceSrcBuffer && _sphereBuffer && _visibleVB && !_indirectArgsBuffers.empty())
    {
        DispatchCull();
        renderVB = _visibleVB.Get();
        useIndirect = true;
    }

    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const Shared<Material> material = _model->Get_Material(i);
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
            if (FAILED(_model->RenderInstancedIndirect(i, renderVB, sizeof(Matrix), _indirectArgsBuffers[i].Get())))
            {
                return E_FAIL;
            }
        }
        else
        {
            if (FAILED(_model->RenderInstanced(i, renderVB, sizeof(Matrix), _instanceCount)))
            {
                return E_FAIL;
            }
        }
    }

    return S_OK;
}

HRESULT MapInstancedMesh::RenderPickingID(uint32 id)
{
    if (_instanceCount == 0 || !_model || !_shader)
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

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(5)))
        {
            return E_FAIL;
        }

        if (FAILED(_model->RenderInstanced(i, _instanceVB.Get(), sizeof(Matrix), _instanceCount)))
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
        _model = sharedModel;
    }
    else
    {
        string pathA = String::ToString(modelPath);
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.01f));
        if (!_model)
        {
            return E_FAIL;
        }
    }

    return S_OK;
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
    const bool hasBounds = _model && _model->Try_Get_LocalBounds(localMin, localMax);
    const Matrix preTransform = _model ? _model->Get_PreLocalTransformMatrix() : Matrix::Identity;

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

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Matrix) * count;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = sizeof(Matrix);

        _visibleStructured.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _visibleStructured.GetAddressOf())))
        {
            return E_FAIL;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format = DXGI_FORMAT_UNKNOWN;
        uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavd.Buffer.FirstElement = 0;
        uavd.Buffer.NumElements = count;
        uavd.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

        _visibleUAV.Reset();
        if (FAILED(_device->CreateUnorderedAccessView(_visibleStructured.Get(), &uavd, _visibleUAV.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Matrix) * count;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        _visibleVB.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _visibleVB.GetAddressOf())))
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

    {
        _indirectArgsBuffers.clear();
        const uint32 numMeshes = _model ? static_cast<uint32>(_model->Get_NumMeshes()) : 1;
        _indirectArgsBuffers.resize(numMeshes);

        for (uint32 i = 0; i < numMeshes; ++i)
        {
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = 5 * sizeof(uint32);
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

            if (FAILED(_device->CreateBuffer(&bd, nullptr, _indirectArgsBuffers[i].GetAddressOf())))
            {
                return E_FAIL;
            }

            uint32 args[5] = { 0, count, 0, 0, 0 };
            if (_model)
            {
                args[0] = static_cast<uint32>(_model->Get_NumIndices(i));
            }

            _context->UpdateSubresource(_indirectArgsBuffers[i].Get(), 0, nullptr, args, 0, 0);
        }
    }

    return S_OK;
}

HRESULT MapInstancedMesh::Render_Shadow()
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

void MapInstancedMesh::DispatchCull()
{
    if (!_cullShader || !_cullCB || !_visibleUAV || !_instanceSrcBuffer || !_sphereBuffer || _indirectArgsBuffers.empty())
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
    params.maxDist = INSTANCE_CULL_DIST;

    const Vec4* cam4 = GAME->Get_CamPosition();
    params.camPos = Vec3(cam4->x, cam4->y, cam4->z);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_cullCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }
    memcpy(mapped.pData, &params, sizeof(CullParams));
    _context->Unmap(_cullCB.Get(), 0);

    uint32 initCount = {};
    _context->CSSetUnorderedAccessViews(0, 1, _visibleUAV.GetAddressOf(), &initCount);

    _cullShader->Bind_SRV(0, _instanceSrcBuffer->Get_SRV());
    _cullShader->Bind_SRV(1, _sphereBuffer->Get_SRV());
    _cullShader->Bind_ConstantBuffer(0, _cullCB.Get());

    const uint32 groups = (_instanceCount + 63) / 64;
    _cullShader->Dispatch(groups, 1, 1);

    ID3D11UnorderedAccessView* nullUAV = {};
    _context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    _context->CopyResource(_visibleVB.Get(), _visibleStructured.Get());
    for (auto& buffer : _indirectArgsBuffers)
    {
        _context->CopyStructureCount(buffer.Get(), sizeof(uint32), _visibleUAV.Get());
    }
}

array<Vec4, 6> MapInstancedMesh::ExtractFrustumPlanes(const Matrix& vp)
{
    array<Vec4, 6> p;

    p[0] = { vp.m[0][0] + vp.m[0][3], vp.m[1][0] + vp.m[1][3], vp.m[2][0] + vp.m[2][3], vp.m[3][0] + vp.m[3][3] };
    p[1] = { vp.m[0][3] - vp.m[0][0], vp.m[1][3] - vp.m[1][0], vp.m[2][3] - vp.m[2][0], vp.m[3][3] - vp.m[3][0] };
    p[2] = { vp.m[0][1] + vp.m[0][3], vp.m[1][1] + vp.m[1][3], vp.m[2][1] + vp.m[2][3], vp.m[3][1] + vp.m[3][3] };
    p[3] = { vp.m[0][3] - vp.m[0][1], vp.m[1][3] - vp.m[1][1], vp.m[2][3] - vp.m[2][1], vp.m[3][3] - vp.m[3][1] };
    p[4] = { vp.m[0][2], vp.m[1][2], vp.m[2][2], vp.m[3][2] };
    p[5] = { vp.m[0][3] - vp.m[0][2], vp.m[1][3] - vp.m[1][2], vp.m[2][3] - vp.m[2][2], vp.m[3][3] - vp.m[3][2] };

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

    _visibleStructured.Reset();
    _visibleUAV.Reset();
    _visibleVB.Reset();
    _cullCB.Reset();
    for (auto& buffer : _indirectArgsBuffers)
    {
        buffer.Reset();
    }
    _indirectArgsBuffers.clear();
    _instanceSrcBuffer.reset();
    _sphereBuffer.reset();
    _cullShader.reset();

    __super::Free();
}

json MapInstancedMesh::To_Json() const
{
    json root = GameObject::To_Json();

    root["model_path"] = String::ToString(_modelPath);

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

    if (node.contains("model_path"))
    {
        string pathA = node["model_path"].get<string>();
        _modelPath = String::ToWString(pathA);
        Ready_Components(_modelPath, nullptr);
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
