#pragma once

#include "UI_Animation.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_EscapeSkipTurnBack final : public UI_RuntimePanelBase
{
public:
    UI_EscapeSkipTurnBack() = default;
    ~UI_EscapeSkipTurnBack() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;
    void    Close_Immediately();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    HRESULT Load_Animations();

private:
    Shared<UI_Base> _keyC{};
    Shared<UI_Base> _keyTab{};

    UI_KeyframeAnimationAsset _introAnimation{};
    UI_KeyframeAnimationAsset _outroAnimation{};

    bool  _hasIntroAnimation = false;
    bool  _hasOutroAnimation = false;
    bool  _isClosingWithOutro = false;
    float _closeElapsed = 0.f;
    float _outroDuration = 0.f;

public:
    static Unique<UI_EscapeSkipTurnBack> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
