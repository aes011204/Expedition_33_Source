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
    void    Close() override;

    void    Play_DestroyShield(
        const BattleChar& target,
        uint32 stackIndex,
        AttackType attackType,
        const Vec3* overrideWorldAnchor = nullptr);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    bool Try_GetFeedbackWorldPosition(const BattleChar& target, Vec3& outWorldPos) const override;
    Vec2 Resolve_ScreenBaseOffset(const Vec3& worldPos) const override;

private:
    Shared<UI_Base> _destroyShieldIcon{};

    AttackType _activeAttackType = AttackType::Invalid;
    bool _hasOverrideWorldAnchor = false;
    Vec3 _overrideWorldAnchor = Vec3::Zero;

    float _visualScale = 1.5f;
    float _lifeTimeSec = 0.5f;
    float _riseDistance = 72.f;
    Vec3  _worldAnchorOffset = Vec3(0.f, 0.f, 0.f);
    Vec2  _screenBaseOffset = Vec2(0.f, -8.f);
    Vec2  _farScreenBaseOffset = Vec2(0.f, 18.f);
    Vec2  _freeAimFarScreenBaseOffset = Vec2(0.f, 48.f);
    float _screenOffsetCloseDistance = 5.f;
    float _screenOffsetFarDistance = 14.f;
    Vec2  _screenClampPadding = Vec2(120.f, 130.f);

	float _stackHorizontalOffset = -120.f;
	float _freeAimStackHorizontalOffset = -120.f;
    float _stackVerticalOffset = 22.f;
    float _worldAnchorRandomMinY = -0.12f;
    float _worldAnchorRandomMaxY = 0.18f;

public:
    static Unique<UI_DestroyShield> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
