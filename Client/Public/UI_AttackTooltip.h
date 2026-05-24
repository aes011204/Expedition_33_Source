#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_AttackTooltip final : public UI_RuntimePanelBase
{
public:
    UI_AttackTooltip() = default;
    ~UI_AttackTooltip() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

private:
    HRESULT Load_UI(uint32 levelIndex);

public:
    static Unique<UI_AttackTooltip> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
