#include "pch.h"
#include "EffectEditorSkybox.h"

#include "Camera.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "TransformCom.h"
#include "VIBuffer_Cube.h"

NS_BEGIN(EffectEditor)

EffectEditorSkybox::EffectEditorSkybox(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

EffectEditorSkybox::EffectEditorSkybox(const EffectEditorSkybox& prototype)
    : GameObject{ prototype }
{
}

HRESULT EffectEditorSkybox::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _transformCom->Set_Scale(Vec3(100.f, 100.f, 100.f));

    return S_OK;
}

void EffectEditorSkybox::Late_Update(float)
{
    GAME->Add_RenderGroup(RenderGroup::Priority, GetSharedPtr<GameObject>());
}

HRESULT EffectEditorSkybox::Render()
{
    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED(_shader->Begin(0), E_FAIL);
    CHECK_FAILED(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT EffectEditorSkybox::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Texture_Effect_EditorSkybox", _texture), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Shader_VtxCube", _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"VIBuffer_Cube", _viBuffer), E_FAIL);

    return S_OK;
}

HRESULT EffectEditorSkybox::Bind_ShaderResources()
{
    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera != nullptr)
    {
        const Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();
        if (cameraTransform != nullptr)
            _transformCom->Set_WorldPosition(cameraTransform->Get_WorldPosition());
    }

    CHECK_FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);

    return S_OK;
}

Shared<EffectEditorSkybox> EffectEditorSkybox::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectEditorSkybox>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : EffectEditorSkybox");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> EffectEditorSkybox::Clone(void* arg)
{
    auto instance = make_shared<EffectEditorSkybox>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : EffectEditorSkybox");
        MSG_BOX("Failed to Clone : EffectEditorSkybox");
        return nullptr;
    }

    return instance;
}

void EffectEditorSkybox::Free()
{
    __super::Free();
}

NS_END
