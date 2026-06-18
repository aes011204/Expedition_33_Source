#pragma once

#include "SL_Types.h"
#include "State.h"

NS_BEGIN(Engine)
class MovementCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

struct Skill_Data;

class SL_Maelle;
class SL_StateMachineCom;
class Body_Player;

class SL_State abstract : public State
{
public:
    explicit SL_State(const Shared<SL_Maelle>& owner);
    ~SL_State() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    virtual bool Request_Move(const SL_MoveRequest& request, float timeDelta) { return false; }
    virtual bool Request_BasicAttack() { return false; }
    virtual bool Request_Dodge(const SL_MoveRequest& request);
    virtual bool Request_Skill(const Skill_Data& skillData, uint32 slotIndex) { return false; }


protected:
    bool Play_Anim(SoulLikeAnim anim, float blendDuration, bool loop);
    bool Is_AnimFinished() const;
    void Stop_Movement();

protected:
    Weak<SL_Maelle> _owner{};
    SL_StateMachineCom* _stateCom = nullptr;
    Body_Player* _body = nullptr;

public:
    void Free() override;
};

NS_END
