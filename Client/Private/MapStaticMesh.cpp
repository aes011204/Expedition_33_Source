#include "MapStaticMesh.h"
#include "GameInstance.h"
#include "Collision_Define.h"

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

    if (FAILED(Ready_Components(pDesc->modelPath, pDesc->model)))
    {
        return E_FAIL;
    }

    _transformCom->Set_Position(pDesc->position);
    _transformCom->Set_RotationQuaternion(pDesc->rotation);
    _transformCom->Set_Scale(pDesc->scale);

    // MapStaticMesh는 움직이지않는 맵 collision으로 취급이라 초기에 한번만 등록
    if (_model)
    {
        _physxStaticHandle = GAME->Register_PhysXStaticModel(
            *_model, _transformCom->Get_WorldMatrix(), Collision_Preset::Enviroment);
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
    Vec3 localMin{}, localMax{};
    if (_model->Try_Get_LocalBounds(localMin, localMax))
    {
        BoundingBox localBox;
        BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));

        BoundingBox worldBox{};
        localBox.Transform(worldBox, _model->Get_PreLocalTransformMatrix() * _transformCom->Get_WorldMatrix());

//#ifdef _DEBUG
//        DebugBoxDesc dbgDesc{};
//        dbgDesc.center = worldBox.Center;
//        dbgDesc.extents = worldBox.Extents;
//        dbgDesc.style.color = Colors::Black;
//        GAME->Draw_DebugBox(dbgDesc);
//#endif

        if (!GAME->IsVisible(worldBox))
        {
            return;
        }
    }

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

        const uint32 passIndex = static_cast<uint32>(material->Get_PassIndex());
        if (FAILED(_shader->Begin(passIndex)))
        {
            return E_FAIL;
        }

        _model->Render(i);
    }

    return S_OK;
}

HRESULT MapStaticMesh::Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader)))
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
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.001f));
        if (!_model)
            return E_FAIL;
    }

    return S_OK;
}

HRESULT MapStaticMesh::Bind_ShaderResources()
{
    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

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
        return nullptr;
    }

    return clone;
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
