#include "AN_Battle_EndAnim.h"

#include <ModelCom.h>

#include "BattleController.h"
#include "ClientInstance.h"


IMPLEMENT_REFLECTION(AN_Battle_EndAnim)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("테스트 값", _nxtAnimName);


    return true;
}

void AN_Battle_EndAnim::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (!context.owner || !context.model)
        return;

    auto Battle = CLIENT->Get_BattleController();
    if (Battle->Is_BattleStarted() == false)
        return;
    auto& action = Battle->Get_TurnAction();

    // 다음 애니로 이동
    action._curAnimIndex++;
    
    
    if (action._curAnimIndex >= action._selected_SkillKey.animationKey.size())
    {
        // 끝났으면 Idle
        string key = action._selected_SkillKey.IdleAnimationKey;
        action._curChar.ModelCom->Set_Animation(key, false, 1.5f);
    
        return;
    }
    
    // 다음 애니 실행
    string key = action._selected_SkillKey.animationKey[action._curAnimIndex];
    action._curChar.ModelCom->Set_Animation(key, false, 1.5f);
}

