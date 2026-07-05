#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Button;
NS_END

NS_BEGIN(Client)

class UI_DailyLog : public UI_RuntimePanelBase
{
public:
    UI_DailyLog() = default;
    ~UI_DailyLog() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Open(JournalOpenAnimation openAnimation);
    void    Close() override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Suppress_ButtonHoverAnimations();
    void    Play_OpenAnimation(JournalOpenAnimation openAnimation);

private:
    Shared<UI_Button> _journalUIButton{};
    Shared<UI_Button> _journalMapButton{};

public:
    static Unique<UI_DailyLog> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
