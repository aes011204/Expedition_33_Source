#include "pch.h"
#include "TrailPreviewModelObject.h"

#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "ShaderCom.h"

NS_BEGIN(EffectEditor)

TrailPreviewModelObject::TrailPreviewModelObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

TrailPreviewModelObject::TrailPreviewModelObject(const TrailPreviewModelObject& prototype)
    : GameObject{ prototype }
{
}

HRESULT TrailPreviewModelObject::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _transformCom->Set_Position(Vec3(0.f, 0.f, 0.f));
    _transformCom->Set_RotationEuler(Vec3(0.f, 0.f, 0.f));
    _transformCom->Set_Scale(Vec3(1.f, 1.f, 1.f));
    Set_Name(L"Lanceram_TrailPreview");

    return S_OK;
}

void TrailPreviewModelObject::Late_Update(float)
{
    if (_model != nullptr && _shader != nullptr)
        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT TrailPreviewModelObject::Render()
{
    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_shader->Begin(static_cast<uint32>(material->Get_PassIndex())), E_FAIL);
        CHECK_FAILED(_model->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

wstring TrailPreviewModelObject::Resolve_ModelPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / kModelRelativePath)
           .lexically_normal()
           .wstring();
}

HRESULT TrailPreviewModelObject::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Shader_VtxMesh", _shader), E_FAIL);

    const wstring modelPath = Resolve_ModelPath();
    if (modelPath.empty())
        return E_FAIL;

    const string modelPathUtf8 = String::ToString(modelPath);
    _model = ModelCom::Create(_device, _context, ModelType::NonAnim, modelPathUtf8.c_str(), Matrix::CreateScale(0.05f));
    CHECK_NULL(_model, E_FAIL);

    LOG_INFO(
        "[TrailPreviewModel] loaded. path={}, meshes={}, materials={}",
        modelPathUtf8,
        static_cast<uint32>(_model->Get_NumMeshes()),
        static_cast<uint32>(_model->Get_NumMaterials())
    );

    return S_OK;
}

HRESULT TrailPreviewModelObject::Bind_ShaderResources()
{
    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    return S_OK;
}

Shared<TrailPreviewModelObject> TrailPreviewModelObject::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewModelObject>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewModelObject");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewModelObject::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewModelObject>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewModelObject");
        MSG_BOX("Failed to Clone : TrailPreviewModelObject");
        return nullptr;
    }

    return instance;
}

void TrailPreviewModelObject::Free()
{
    __super::Free();
}

NS_END
