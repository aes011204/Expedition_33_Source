#pragma once

#include "UI_RuntimePanelBase.h"
#include "Client_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_BattleJumpReaction final : public UI_RuntimePanelBase
{
public:
    struct PromptDesc
    {
        HeroId heroId = HeroId::END;
        Vec2 screenPosition{};
    };

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;

    void Show_JumpText(const PromptDesc& desc);
    void Show_JumpCounterIntro(const PromptDesc& desc);
    void Play_JumpCounterSuccess(const PromptDesc& desc);
    void Close_JumpCounter();
    void Close() override;
    void Close_Immediately();

    static Unique<UI_BattleJumpReaction> Create(uint32 levelIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Close_IfAllClosed();
    void Set_RootPosition(const Shared<Engine::UI_Base>& root, const Vec2& position) const;
    void Play_TargetAnimation(
        const umap<wstring, Shared<Engine::UI_Base>>& uiByName,
        const wchar_t* animationName);
    void Stop_TargetAnimations(const umap<wstring, Shared<Engine::UI_Base>>& uiByName);

private:
    umap<wstring, Shared<Engine::UI_Base>> _jumpUIByName{};
    umap<wstring, Shared<Engine::UI_Base>> _jumpCounterUIByName{};

    Shared<Engine::UI_Base> _jumpRoot{};
    Shared<Engine::UI_Base> _jumpCounterRoot{};

    float _jumpTextElapsed = 0.f;
    float _jumpCounterElapsed = 0.f;
    bool _isJumpTextOpen = false;
    bool _isJumpCounterOpen = false;
    bool _isJumpCounterSuccessPlaying = false;

public:
    void Free() override;
};

NS_END
