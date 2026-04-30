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
	Shared<Light> pInstance = MakeSharedFromRaw(new Light());

	if (FAILED(pInstance->Initialize(LightDesc)))
	{
		MSG_BOX("Failed to Created : Light");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void Light::Free()
{
    __super::Free();


}

