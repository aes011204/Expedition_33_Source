#pragma once

#include "UI_BattleFloatingTextBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_Slow final : public UI_BattleFloatingTextBase
{
public:
    UI_Slow() = default;
    ~UI_Slow() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Open() override;
    void    Close() override;

public:
    void    Show_Slow(const BattleChar& target, uint32 stackIndex);

    void    Update(float timeDelta) override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    Shared<UI_Base> _slowRoot{};

public:
    static Unique<UI_Slow> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
