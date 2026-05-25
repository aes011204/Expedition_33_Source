#pragma once

#include "UI_RuntimePanelBase.h"
#include "BattleController.h"

NS_BEGIN(Client)

class UI_ReactionResult final : public UI_RuntimePanelBase
{
public:
    UI_ReactionResult() = default;
    ~UI_ReactionResult() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Show_Reaction(BattleController::ReactionType reactionType);

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
