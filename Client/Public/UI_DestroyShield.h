#pragma once

#include "UI_BattleFloatingTextBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_DestroyShield final : public UI_BattleFloatingTextBase
{
public:
    UI_DestroyShield() = default;
    ~UI_DestroyShield() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Play_DestroyShield(const BattleChar& target, uint32 stackIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    Shared<UI_Base> _destroyShieldIcon{};

    float _lifeTimeSec = 0.5f;
    float _riseDistance = 20.f;
    Vec3  _worldAnchorOffset = Vec3(0.f, -0.4f, 0.f);
    Vec2  _screenBaseOffset = Vec2(-30.f, 50.f);
    float _stackHorizontalOffset = 80.f;
    float _stackVerticalOffset = 22.f;

public:
    static Unique<UI_DestroyShield> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
