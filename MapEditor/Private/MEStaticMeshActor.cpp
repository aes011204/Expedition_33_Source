#include "MEStaticMeshActor.h"
#include "GameInstance.h"

HRESULT MEStaticMeshActor::Render(const Matrix& worldMatrix)
{
    if (!_model || !_shader)
    {
        return E_FAIL;
    }

    CHECK_FAILED(_shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);

    //Vec4 lightDir = { 0.5f, -1.f, 0.5f, 0.f };
    //Color diffuse = { 1.f, 1.f, 1.f, 1.f };
    //Color ambient = { 0.3f, 0.3f, 0.3f, 1.f };
    //Color specular = { 1.f, 1.f, 1.f, 1.f };

    //CHECK_FAILED(_shader->Bind_RawValue("g_vLightDir", &lightDir, sizeof(Vec4)), E_FAIL);
    //CHECK_FAILED(_shader->Bind_RawValue("g_vLightDiffuse", &diffuse, sizeof(Color)), E_FAIL);
    //CHECK_FAILED(_shader->Bind_RawValue("g_vLightAmbient", &ambient, sizeof(Color)), E_FAIL);
    //CHECK_FAILED(_shader->Bind_RawValue("g_vLightSpecular", &specular, sizeof(XMFLOAT4)), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        bool hasTexture = SUCCEEDED(_model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, 1, 0));
        _shader->Begin(hasTexture ? 0 : 2);
        _model->Render(i);
    }
    return S_OK;
}

Shared<MEStaticMeshActor> MEStaticMeshActor::Create(ComPtr<Device> device, ComPtr<Context> context, const wstring& modelPath, Shared<ShaderCom> shader)
{
    auto instance = make_shared<MEStaticMeshActor>();

    string pathA = String::ToString(modelPath);
    instance->_model = ModelCom::Create(device, context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.01f));

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
