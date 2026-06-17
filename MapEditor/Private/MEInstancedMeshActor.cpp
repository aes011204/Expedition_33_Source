#include "MEInstancedMeshActor.h"
#include "GameInstance.h"
#include "Helper_Math.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "InstanceCullParams.h"
#include "MEModelManager.h"
#include "ShaderPassFinder.h"

MEInstancedMeshActor::MEInstancedMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MEInstancedMeshActor::MEInstancedMeshActor(const MEInstancedMeshActor& prototype)
    : GameObject{ prototype }
{
}

void MEInstancedMeshActor::Set_LodModel(int lod, Shared<ModelCom> model)
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

HRESULT MEInstancedMeshActor::Initialize(void* arg)
{

    if (FAILED(__super::Initialize(arg)))
    {
        return E_FAIL;
    }

    if (FAILED(Ready_Components()))
    {
        return E_FAIL;
    }

    return S_OK;
}

void MEInstancedMeshActor::Late_Update(float timeDelta)
{
    if (_instanceCount == 0)
    {
        return;
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MEInstancedMeshActor::Render()
{
    if (_instanceCount == 0 || !_lodModels[0] || !_shader)
    {
        return S_OK;
    }

    bool useIndirect = false;
    if (_cullShader && _instanceSrcBuffer && _sphereBuffer && _visibleVB[0] && !_indirectArgsBuffers[0].empty())
    {
        DispatchCull();
        useIndirect = true;
    }

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
            const uint32 materialIndex = _lodModels[lod]->Get_MeshMaterialIndex(i);
            const Shared<Material> material = _lodModels[lod]->Get_Material(materialIndex);
            int32 passIndex = static_cast<int32>(VtxMeshPassIndex::NoTexturePass);
            if (material != nullptr)
            {
                if (FAILED(_shader->Bind_MaterialTextures(*material)))
                {
                    continue;
                }

                if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
                {
                    return E_FAIL;
                }
                passIndex = static_cast<int32>(material->Get_PassIndex());
            }

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
                if (FAILED(_lodModels[lod]->RenderInstanced(i, _instanceVB.Get(), sizeof(Matrix), _instanceCount)))
                {
                    return E_FAIL;
                }
            }
        }
    }

    return S_OK;
}

HRESULT MEInstancedMeshActor::RenderPickingID(uint32 id)
{
    if (_instanceCount == 0 || !_lodModels[0] || !_shader)
    {
        return S_OK;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
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
        if (FAILED(_shader->Begin(VtxInstanceMeshExactPassIndex::IDInstancePass)))
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


HRESULT MEInstancedMeshActor::Bind_ShaderResources()
{
    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
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

HRESULT MEInstancedMeshActor::CreateCullBuffers(const vector<Matrix>& matrices, const vector<InstanceTransform>& transforms)
{
    const uint32 count = static_cast<uint32>(matrices.size());
    if (count == 0 || !_cullShader)
    {
        return S_OK;
    }

    _instanceSrcBuffer = ComputeStructuredBuffer::Create(_device, _context, sizeof(Matrix), count, matrices.data());
    if (!_instanceSrcBuffer)
    {
        return E_FAIL;
    }

    Vec3 localMin{}, localMax{};
    const bool hasBounds = _lodModels[0] && _lodModels[0]->Try_Get_LocalBounds(localMin, localMax);
    const Matrix preTransform = _lodModels[0] ? _lodModels[0]->Get_PreLocalTransformMatrix() : Matrix::Identity;

    vector<Vec4> spheres;
    spheres.reserve(count);

    for (uint32 i = 0; i < count; ++i)
    {
        if (hasBounds)
        {
            BoundingBox localBox;
            BoundingBox::CreateFromPoints(localBox,
                XMVectorSet(localMin.x, localMin.y, localMin.z, 0.f),
                XMVectorSet(localMax.x, localMax.y, localMax.z, 0.f));

            BoundingBox worldBox;
            localBox.Transform(worldBox, preTransform * matrices[i]);

            BoundingSphere sphere;
            BoundingSphere::CreateFromBoundingBox(sphere, worldBox);

            spheres.push_back({ sphere.Center.x, sphere.Center.y, sphere.Center.z, sphere.Radius });
        }
        else
        {
            spheres.push_back({ matrices[i].m[3][0], matrices[i].m[3][1], matrices[i].m[3][2], 50.f });
        }
    }

    _sphereBuffer = ComputeStructuredBuffer::Create(_device, _context, sizeof(Vec4), count, spheres.data());
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

    {
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
    }

    return S_OK;
}

void MEInstancedMeshActor::DispatchCull()
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

    params.maxDist = 300.f;
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
        _context->CopyResource(_visibleVB[lod].Get(), _visibleStructured[lod].Get());
        for (auto& buffer : _indirectArgsBuffers[lod])
        {
            _context->CopyStructureCount(buffer.Get(), sizeof(uint32), _visibleUAV[lod].Get());
        }
    }

    ID3D11ShaderResourceView* nullSRVs[3] = {};
    _context->CSSetShaderResources(0, 3, nullSRVs);
}

array<Vec4, 6> MEInstancedMeshActor::ExtractFrustumPlanes(const Matrix& vp)
{
    array<Vec4, 6> p;

    // Left
    p[0] = { vp.m[0][0] + vp.m[0][3], vp.m[1][0] + vp.m[1][3], vp.m[2][0] + vp.m[2][3], vp.m[3][0] + vp.m[3][3] };
    // Right
    p[1] = { vp.m[0][3] - vp.m[0][0], vp.m[1][3] - vp.m[1][0], vp.m[2][3] - vp.m[2][0], vp.m[3][3] - vp.m[3][0] };
    // Bottom
    p[2] = { vp.m[0][1] + vp.m[0][3], vp.m[1][1] + vp.m[1][3], vp.m[2][1] + vp.m[2][3], vp.m[3][1] + vp.m[3][3] };
    // Top
    p[3] = { vp.m[0][3] - vp.m[0][1], vp.m[1][3] - vp.m[1][1], vp.m[2][3] - vp.m[2][1], vp.m[3][3] - vp.m[3][1] };
    // Near
    p[4] = { vp.m[0][2], vp.m[1][2], vp.m[2][2], vp.m[3][2] };
    // Far
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

HRESULT MEInstancedMeshActor::CreateInstanceBuffer(const vector<Matrix>& matrices)
{
    const uint32 count = static_cast<uint32>(matrices.size());

    if (count == 0)
    {
        _instanceCount = 0;
        return S_OK;
    }

    if (count > _instanceCapacity)
    {
        _instanceCapacity = count;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Matrix) * _instanceCapacity;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        _instanceVB.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _instanceVB.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_instanceVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return E_FAIL;
    }

    memcpy(mapped.pData, matrices.data(), sizeof(Matrix) * count);
    _context->Unmap(_instanceVB.Get(), 0);

    _instanceCount = count;
    return S_OK;
}

void MEInstancedMeshActor::GenerateInstances(const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed, vector<MESHINSTANCEDATA>& outInstances)
{
    outInstances.clear();

    const uint32 count = static_cast<uint32>(areaWidth * areaDepth * density);
    outInstances.reserve(count);

    mt19937 rng(seed);

    for (uint32 i = 0; i < count; ++i)
    {
        MESHINSTANCEDATA inst{};
        inst.position.x = centerPos.x + Math::RandNum(-areaWidth * 0.5f, areaWidth * 0.5f, rng);
        inst.position.z = centerPos.z + Math::RandNum(-areaDepth * 0.5f, areaDepth * 0.5f, rng);
        inst.position.y = centerPos.y;
        inst.scale = Math::RandNum(0.8f, 1.2f, rng);
        inst.yRotation = Math::RandNum(0.f, XM_2PI, rng);
        inst.windPhase = Math::RandNum(0.f, XM_2PI, rng);
        inst.colorVariation = Math::RandNum(-1.f, 1.f, rng);
        outInstances.push_back(inst);
    }
}

HRESULT MEInstancedMeshActor::UpdateInstances(const wstring& modelPath, const vector<InstanceTransform>& transforms, MEModelManager* modelManager)
{
    if (_loadedModelPath != modelPath)
    {
        if (modelManager)
        {
            _lodModels[0] = modelManager->GetOrLoad(_device, _context, modelPath);
        }
        else
        {
            _lodModels[0] = ModelCom::Create(_device, _context, ModelType::NonAnim, String::ToString(modelPath).c_str(), Matrix::CreateScale(0.01f));
        }

        if (!_lodModels[0])
        {
            return E_FAIL;
        }
        if (!_lodModels[0])
        {
            return E_FAIL;
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
            Shared<ModelCom> lodModel = modelManager
                ? modelManager->GetOrLoad(_device, _context, lodPath)
                : ModelCom::Create(_device, _context, ModelType::NonAnim, String::ToString(lodPath).c_str(), Matrix::CreateScale(0.01f));

            _lodModels[lod] = lodModel;
        }
        _loadedModelPath = modelPath;
    }

    vector<Matrix> matrices;
    matrices.reserve(transforms.size());

    for (auto& t : transforms)
    {
        Quat q(t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w);
        if (q.Length() < 1e-6f)
        {
            q = Quat::Identity;
        }
        else
        {
            q.Normalize();
        }

        Matrix world = Matrix::CreateScale(t._scale)
            * Matrix::CreateFromQuaternion(q)
            * Matrix::CreateTranslation(t._position);
        matrices.push_back(world);
    }

    if (FAILED(CreateInstanceBuffer(matrices)))
    {
        return E_FAIL;
    }

    CreateCullBuffers(matrices, transforms);
    return S_OK;
}

HRESULT MEInstancedMeshActor::Ready_Components()
{
    if (FAILED(Add_Component(0, TEXT("Shader_VtxMeshInstanceExact"), _shader)))
    {
        return E_FAIL;
    }

    return S_OK;
}

Shared<MEInstancedMeshActor> MEInstancedMeshActor::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    auto instance = make_shared<MEInstancedMeshActor>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MEInstancedMeshActor::Clone(void* arg)
{
    auto instance = make_shared<MEInstancedMeshActor>(*this);
    if (FAILED(instance->Initialize(arg)))
    {
        return nullptr;
    }

    return instance;
}

void MEInstancedMeshActor::Free()
{
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
    }
    _cullCB.Reset();
    _instanceSrcBuffer.reset();
    _sphereBuffer.reset();
    _cullShader.reset();

    GameObject::Free();
}
