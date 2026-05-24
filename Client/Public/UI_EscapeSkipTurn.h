#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_EscapeSkipTurn final : public UI_RuntimePanelBase
{
public:
    UI_EscapeSkipTurn() = default;
    ~UI_EscapeSkipTurn() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    Shared<UI_Base> _keyC{};
    Shared<UI_Base> _keyTab{};

public:
    static Unique<UI_EscapeSkipTurn> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
