#include "Light_Manager.h"
#include "Light.h"
#include "ShaderCom.h"

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

const LIGHT_DESC* Light_Manager::Get_PrimaryShadowLightDesc() const
{
    // 아직 사용 안 한다. 이후 환경 요소 설정 관련해서 필요해지면 연결
    if (_lights.empty())
        return nullptr;

    for (const auto& light : _lights)
    {
        if (!light) continue;

        const LIGHT_DESC* desc = light->Get_LightDesc();
        if (!desc) continue;

        if (desc->type == LightType::Directional && desc->castShadow)
            return desc;
    }

    return nullptr;
}

uint32 Light_Manager::Get_LightCount() const
{
    return static_cast<uint32>(_lights.size());
}

void Light_Manager::Clear_Lights()
{
    _lights.clear();
}

void Light_Manager::Get_LightDescs(vector<LIGHT_DESC>& outLightDescs) const
{
    outLightDescs.clear();
    outLightDescs.reserve(_lights.size());

    for (const auto& light : _lights)
    {
        if (light == nullptr)
            continue;

        const LIGHT_DESC* desc = light->Get_LightDesc();
        if (desc == nullptr)
            continue;

        outLightDescs.push_back(*desc);
    }
}

HRESULT Light_Manager::Add_Light(const LIGHT_DESC& lightDesc)
{
	Shared<Light> light = Light::Create(lightDesc);
	if (nullptr == light)
		return E_FAIL;

	_lights.push_back(light);

	return S_OK;
}

HRESULT Light_Manager::Set_Lights(const vector<LIGHT_DESC>& lightDescs)
{
    _lights.clear();

    for (const LIGHT_DESC& lightDesc : lightDescs)
    {
        if (FAILED(Add_Light(lightDesc)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT Light_Manager::Update_PrimaryShadowLightDesc(const LIGHT_DESC& lightDesc)
{
    Shared<Light> fallbackDirectional = nullptr;

    for (auto& light : _lights)
    {
        if (light == nullptr)
            continue;

        const LIGHT_DESC* desc = light->Get_LightDesc();
        if (desc == nullptr || desc->type != LightType::Directional)
            continue;

        if (fallbackDirectional == nullptr)
            fallbackDirectional = light;

        if (desc->castShadow)
            return light->Update_Desc(lightDesc);
    }

    if (fallbackDirectional != nullptr)
        return fallbackDirectional->Update_Desc(lightDesc);

    if (_lights.empty())
        return E_FAIL;

    return _lights.front()->Update_Desc(lightDesc);
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

