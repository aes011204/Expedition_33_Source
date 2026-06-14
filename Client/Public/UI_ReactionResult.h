#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

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
    void    Show_Reaction(BattleReactionType reactionType, const Vec2& screenPos);

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

    bool  _isShowing = false; // 성공 텍스트가 자동 닫힘 타이머로 관리되는 중인지
    float _elapsed = 0.f;
    float _lifeTime = 0.85f;  // Dodge/Parry 텍스트를 화면에 유지할 시간

public:
    static Unique<UI_ReactionResult> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
