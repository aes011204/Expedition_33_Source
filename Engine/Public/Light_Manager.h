#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

class Light_Manager final : public Base
{
private:
	Light_Manager();

public:
	virtual ~Light_Manager() = default;

public:
	const LIGHT_DESC* Get_LightDesc(uint32 index);
    const LIGHT_DESC* Get_PrimaryShadowLightDesc() const;
    uint32 Get_LightCount() const;
    void Clear_Lights();
    void Get_LightDescs(vector<LIGHT_DESC>& outLightDescs) const;
    HRESULT Set_Lights(const vector<LIGHT_DESC>& lightDescs);
    HRESULT Update_PrimaryShadowLightDesc(const LIGHT_DESC& lightDesc);

public:
	HRESULT Add_Light(const LIGHT_DESC& lightDesc);
    HRESULT Render_Lights(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer);

private:
	list<Shared<class Light>>			_lights;

public:
	static Unique<Light_Manager> Create();
	virtual void Free() override;
};

NS_END
