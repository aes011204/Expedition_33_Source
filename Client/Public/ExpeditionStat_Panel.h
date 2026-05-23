#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class ExpeditionStat_Panel final : public UI_RuntimePanelBase
{
public:
    ExpeditionStat_Panel() = default;
    ~ExpeditionStat_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;
    void    Toggle();

private:
    HRESULT Load_UI(uint32 levelIndex);

public:
    static Unique<ExpeditionStat_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
