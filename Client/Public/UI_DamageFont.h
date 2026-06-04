#pragma once

#include "UI_BattleFloatingTextBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_DamageFont final : public UI_BattleFloatingTextBase
{
    GENERATED_BASE_TYPE_BODY(UI_DamageFont)

private:
    enum class DamageFontStyle { Normal, Critical, END };

public:
    UI_DamageFont() = default;
    ~UI_DamageFont() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;
    void Apply_EditorReflectionRuntimeTuning() override;

public:
    void Open() override;
    void Close() override;

    // 데미지 popup은 시작 위치만 잡고, 이후에는 snapshot screen position 기준으로 rise만 적용한다.
    void Play_DamageFont(const BattleChar& target, float damage, uint32 stackIndex, bool isCritical);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Set_ActiveStyle(DamageFontStyle style);
    void Set_DamageText(int32 damageValue);
    bool Apply_DamageFontTuning();

private:
    Shared<UI_Base> _normalRoot{};
    Shared<UI_Base> _criticalRoot{};

    Shared<UI_Text> _normalDamageText{};
    Shared<UI_Text> _normalDamageTextBg{};
    Shared<UI_Text> _criticalDamageText{};
    Shared<UI_Text> _criticalDamageTextBg{};

    Shared<UI_Text> _damageText{};
    Shared<UI_Text> _damageTextBg{};

    DamageFontStyle _activeStyle = DamageFontStyle::Normal;

private:
    float _damageLifeTime = 1.05f;
    float _damageRiseDistance = 72.f;

    float _damageStackHorizontalOffset = 80.f;
    float _damageStackVerticalOffset = 22.f;

    Vec3 _damageWorldAnchorOffset = Vec3(0.f, -0.4f, 0.f);
    Vec2 _damageScreenBaseOffset = Vec2(-30.f, 50.f);

    float _damageWorldAnchorRandomMinY = -0.12f;
    float _damageWorldAnchorRandomMaxY = 0.18f;

    bool  _hasAppliedDamageFontTuning = false;
    float _appliedDamageLifeTime = 0.f;
    float _appliedDamageRiseDistance = 0.f;
    float _appliedDamageStackHorizontalOffset = 0.f;
    float _appliedDamageStackVerticalOffset = 0.f;
    Vec3  _appliedDamageWorldAnchorOffset = Vec3::Zero;
    Vec2  _appliedDamageScreenBaseOffset = Vec2::Zero;
    float _appliedDamageWorldAnchorRandomMinY = 0.f;
    float _appliedDamageWorldAnchorRandomMaxY = 0.f;

public:
    static Unique<UI_DamageFont> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
