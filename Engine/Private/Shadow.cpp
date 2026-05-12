#include "pch.h"
#include "Shadow.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"

HRESULT Shadow::Update_ShadowLight(const ShadowLightDesc& desc)
{
    _desc = desc;

    _viewMatrix = Matrix::CreateLookAt(desc.eye, desc.at, Vec3::Up);
    _projMatrix = Matrix::CreatePerspectiveFieldOfView(desc.fovy, desc.aspect, desc.zn, desc.zf);

    return S_OK;
}

HRESULT Shadow::Bind_TransformStateMatrix(Shared<ShaderCom> shader, const char* viewName, const char* projName)
{
    CHECK_NULL(shader, E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(viewName, &_viewMatrix), E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(projName, &_projMatrix), E_FAIL);

    return S_OK;
}

HRESULT Shadow::Bind_ShadowCB(Shared<ShaderCom> shader)
{
    CHECK_NULL(shader, E_FAIL);

    ShadowCB shadowCB{};
    shadowCB.lightViewMatrix = _viewMatrix;
    shadowCB.lightProjMatrix = _projMatrix;
    shadowCB.lightFar = _desc.zf;
    shadowCB.bias = _desc.bias;
    shadowCB.intensity = _desc.intensity;

    return shader->Bind_CBufferData(shadowCB);
}

HRESULT Shadow::Bind_ShadowLightDesc(Shared<ShaderCom> shader, const char* lightFarName, const char* biasName, const char* intensityName)
{
    if (lightFarName)   
        CHECK_FAILED(shader->Bind_RawValue(lightFarName, &_desc.zf, sizeof _desc.zf), E_FAIL);
    if (biasName)
        CHECK_FAILED(shader->Bind_RawValue(biasName, &_desc.bias, sizeof _desc.bias), E_FAIL);
    if (intensityName)
        CHECK_FAILED(shader->Bind_RawValue(intensityName, &_desc.intensity, sizeof _desc.intensity), E_FAIL);

    return S_OK;
}

void Shadow::Clear()
{
    _viewMatrix = Matrix::Identity;
    _projMatrix = Matrix::Identity;
}

Unique<Shadow> Shadow::Create()
{
    return make_unique<Shadow>();
}

void Shadow::Free()
{
    Base::Free();
}

const Matrix* Shadow::Get_ViewMatrix() const
{
    return &_viewMatrix;
}

const Matrix* Shadow::Get_ProjMatrix() const
{
    return &_projMatrix;
}

const ShadowLightDesc* Shadow::Get_ShadowLightDesc() const
{
    return &_desc;
}
