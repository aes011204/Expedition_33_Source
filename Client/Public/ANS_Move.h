#pragma once
#include "AnimNotifyState.h"
#include "AnimNotifyState.h"
NS_BEGIN(Engine)

class TransformCom;
NS_END
NS_BEGIN(Client)
class ANS_Move : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_Move)

public:
    ANS_Move() = default;
    ~ANS_Move() override = default;

public:
    string Get_TypeName() const override { return "ANS_Move"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Vec3 _goalPos ={};
    Vec3 _startPos = {};

    Vec3 _startRot = {};
    Vec3 _goalRot = {};

   bool  _isCounter ={};
  Shared<TransformCom> _moveTarget = {};
};

NS_END
