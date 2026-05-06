#include "MapStaticMesh.h"
#include "GameInstance.h"

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

    if (FAILED(Ready_Components(pDesc->modelPath)))
    {
        return E_FAIL;
    }

    _transformCom->Set_Position(pDesc->position);
    _transformCom->Set_RotationQuaternion(pDesc->rotation);
    _transformCom->Set_Scale(pDesc->scale);

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
        const uint32 pass = SUCCEEDED(_model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, 1, 0)) ? 0 : 2;

        if (FAILED(_shader->Begin(pass)))
        {
            return E_FAIL;
        }

        _model->Render(i);
    }

    return S_OK;
}

HRESULT MapStaticMesh::Ready_Components(const wstring& modelPath)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxMesh"), _shader)))
    {
        return E_FAIL;
    }

    string pathA = String::ToString(modelPath);
    _model = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::Identity);
    if (!_model)
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MapStaticMesh::Bind_ShaderResources()
{
    if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_CameraFar(_shader, "g_fFar")))
    {
        return E_FAIL;
    }

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
        MSG_BOX("Failed to Cloned : MapStaticMesh");
        return nullptr;
    }

    return clone;
}

void MapStaticMesh::Free()
{
    __super::Free();
}
