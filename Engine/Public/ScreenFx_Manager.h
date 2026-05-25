#pragma once

#include "Shader_CBuffer_Define.h"
#include "ScreenFx_Instance.h"
NS_BEGIN(Engine)

class ShaderCom;

class ScreenFx_Manager final : Base
{
public:
    ScreenFx_Manager() = default;
    ~ScreenFx_Manager() override;

public:
    bool Is_ScreenFXActivated() const { return _activeFx; }
    void Set_ScreenFxActivated(bool active) { _activeFx = active; }

    ScreenFxCB& Get_Parameters() { return _parameters; }
    const ScreenFxCB& Get_Parameters() const { return _parameters; }

    void Play_ScreenEffect(const Shared<ScreenFx_Instance>& effect);
    void Clear_ScreenEffect();

public:
    void Update(float timeDelta);
    HRESULT Render_ScreenFx(class ShaderCom* shader, class VIBuffer_Rect* rtBuffer);
    HRESULT Bind_ScreenFxCBuffer(ShaderCom* shader);

private:
    bool _activeFx{ true };

private:
    vector<Shared<ScreenFx_Instance>> _activeEffects;
    ScreenFxCB _parameters{};

public:
    static Unique<ScreenFx_Manager> Create();
};

NS_END
