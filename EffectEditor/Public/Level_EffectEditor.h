#pragma once

#include "Level.h"

NS_BEGIN(EffectEditor)

class Level_EffectEditor final : public Level
{
public:
    Level_EffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~Level_EffectEditor() override;

public:
    HRESULT Initialize() override;
    void Update(float timeDelta) override;
    HRESULT Render() override;

private:
    // preview에 필요한 최소 directional light만 준비한다.
    HRESULT Ready_Lights();
    // EffectEditor preview용 orbit camera를 만들고 active camera로 지정한다.
    HRESULT Ready_Layer_Camera(const wstring& layerTag);
    // 현재 smoke-test effect consumer만 preview layer에 올린다.
    HRESULT Ready_Layer_Preview(const wstring& layerTag);

public:
    static Shared<Level_EffectEditor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
