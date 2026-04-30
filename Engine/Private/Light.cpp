#include "Light.h"

Light::Light()
{
}

HRESULT Light::Initialize(const LIGHT_DESC& lightDesc)
{
	memcpy(&_desc, &lightDesc, sizeof(LIGHT_DESC));

    return S_OK;
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

