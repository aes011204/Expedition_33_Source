#include "InstancedMeshActor.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(InstancedMeshActor)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "InstancedMeshActor";
    info.category = "World";
    return true;
}

InstancedMeshActor::InstancedMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

InstancedMeshActor::InstancedMeshActor(const InstancedMeshActor& rhs)
    : GameObject{ rhs }
{
}

HRESULT InstancedMeshActor::Initialize_Prototype()
{
    return S_OK;
}

HRESULT InstancedMeshActor::Initialize(void* pArg)
{
    CHECK_FAILED(__super::Initialize(pArg), E_FAIL);

    auto* desc = static_cast<INSTANCED_MESH_DESC*>(pArg);
    if (!desc)
    {
        return E_FAIL;
    }

    CHECK_FAILED(Ready_Components(*desc), E_FAIL);

    return S_OK;
}

void InstancedMeshActor::Priority_Update(float fTimeDelta)
{
}

void InstancedMeshActor::Update(float fTimeDelta)
{
}

void InstancedMeshActor::Late_Update(float fTimeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT InstancedMeshActor::Render()
{
    if (_instanceCount == 0)
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
        if (FAILED(_model->Bind_Material(_shader.get(), "g_Texture", i, MaterialTextureSlot::BaseColor)))
        {
            continue;
        }

        if (FAILED(_shader->Begin(0)))
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

HRESULT InstancedMeshActor::Ready_Components(const INSTANCED_MESH_DESC& desc)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), L"Shader_VtxMeshInstance", _shader)))
    {
        return E_FAIL;
    }

    _model = ModelCom::Create(_device, _context, ModelType::NonAnim,
        String::ToString(desc.modelPath).c_str(), Matrix::CreateScale(0.01f));

    if (!_model)
    {
        return E_FAIL;
    }

    if (desc.instances && desc.instanceCount > 0)
    {
        if (FAILED(CreateInstanceBuffer(desc.instances, desc.instanceCount)))
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT InstancedMeshActor::Bind_ShaderResources()
{
    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT InstancedMeshActor::CreateInstanceBuffer(const MESHINSTANCEDATA* instances, uint32 count)
{
    _instanceCapacity = count;
    _instanceCount = count;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(MESHINSTANCEDATA) * count;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = instances;

    _instanceVB.Reset();
    if (FAILED(_device->CreateBuffer(&bd, &sd, _instanceVB.GetAddressOf())))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT InstancedMeshActor::UpdateInstances(const MESHINSTANCEDATA* instances, uint32 count)
{
    if (count == 0)
    {
        _instanceCount = 0;
        return S_OK;
    }

    if (count > _instanceCapacity)
    {
        return CreateInstanceBuffer(instances, count);
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_instanceVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return E_FAIL;
    }

    memcpy(mapped.pData, instances, sizeof(MESHINSTANCEDATA) * count);
    _context->Unmap(_instanceVB.Get(), 0);

    _instanceCount = count;
    return S_OK;
}

Shared<InstancedMeshActor> InstancedMeshActor::Create(const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto pInstance = make_shared<InstancedMeshActor>(device, context);
    if (FAILED(pInstance->Initialize_Prototype()))
    {
        return nullptr;
    }
    return pInstance;
}

Shared<GameObject> InstancedMeshActor::Clone(void* pArg)
{
    auto pInstance = make_shared<InstancedMeshActor>(*this);
    if (FAILED(pInstance->Initialize(pArg)))
    {
        return nullptr;
    }
    return pInstance;
}

void InstancedMeshActor::Free()
{
    __super::Free();
}
