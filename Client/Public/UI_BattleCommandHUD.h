#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_BattleCommandHUD final : public UI_RuntimePanelBase
{
public:
    UI_BattleCommandHUD() = default;
    ~UI_BattleCommandHUD() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Refresh_ForState(BattleTurnState state);
    void    Set_TargetWorldPosition(const Vec3& worldPos);

private:
    struct CommandSlot
    {
        Shared<UI_Base> root{};
        Shared<UI_Base> frameBG{};
        Shared<UI_Base> frameLeft{};
        Shared<UI_Base> frameRight{};
        Shared<UI_Base> text{};
        Shared<UI_Base> key{};
    };

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_DefaultHighlight();
    HRESULT Bind_CommandSlot(
        const wstring& commandName,
        const wstring& keyName,
        bool requireKey,
        CommandSlot& outSlot);
    void    Set_CommandSlotHighlighted(const CommandSlot& slot, bool highlighted);

private:
    CommandSlot _attackSlot{};
    CommandSlot _skillSlot{};
    CommandSlot _itemSlot{};
    CommandSlot _shotSlot{};

private:
    Vec2 _normalScale = Vec2(1.f, 1.f);
    Vec2 _highlightScale = Vec2(1.08f, 1.08f);

    Vec3 _targetWorldPos = Vec3::Zero;
    bool _hasTargetWorldPos = false;

public:
    static Unique<UI_BattleCommandHUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
