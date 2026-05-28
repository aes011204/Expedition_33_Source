#pragma once

#include "Shader_CBuffer_Define.h"
#include "ScreenFx_Instance.h"
NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

class ScreenFx_Manager final : Base
{
public:
    ScreenFx_Manager() = default;
    ~ScreenFx_Manager() override;

public:
    bool Is_ScreenFXActivated() const { return _activeFx; }
    void Set_ScreenFxActivated(bool active) { _activeFx = active; }
#ifdef _DEBUG
    bool Has_ActiveEffects() const { return !_activeEffects.empty() || _previewEffect != nullptr; }
#else
    bool Has_ActiveEffects() const { return !_activeEffects.empty(); }
#endif

    void Play_ScreenEffect(const Shared<ScreenFx_Instance>& effect);
    void Clear_ScreenEffect();

public:
    void Update(float timeDelta);
    HRESULT Render_ScreenFx(const Shared<ShaderCom>& shader, const Shared <VIBuffer_Rect>& rtBuffer);

private:
    bool _activeFx{ true };

private:
    vector<Shared<ScreenFx_Instance>> _activeEffects;

public:
#ifdef _DEBUG
    void Set_PreviewEffect(const Shared<ScreenFx_Instance>& effect) { _previewEffect = effect; }
    void Clear_PreviewEffect() { _previewEffect = nullptr; }
    Shared<ScreenFx_Instance> _previewEffect{};
#endif

public:
    static Unique<ScreenFx_Manager> Create();
};

NS_END
