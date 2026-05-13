#include "Light.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"
#include "VIBuffer_Rect.h"

Light::Light()
{
}

HRESULT Light::Initialize(const LIGHT_DESC& lightDesc)
{
    memcpy(&_desc, &lightDesc, sizeof(LIGHT_DESC));

    return S_OK;
}

HRESULT Light::Render(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer)
{
    CHECK_NULL(shader, E_FAIL);
    CHECK_NULL(viBuffer, E_FAIL);

    uint32 pass = 0;

    if (_desc.type == LightType::Directional)
    {
        pass = ETOI(DeferredPass::Directional);
    }
    else if (_desc.type == LightType::Point)
    {
        pass = ETOI(DeferredPass::Point);
    }
    else
        return E_FAIL;

    LightCB lightCB{};
    lightCB.direction = _desc.direction;
    lightCB.position = _desc.position;
    lightCB.diffuse = _desc.diffuse;
    lightCB.ambient = _desc.ambient;
    lightCB.specular = _desc.specular;
    lightCB.range = _desc.range;

    if (FAILED(shader->Bind_CBufferData(lightCB)))
        return E_FAIL;

    if (FAILED(shader->Begin(pass)))
        return E_FAIL;

    return viBuffer->Render();
}

Shared<Light> Light::Create(const LIGHT_DESC& lightDesc)
{
    auto instance = make_shared<Light>();

    if (FAILED(instance->Initialize(lightDesc)))
    {
        MSG_BOX("Failed to Created : Light");
        return nullptr;
    }
    return instance;
}

void Light::Free()
{
    __super::Free();
}

