#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
NS_END

NS_BEGIN(Client)

class UI_SavePoint : public Base
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
    UI_SavePoint() = default;
    virtual ~UI_SavePoint() = default;

public:
    void Open();
    void Close();
    void Toggle();

    bool Is_Open() const { return _isOpen; }
    SavePointAction Get_LastAction() const { return _lastAction; }

private:

private:
    uint32 _levelIndex = 0;
    bool _isOpen = false;

    

public:
    static Unique<UI_SavePoint> Create();
    void Free() override;
    
};

NS_END
