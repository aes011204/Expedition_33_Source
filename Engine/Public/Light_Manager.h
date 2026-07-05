#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;
struct LightCB;

class Light_Manager final : public Base
{
private:
	Light_Manager();

public:
	virtual ~Light_Manager() = default;

public:
	const LIGHT_DESC* Get_LightDesc(uint32 index);
    const LIGHT_DESC* Get_PrimaryShadowLightDesc() const;
    HRESULT Bind_PrimaryShadowLightCB(Shared<ShaderCom> shader) const;
    uint32 Get_LightCount() const;
    void Clear_Lights();
    void Get_LightDescs(vector<LIGHT_DESC>& outLightDescs) const;
    HRESULT Set_Lights(const vector<LIGHT_DESC>& lightDescs);
    HRESULT Update_PrimaryShadowLightDesc(const LIGHT_DESC& lightDesc);
    const LIGHT_DESC* Get_PointLightDesc(uint32 pointIndex) const;
    uint32 Get_PointLightCount() const;
    HRESULT Add_PointLight(
        const Vec3& position,
        float range,
        const Vec4& diffuse,
        const Vec4& ambient,
        const Vec4& specular,
        float attenuationPower = 1.f);
    HRESULT Update_PointLightDesc(uint32 pointIndex, const LIGHT_DESC& lightDesc);
    HRESULT Set_PointLightPosition(uint32 pointIndex, const Vec3& position);
    HRESULT Set_PointLightRange(uint32 pointIndex, float range);
    HRESULT Set_PointLightAlpha(uint32 pointIndex, float alpha);
    HRESULT Set_PointLightAttenuationPower(uint32 pointIndex, float attenuationPower);

public:
	HRESULT Add_Light(const LIGHT_DESC& lightDesc);
    HRESULT Render_Lights(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer);

private:
    Shared<class Light> Find_PointLight(uint32 pointIndex) const;

private:
	list<Shared<class Light>>			_lights;

public:
	static Unique<Light_Manager> Create();
	virtual void Free() override;
};

NS_END
