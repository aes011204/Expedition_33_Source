#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class SL_Maelle;

class UI_SL_LockOn final : public UI_RuntimePanelBase
{
public:
    UI_SL_LockOn() = default;
    ~UI_SL_LockOn() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;
    void    Open() override;
    void    Close() override;

public:
    void Update_Target(const Shared<SL_Maelle>& player);
    void Clear_Target();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    bool    Try_ResolveLockOnScreenPosition(const Shared<SL_Maelle>& player, Vec2& outScreenPos) const;

private:
    Shared<UI_Base> _lockOnImage{};

public:
    static Unique<UI_SL_LockOn> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
