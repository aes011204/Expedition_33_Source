#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Engine)
class ModelCom;
class MovementCom;
NS_END

NS_BEGIN(Client)

class SL_Mon_AttackState final : public SL_Mon_State
{
public:
    explicit SL_Mon_AttackState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_AttackState() override = default;

public:
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    Shared<MovementCom> _movementCom{};
    Shared<ModelCom> _modelCom{};
    Vec3 _initPlayerPos = Vec3::Zero;
public:
    static Shared<SL_Mon_AttackState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
