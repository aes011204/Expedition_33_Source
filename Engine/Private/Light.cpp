#include "Light.h"
#include "Shader.h"
#include "VIBuffer_Rect.h"

Light::Light()
{
}

HRESULT Light::Initialize(const LIGHT_DESC& lightDesc)
{
	memcpy(&_desc, &lightDesc, sizeof(LIGHT_DESC));

    return S_OK;
}

HRESULT Light::Render(Shared<Shader> shader, Shared<VIBuffer_Rect> viBuffer)
{
    int32 pass = 0;

    if (_desc.type == LightType::Directional)
    {
        if (FAILED(shader->Bind_RawValue("g_vLightDir", &_desc.direction, sizeof _desc.direction)))
            return E_FAIL;

        pass = ETOI(DeferredPass::Directional);
    }
    else if (_desc.type == LightType::Point)
    {
        if (FAILED(shader->Bind_RawValue("g_vLightPos", &_desc.position, sizeof _desc.position)))
            return E_FAIL;
        if (FAILED(shader->Bind_RawValue("g_fLightRange", &_desc.range, sizeof _desc.range)))
            return E_FAIL;

        pass = ETOI(DeferredPass::Point);
    }

    if (FAILED(shader->Bind_RawValue("g_vLightDiffuse", &_desc.diffuse, sizeof _desc.diffuse)))
        return E_FAIL;
    if (FAILED(shader->Bind_RawValue("g_vLightAmbient", &_desc.ambient, sizeof _desc.ambient)))
        return E_FAIL;
    if (FAILED(shader->Bind_RawValue("g_vLightSpecular", &_desc.specular, sizeof _desc.specular)))
        return E_FAIL;

    shader->Begin(pass);

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

