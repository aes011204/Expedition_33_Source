#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_SuddenAttack : public UI_RuntimePanelBase
{
public:
    UI_SuddenAttack() = default;
    ~UI_SuddenAttack() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;

    void    Play_SuddenAttack();
    bool    Is_Playing() const { return _isPlaying; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    bool  _isPlaying = false;
    float _elapsed = 0.f;
    float _lifeTime = 1.f;

public:
    static Unique<UI_SuddenAttack> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
