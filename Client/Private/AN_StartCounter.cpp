#include "AN_StartCounter.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_StartCounter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("스트링", strTest);
    PROPERTY_WSTRING("W스트링", wstrTest);

    
    return true;
}

void AN_StartCounter::Execute(const AnimNotifyContext& context)
{
    if (!context.owner)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle)
    {
        battle->StartCounter(context.model);
    }

}
