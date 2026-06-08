#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
NS_END

NS_BEGIN(Client)

class SavePoint_Panel : public UI_RuntimePanelBase
{
public:
    enum class SavePointAction
    {
        None,
        Rest,
        UpgradeStat,
        UpgradeSkill,
        UpgradeWeapon
    };

public:
    SavePoint_Panel() = default;
    virtual ~SavePoint_Panel() = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void Open() override;
    void Close() override;
    void Close_Immediately(bool restoreWorldControl = true);
    void Close_ForMenuTransition();
    void Toggle();

    bool Is_ClosingWithOutro() const { return _isClosingWithOutro; }

    SavePointAction Get_LastAction() const { return _lastAction; }
    SavePointAction Consume_LastAction(); // GameplayUI가 한번 읽고 비우는 용도

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Start_CloseWithOutro(bool restoreWorldControl);
    void    Play_TargetAnimation(const wchar_t* animationName);
    void    Stop_TargetAnimations();
    void    Handle_ButtonInput();  // 버튼 클릭 감지 및 결과 저장

private:
    SavePointAction _lastAction = SavePointAction::None;
    bool            _isClosingWithOutro = false;
    bool            _restoreWorldControlOnClose = true;
    float           _closeElapsed = 0.f;
    float           _outroDuration = 1.f;

    // 버튼4개
    Shared<UI_Button> _restButton{};
    Shared<UI_Button> _upgradeStatButton{};
    Shared<UI_Button> _upgradeSkillButton{};
    Shared<UI_Button> _upgradeWeaponButton{};

public:
    static Unique<SavePoint_Panel> Create(uint32 levelIndex);
    void Free() override;
    
};

NS_END
