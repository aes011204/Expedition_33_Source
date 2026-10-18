#pragma once

#include "UI_BattleFloatingTextBase.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_HealFont final : public UI_BattleFloatingTextBase
{
public:
    UI_HealFont() = default;
    ~UI_HealFont() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;

    void Open() override;

    void Play_HealFont(const BattleChar& target, float healAmount, uint32 stackIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Set_HealText(int32 healValue);
    bool Try_GetFeedbackWorldPosition(const BattleChar& target, Vec3& outWorldPos) const override;

private:
    Shared<UI_Text> _healText{};
    Shared<UI_Text> _healTextBg{};
    Shared<UI_Text> _plusText{};
    Shared<UI_Text> _plusTextBg{};

public:
    static Unique<UI_HealFont> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
