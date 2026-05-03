#include "Light_Manager.h"
#include "Light.h"

Light_Manager::Light_Manager()
{
}

const LIGHT_DESC* Light_Manager::Get_LightDesc(uint32 index)
{
    if (_lights.empty())
        return nullptr;

	auto iter = _lights.begin();

    for (uint32 i = 0; i < index; i++)
    {
        ++iter;
        if (iter == _lights.end())
            return nullptr;
    }

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

HRESULT Light_Manager::Render_Lights(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer)
{
    for (auto& pLight : _lights)
    {
        if (nullptr != pLight)
            pLight->Render(shader, viBuffer);
    }

    return S_OK;
}

Unique<Light_Manager> Light_Manager::Create()
{
	return Unique<Light_Manager>{ new Light_Manager() };
}

void Light_Manager::Free()
{
	__super::Free();

	_lights.clear();

}

