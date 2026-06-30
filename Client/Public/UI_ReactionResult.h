#pragma once

#include "UI_BattleFloatingTextBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class UI_ReactionResult final : public UI_BattleFloatingTextBase
{
public:
    UI_ReactionResult() = default;
    ~UI_ReactionResult() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;

    void    Open() override;
    void    Close() override;
    void    Append_RuntimeUIEntries(
        vector<RuntimeUIEntry>& outEntries,
        const string& category,
        const string& rootName) const override;

public:
    void    Show_Reaction(BattleReactionType reactionType, const BattleChar& target, uint32 stackIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Load_UIFragment(
        const wchar_t* jsonPath,
        const wstring& prototypeTagPrefix,
        umap<wstring, Shared<UI_Base>>& outUIByName) const;
    HRESULT Bind_UI();

private:
    Shared<UI_Base> _dodgeRoot{};
    Shared<UI_Base> _parryRoot{};

public:
    static Unique<UI_ReactionResult> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
