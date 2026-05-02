#include "Light_Manager.h"
#include "Light.h"

Light_Manager::Light_Manager()
{
}

const LIGHT_DESC* Light_Manager::Get_LightDesc(uint32 index)
{
	auto	iter = _lights.begin();

	for (uint32 i = 0; i < index; i++)
		++iter;

	return (*iter)->Get_LightDesc();	
}

HRESULT Light_Manager::Add_Light(const LIGHT_DESC& lightDesc)
{
	Shared<Light> light = Light::Create(lightDesc);
	if (nullptr == light)
		return E_FAIL;

	_lights.push_back(light);

	return S_OK;
}

Unique<Light_Manager> Light_Manager::Create()
{
	return Unique<Light_Manager>{ new Light_Manager() };
}

void Light_Manager::Free()
{
	__super::Free();

	for (auto& light : _lights)
		Safe_Release(light);

	_lights.clear();

}

