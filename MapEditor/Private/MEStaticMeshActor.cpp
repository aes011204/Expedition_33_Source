#include "MEStaticMeshActor.h"
#include "GameInstance.h"
#include "Material_Types.h"

HRESULT MEStaticMeshActor::Render(const Matrix& worldMatrix)
{
    if (!_model || !_shader)
    {
        return E_FAIL;
    }

    // Bind Resources
    {
        CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    }

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const Shared<Material> material = _model->Get_Material(i);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        const uint32 passIndex = 2;
        if (FAILED(_shader->Begin(passIndex)))
            return E_FAIL;

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

    // Bind Resources
    {
        CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_iObjectID", &id, sizeof(uint32)), E_FAIL);
    }

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const Shared<Material> material = _model->Get_Material(i);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        // 하드코딩 !!!!!!!!!! TODO 추후 적당한 매핑 필요
        const uint32 passIndex = 6;
        if (FAILED(_shader->Begin(passIndex)))
            return E_FAIL;

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
