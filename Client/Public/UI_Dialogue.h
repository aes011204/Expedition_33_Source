#pragma once
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)
class UI_Dialogue : public UI_RuntimePanelBase
{
public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

public:
    static Unique<UI_Dialogue> Create(uint32 levelIndex);
};



NS_END
