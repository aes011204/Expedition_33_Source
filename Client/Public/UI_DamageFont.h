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

    void    Play_DamageFont(const Vec2& screenPos, float damage, uint32 stackIndex); // 월드 좌표를 계속 따라가지 않고, 시작 화면 좌표를 고정한다.
    bool    Is_Playing() const { return _isPlaying; }
    float   Get_Elapsed() const { return _elapsed; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Set_DamageText(int32 damageValue);
    void    Apply_StackOffset(Vec2& screenPos) const;          // 동시 표시 중인 숫자끼리 좌우/상하로 살짝 분산
    void    Apply_RiseOffset(Vec2& screenPos, float t) const;  // lifetime 동안 위로 떠오르는 연출

private:
    Shared<UI_Text> _damageText{};
    Shared<UI_Text> _damageTextBg{};

    Vec2  _startScreenPos = Vec2::Zero;
    bool  _hasStartScreenPos = false;

    bool  _isPlaying = false;
    float _elapsed = 0.f;
    float _lifeTime = 1.05f;
    float _riseDistance = 72.f; // 전체 lifetime 동안 위로 떠오를 총 거리

    uint32 _stackIndex = 0;

public:
    static Unique<UI_DamageFont> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
