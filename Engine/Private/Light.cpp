#include "Light.h"

Light::Light()
{
}

HRESULT Light::Initialize(const LIGHT_DESC& LightDesc)
{
	memcpy(&_desc, &LightDesc, sizeof(LIGHT_DESC));

    return S_OK;
}

Shared<Light> Light::Create(const LIGHT_DESC& LightDesc)
{
	auto pInstance = make_shared<Light>();

	if (FAILED(pInstance->Initialize(LightDesc)))
	{
		MSG_BOX("Failed to Created : Light");
		return nullptr;
	}
	return pInstance;
}

void Light::Free()
{
    __super::Free();


}

