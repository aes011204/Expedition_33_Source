#include "AN_EndTurn.h"

#include "ModelCom.h"
#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_EndTurn)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

void AN_EndTurn::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.model)
    {
        context.model->Set_AnimationByState(ETOI(BattleAnimType::Base_Idle), 0.2f, true); // Item_Use 종료 프레임에서 기본 전투 Idle로 복귀한다.
    }

    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr || !battle->Is_BattleStarted())
        return;

    battle->EndTurn(); // 애니메이션이 어느정도 진행되고 턴을 끊으려고
}
