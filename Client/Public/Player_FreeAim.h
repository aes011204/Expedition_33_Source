#pragma once
#include "Player_State.h"


NS_BEGIN(Engine)
    class OffsetAnimationModifier;
NS_END

NS_BEGIN(Client)

class Monster_WorldBase;

class Player_FreeAim final : public Player_State
{
public:
    Player_FreeAim(const Shared<Player_WorldBase>& owner);
    ~Player_FreeAim() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    bool Is_HitMonster() const { return _hasHitMonster; }
    Shared<Monster_WorldBase> Get_HitMonster() const { return _hitMonster.lock(); }

private:
    void Update_DirectionalModifiers();
    void Update_EnterTurnBurst();
    void Update_FreeAimHitState();
    Shared<Monster_WorldBase> Find_FreeAimHitMonster() const;

private:
    Shared<OffsetAnimationModifier> _offsetModifier = nullptr;
    Shared<OffsetAnimationModifier> _walkModifiers[6];

    float _freeAimTurnSpeedDegree = 1800.f;
    float _freeAimTurnResponse = 18.f;

    float _prevTurnSpeedDegree = 0.f;       
    float _prevTurnResponse = 0.f;
    bool _isEnterTurnBurstActive = false;

    float _rotationSensitivity = 0.5f; 

private:
    bool _hasHitMonster = false;
    Weak<Monster_WorldBase> _hitMonster{};

public:
    static Shared<Player_FreeAim> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;

};

NS_END
