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
    PROPERTY_BOOL("tmp", _tmp);

    PROPERTY_WSTRING("partName", _partName);

    return true;
}

void ANS_Visible_Parts::On_Begin(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;
    if (_partName == L"")
    {
        Part = context.owner;
    }
    else
    Part = dynamic_pointer_cast<Character_BattleBase>(context.owner->Get_Owner())->Get_PartObject(_partName);
    
    Part->Set_Visible(_tmp);
}

void ANS_Visible_Parts::On_Tick(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    float alpha = context.notifyStateProgress;
    float process = lerp(_start, _end, context.notifyStateProgress);


}

void ANS_Visible_Parts::On_End(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    LOG_DEBUG("ANS_Test End");
}
