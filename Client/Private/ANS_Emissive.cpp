#include "pch.h"
#include "ANS_Emissive.h"

#include <PartObject.h>
#include "Character_BattleBase.h"

#include "ModelCom.h"

IMPLEMENT_REFLECTION(ANS_Emissive)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("end", _end);
    PROPERTY_FLOAT("start", _start);

    PROPERTY_WSTRING("partName", _partName);
    PROPERTY_INT("matIndex", _matIndex);


    return true;
}

void ANS_Emissive::On_Begin(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    if (_partName == L"")
    {
        modelCom = context.model;
    }
    else
        modelCom = dynamic_cast<Character_BattleBase*>(context.owner)->Get_PartObject(_partName)->Get_Component<ModelCom>().get();

}

void ANS_Emissive::On_Tick(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    float alpha = context.notifyStateProgress;
    float process = lerp(_start, _end, context.notifyStateProgress);


    if(_matIndex == -1)
    {
        for(int i =0; i< modelCom->Get_NumMaterials(); i++)
        {
        auto cb = modelCom->Get_Material(i)->Get_CB();
        cb.emissiveIntensity = process;
        modelCom->Get_Material(i)->Set_CB(cb);
        }
    }
    else
    {
        
    auto cb = modelCom->Get_Material(_matIndex)->Get_CB();
    cb.emissiveIntensity = process;
    modelCom->Get_Material(_matIndex)->Set_CB(cb);
    }


}

void ANS_Emissive::On_End(const AnimNotifyContext& context)
{
    if (!context.owner || !context.model)
        return;

    LOG_DEBUG("ANS_Test End");
}
