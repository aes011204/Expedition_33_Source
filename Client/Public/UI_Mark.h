#pragma once

#include "UI_BattleFloatingTextBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_Mark final : public UI_BattleFloatingTextBase
{
public:
    UI_Mark() = default;
    ~UI_Mark() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Open() override;
    void    Close() override;

public:
    void    Show_Mark(const BattleChar& target, uint32 stackIndex);

    void    Update(float timeDelta) override;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

private:
    Shared<UI_Base> _markRoot{};

public:
    static Unique<UI_Mark> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
