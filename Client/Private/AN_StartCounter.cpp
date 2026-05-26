#include "AN_StartCounter.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_StartCounter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Slow 배율", _slowTimeScale);
    PROPERTY_FLOAT("Slow 지속시간", _slowDuration);
    PROPERTY_FLOAT("Counter PlayerRate", _counterPlayRate);

    return true;
}

void AN_StartCounter::Execute(const AnimNotifyContext& context)
{
    if (!context.owner)
        return;

    BattleSlowDesc slowDesc{};
    slowDesc.elapsed = 0.f;
    slowDesc.duration = _slowDuration;
    slowDesc.worldTimeScale = _slowTimeScale;

    auto battle = CLIENT->Get_BattleController();
    if (battle)
    {
        battle->StartCounter(context.model, slowDesc, _counterPlayRate);
    }

}
