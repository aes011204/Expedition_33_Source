#include "MEStaticMeshActor.h"
#include "GameInstance.h"
#include "Material_Types.h"

HRESULT MEStaticMeshActor::Render(const Matrix& worldMatrix)
{
    if (!_model || !_shader)
    {
        return E_FAIL;
    }

    CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);

    // compute shade?
    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        bool hasTexture = SUCCEEDED(_model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, MaterialTextureSlot::BaseColor, 0));
        _shader->Begin(hasTexture ? 0 : 1);
        _model->Render(i);

    }
    return S_OK;
}

HRESULT MEStaticMeshActor::RenderID(const Matrix& worldMatrix, uint32 id)
{
    if (!_model || !_shader)
    {
        return E_FAIL;
    }

    CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_iObjectID", &id, sizeof(uint32)), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        _shader->Begin(2);
        _model->Render(i);
    }

    return S_OK;
}

Shared<MEStaticMeshActor> MEStaticMeshActor::Create(ComPtr<Device> device, ComPtr<Context> context, const wstring& modelPath, Shared<ShaderCom> shader)
{
    auto instance = make_shared<MEStaticMeshActor>();

    string pathA = String::ToString(modelPath);
    instance->_model = ModelCom::Create(device, context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.001f));

    CHECK_NULL(instance->_model, nullptr);

    instance->_shader = shader;
    return instance;
}

Shared<MEStaticMeshActor> MEStaticMeshActor::Create(Shared<ModelCom> model, Shared<ShaderCom> shader)
{
    auto instance = make_shared<MEStaticMeshActor>();
    instance->_model = model;
    instance->_shader = shader;

    return instance;
}
