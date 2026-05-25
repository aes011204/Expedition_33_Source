#include "pch.h"
#include "ANS_Visible_Parts.h"

#include <PartObject.h>
#include "Character_BattleBase.h"

#include "ModelCom.h"

IMPLEMENT_REFLECTION(ANS_Visible_Parts)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("end", _end);
    PROPERTY_FLOAT("start", _start);

    PROPERTY_WSTRING("partName", _partName);



    return true;
}

void ANS_Visible_Parts::On_Begin(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    chart = dynamic_cast<Character_BattleBase*>(context.owner)->Get_PartObject(_partName);
 
}

void ANS_Visible_Parts::On_Tick(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    float alpha = context.notifyStateProgress;
    
    chart->

}

void ANS_Visible_Parts::On_End(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    LOG_DEBUG("ANS_Test End");
}
