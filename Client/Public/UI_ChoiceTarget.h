#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_ChoiceTarget final : public UI_RuntimePanelBase
{
public:
    UI_ChoiceTarget() = default;
    ~UI_ChoiceTarget() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

private:
    HRESULT Load_UI(uint32 levelIndex);

public:
    static Unique<UI_ChoiceTarget> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
