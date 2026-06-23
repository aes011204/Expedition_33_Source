#include "MEStaticMeshActor.h"
#include "GameInstance.h"
#include "Material_Types.h"
#include "ShaderPassFinder.h"

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
    Refresh_HasEclipseGlow();
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (_hasEclipseGlow)
    {
        GAME->Add_RenderGroup(RenderGroup::Glow, GetSharedPtr<GameObject>());
    }
}

HRESULT MEStaticMeshActor::Render()
{
    if (!_model || !_shader)
    {
        return S_OK;
    }

    if (FAILED(Bind_ShaderResources()))
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
        const Shared<Material> material = _model->Get_Material(_model->Get_MeshMaterialIndex(i));
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
            if (passIndex == static_cast<int32>(VtxMeshPassIndex::EclipseSpherePass))
            {
                passIndex = static_cast<int32>(VtxMeshPassIndex::DefaultPass);
            }
        }
        if (FAILED(_shader->Begin(passIndex)))
        {
            return E_FAIL;
        }

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

    Refresh_HasEclipseGlow();
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

    if (FAILED(GAME->Bind_CamPosition(_shader.get(), "g_CamPosition")))
        return E_FAIL;

    return S_OK;
}

void MEStaticMeshActor::Refresh_HasEclipseGlow()
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
