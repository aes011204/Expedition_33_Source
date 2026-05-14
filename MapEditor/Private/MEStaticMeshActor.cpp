#include "MEStaticMeshActor.h"
#include "GameInstance.h"
#include "Material_Types.h"

MEStaticMeshActor::MEStaticMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

MEStaticMeshActor::MEStaticMeshActor(const MEStaticMeshActor& prototype)
    : GameObject(prototype)
    , _model(prototype._model)
{
}

HRESULT MEStaticMeshActor::Initialize(void* arg)
{
    if (FAILED(GameObject::Initialize(arg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void MEStaticMeshActor::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MEStaticMeshActor::Render()
{
    if (!_model || !_shader)
        return S_OK;

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const Shared<Material> material = _model->Get_Material(i);
        if (material != nullptr)
        {
            _shader->Bind_MaterialTextures(*material);
            _shader->Bind_CBufferData(material->Get_CB());
        }

        if (FAILED(_shader->Begin(2)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

HRESULT MEStaticMeshActor::RenderID(const Matrix& worldMatrix, uint32 id)
{
    if (!_model || !_shader)
        return S_OK;

    CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_iObjectID", &id, sizeof(uint32)), E_FAIL);

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        // for picking

        if (FAILED(_shader->Begin(6)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

void MEStaticMeshActor::Set_Model(Shared<ModelCom> model)
{
    if (_model)
    {
        _components.erase(TEXT("Com_Model"));
    }

    _model = model;
    if (_model)
    {
        _components.emplace(TEXT("Com_Model"), _model);
    }
}

HRESULT MEStaticMeshActor::Ready_Components()
{
    if (FAILED(Add_Component(0, TEXT("Shader_VtxMesh_Static"), _shader)))
        return E_FAIL;

    return S_OK;
}

HRESULT MEStaticMeshActor::Bind_ShaderResources()
{
    if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_Far", &_cameraFar, sizeof(float))))
        return E_FAIL;

    return S_OK;
}

Shared<MEStaticMeshActor> MEStaticMeshActor::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    auto instance = make_shared<MEStaticMeshActor>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MEStaticMeshActor::Clone(void* arg)
{
    auto instance = make_shared<MEStaticMeshActor>(*this);
    if (FAILED(instance->Initialize(arg)))
    {
        return nullptr;
    }

    return instance;
}

void MEStaticMeshActor::Free()
{
    GameObject::Free();
}
