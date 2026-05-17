#include "MEInstancedMeshActor.h"
#include "GameInstance.h"
#include "Helper_Math.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "InstanceCullParams.h"


MEInstancedMeshActor::MEInstancedMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MEInstancedMeshActor::MEInstancedMeshActor(const MEInstancedMeshActor& prototype)
    : GameObject{ prototype }
{
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

    _cullShader = ComputeShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_InstanceCull.hlsl",
        "CS_Main");

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
    if (_instanceCount == 0 || !_model || !_shader)
    {
        return S_OK;
    }

    Buffer* renderVB = _instanceVB.Get();
    uint32 renderCount = _instanceCount;

    if (_cullShader && _instanceSrcBuffer && _sphereBuffer && _visibleVB)
    {
        DispatchCull();
        renderVB = _visibleVB.Get();
        renderCount = _visibleCount;
    }

    if (renderCount == 0)
    {
        return S_OK;
    }

    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const Shared<Material> material = _model->Get_Material(i);
        int32 passIndex = 2; // NoTexture fallback
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

        if (FAILED(_model->RenderInstanced(i, renderVB, sizeof(Matrix), renderCount)))
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
    if (count == 0 || _cullShader)
    {
        return S_OK;
    }

    _instanceSrcBuffer = ComputeStructuredBuffer::Create(_device, _context, sizeof(Matrix), count, matrices.data());
    if (!_instanceSrcBuffer)
    {
        return E_FAIL;
    }

    Vec3 localMin{}, localMax{};
    const bool hasBounds = _model && _model->Try_Get_LocalBounds(localMin, localMax);
    const Matrix preTransform = _model ? _model->Get_PreLocalTransformMatrix() : Matrix::Identity;

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
    if (_sphereBuffer)
    {
        return E_FAIL;
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(Matrix) * count;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = sizeof(Matrix);

        _visibleVB.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _visibleVB.GetAddressOf())))
        {
            return E_FAIL;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format = DXGI_FORMAT_UNKNOWN;
        uavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavd.Buffer.NumElements = count;
        uavd.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

        _visibleUAV.Reset();
        if (FAILED(_device->CreateUnorderedAccessView(_visibleVB.Get(), &uavd, _visibleUAV.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(CullParams);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        _cullCB.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _cullCB.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(uint32);
        bd.Usage = D3D11_USAGE_STAGING;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        _countStaging.Reset();
        if (FAILED(_device->CreateBuffer(&bd, nullptr, _countStaging.GetAddressOf())))
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

void MEInstancedMeshActor::DispatchCull()
{
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

    _context->CopyStructureCount(_countStaging.Get(), 0, _visibleUAV.Get());

    D3D11_MAPPED_SUBRESOURCE countMapped{};
    if (SUCCEEDED(_context->Map(_countStaging.Get(), 0, D3D11_MAP_READ, 0, &countMapped)))
    {
        _visibleCount = *static_cast<uint32*>(countMapped.pData);
        _context->Unmap(_countStaging.Get(), 0);
    }
    else
    {
        _visibleCount = 0;
    }
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

HRESULT MEInstancedMeshActor::UpdateInstances(const wstring& modelPath, const vector<InstanceTransform>& transforms)
{
    if (_loadedModelPath != modelPath)
    {
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim, String::ToString(modelPath).c_str(), Matrix::CreateScale(0.01f));
        if (!_model)
        {
            return E_FAIL;
        }
        _loadedModelPath = modelPath;
    }

    vector<Matrix> matrices;
    matrices.reserve(transforms.size());

    for (auto& t : transforms)
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
    _visibleUAV.Reset();
    _visibleVB.Reset();
    _cullCB.Reset();
    _countStaging.Reset();
    _instanceSrcBuffer.reset();
    _sphereBuffer.reset();
    _cullShader.reset();

    GameObject::Free();
}
