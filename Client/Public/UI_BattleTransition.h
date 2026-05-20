#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

// 월드 -> 전투 진입 시점 전환 임시 UI
class UI_BattleTransition final : public Base
{
public:
    UI_BattleTransition() = default;
    ~UI_BattleTransition() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    // 배틀 진입
    void    Play_BattleEnter();
    void    Stop();

    bool    Is_Playing() const { return _isPlaying; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    Shared<UI_Base> Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const;

private:
    uint32 _levelIndex = 0;
    bool   _isPlaying = false;
    float  _elapsed = 0.f;
    float  _duration = 0.85f; 

    bool   _skipFirstUpdate = false;

    float _startRadius = 0.02f;
    float _endRadius = 1.2f;
    float _holeFeather = 0.06f;


    umap<wstring, Shared<UI_Base>> _uiByName{};

    Shared<UI_Base> _root{};
    Shared<UI_Image> _holeMaskImage{};

public:
    static Unique<UI_BattleTransition> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
