#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_DamageFont final : public UI_RuntimePanelBase
{
public:
    UI_DamageFont() = default;
    ~UI_DamageFont() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;

    void    Play_DamageFont(const Vec3& worldPos, float damage, uint32 stackIndex);
    bool    Is_Playing() const { return _isPlaying; }
    float   Get_Elapsed() const { return _elapsed; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Set_DamageText(int32 damageValue);          // 앞/뒤 텍스트를 항상 같이 바꾼다.
    void    Apply_ScreenOffset(Vec2& screenPos) const;  // 동시 표시 시 살짝 흩어지게 한다.

private:
    Shared<UI_Text> _damageText{};
    Shared<UI_Text> _damageTextBg{};

    Vec3  _targetWorldPos = Vec3::Zero;
    bool  _hasTargetWorldPos = false;

    bool  _isPlaying = false;
    float _elapsed = 0.f;
    float _lifeTime = 1.05f;

    uint32 _stackIndex = 0;

public:
    static Unique<UI_DamageFont> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
