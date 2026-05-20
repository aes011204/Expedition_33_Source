#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

// 월드 -> 전투 진입 시점 전환 임시 UI
class UI_BattleTransition final : public UI_RuntimePanelBase
{
public:
    UI_BattleTransition() = default;
    ~UI_BattleTransition() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    // 배틀 진입
    void    Play_BattleEnter();
    void    Close() override;

    bool    Is_Playing() const { return _isPlaying; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    bool   _isPlaying = false;
    float  _elapsed = 0.f;
    float  _duration = 0.85f; 

    bool   _skipFirstUpdate = false;

    float _startRadius = 0.02f;
    float _endRadius = 1.2f;
    float _holeFeather = 0.06f;

    Shared<UI_Image> _holeMaskImage{};

public:
    static Unique<UI_BattleTransition> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
