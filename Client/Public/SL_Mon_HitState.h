#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Engine)
class ModelCom;
class MovementCom;
NS_END

NS_BEGIN(Client)

class SL_Mon_HitState final : public SL_Mon_State
{
public:
    explicit SL_Mon_HitState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_HitState() override = default;

public:
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;

private:
    Shared<MovementCom> _movementCom{};
    Shared<ModelCom> _modelCom{};

public:
    static Shared<SL_Mon_HitState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
