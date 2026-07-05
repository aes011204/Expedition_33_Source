#pragma once

#include "GameplayUI_Types.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_EventDialoguePlayer final : public UI_RuntimePanelBase
{
public:
    UI_EventDialoguePlayer() = default;
    ~UI_EventDialoguePlayer() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    bool    Open_EventDialogue(const EventDialogueUIRequest& request);
    void    Close() override;
    void    Close_Immediately();

private:
    void    Release_LoadedDocument();

private:
    wstring _currentRootUIName{};
    float   _autoCloseRemainingSec = 0.f;
    bool    _hasAutoClose = false;

public:
    static Unique<UI_EventDialoguePlayer> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
