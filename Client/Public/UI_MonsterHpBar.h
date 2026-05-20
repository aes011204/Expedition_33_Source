#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_MonsterHpBar : public UI_RuntimePanelBase
{
public:
    UI_MonsterHpBar() = default;
    ~UI_MonsterHpBar() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Set_TargetWorldPosition(const Vec3& worldPos);
    void    Set_Hp(float curHp, float maxHp);

    void    Open() override;
    void    Close() override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    bool    Try_ProjectWorldToScreen(const Vec3& worldPos, Vec2& outScreenPos) const;
    void    Apply_HpGauge(float ratio);

private:
    Shared<UI_Image> _hpGauge{};

    Vec2 _hpGaugeBasePos{};
    Vec2 _hpGaugeBaseSize{};

    Vec3 _targetWorldPos = Vec3::Zero;
    bool _hasTargetWorldPos = false;


public:
    static Unique<UI_MonsterHpBar> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
