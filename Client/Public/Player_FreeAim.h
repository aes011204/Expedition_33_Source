#pragma once
#include "Player_State.h"


NS_BEGIN(Engine)
    class OffsetModifier;
NS_END

NS_BEGIN(Client)

class Player_FreeAim final : public Player_State
{
public:
    Player_FreeAim(const Shared<Player_WorldBase>& owner);
    ~Player_FreeAim() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    Shared<OffsetModifier> _offsetModifier = nullptr;
    float _rotationSensitivity = 0.1f; 

public:
    static Shared<Player_FreeAim> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;

};

NS_END
