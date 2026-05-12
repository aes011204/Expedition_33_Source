#include "MEInstancedMeshActor.h"
#include "GameInstance.h"
#include "Helper_Math.h"


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

    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        bool hasTexture = SUCCEEDED(_model->Bind_Material(_shader.get(), "g_Texture", i, MaterialTextureSlot::BaseColor));

        if (FAILED(_shader->Begin(hasTexture ? 0 : 1)))
        {
            return E_FAIL;
        }

        if (FAILED(_model->RenderInstanced(i, _instanceVB.Get(), sizeof(MESHINSTANCEDATA), _instanceCount)))
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT MEInstancedMeshActor::Bind_ShaderResources()
{
    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_fFar", &_cameraFar, sizeof(float))))
        return E_FAIL;

    Matrix preTransform = _model->Get_PreLocalTransformMatrix();
    if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &preTransform)))
        return E_FAIL;

    return S_OK;
}

HRESULT MEInstancedMeshActor::CreateInstanceBuffer(const vector<MESHINSTANCEDATA>& instances)
{
    const uint32 count = static_cast<uint32>(instances.size());

    if (count == 0)
    {
        _instanceCount = 0;
        return S_OK;
    }

    if (count > _instanceCapacity)
    {
        _instanceCapacity = count;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(MESHINSTANCEDATA) * _instanceCapacity;
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

    memcpy(mapped.pData, instances.data(), sizeof(MESHINSTANCEDATA) * count);
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

HRESULT MEInstancedMeshActor::UpdateInstances(const wstring& modelPath, const Vec3& centerPos, float areaWidth, float areaDepth, float density, uint32 seed)
{
    if (_loadedModelPath != modelPath)
    {
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim,
            String::ToString(modelPath).c_str(), Matrix::CreateScale(0.01f));

        if (!_model)
        {
            return E_FAIL;
        }
        _loadedModelPath = modelPath;
    }

    vector<MESHINSTANCEDATA> instances;
    GenerateInstances(centerPos, areaWidth, areaDepth, density, seed, instances);

    return CreateInstanceBuffer(instances);
}

HRESULT MEInstancedMeshActor::Ready_Components()
{
    if (FAILED(Add_Component(0, TEXT("Shader_VtxMeshInstance"), _shader)))
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
    GameObject::Free();
}
