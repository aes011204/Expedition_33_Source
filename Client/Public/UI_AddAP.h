#pragma once

#include "UI_BattleFloatingTextBase.h"

NS_BEGIN(Engine)
class UI_Text;
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_AddAP final : public UI_BattleFloatingTextBase
{
public:
    UI_AddAP() = default;
    ~UI_AddAP() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;

public:
    void Open() override;

    void Play_AddAP(const BattleChar& target, uint32 apAmount, uint32 stackIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Set_APText(uint32 apAmount);
    void Reset_APVisualState();
    void Play_AddAPAnimation();
    void Stop_AddAPAnimations();

private:
    Shared<UI_Text> _apText{};
    Shared<UI_Text> _apTextBg{};
    Shared<UI_Image> _apImage{};
    Shared<UI_Image> _apImageBg{};

public:
    static Unique<UI_AddAP> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
