#include "AN_Dissolve_Parts.h"

#include "Character_BattleBase.h"
#include "EffectCom.h"
#include "GameObject.h"
#include "PartObject.h"

IMPLEMENT_REFLECTION(AN_Dissolve_Parts)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("Dissolve Type", _type, DissolveType);
    PROPERTY_FLOAT("지속시간", _duration);  
    PROPERTY_WSTRING("partName", _partName);
    return true;
}

void AN_Dissolve_Parts::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.owner == nullptr)
        return;

    if (_partName == L"")
    {
        Part = context.owner->Get_Owner().get();
    }
    else
        Part = (dynamic_pointer_cast<ContainerObject>(context.owner->Get_Owner()))->Get_PartObject(_partName);

    {
        assert("Errer_NOKey");
    }

    const Shared<EffectCom> effectCom = Part->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return;

    switch (_type)
    {
    case DissolveType::Switch:
        if (Part->Is_Visible() == false)
        {
            Part->Set_Visible(true);
        }

        effectCom->Start_SwitchDissolve(_duration, "T_Perlin_Noise_M");
        break;
    case DissolveType::Vanish:

        effectCom->Start_VanishDissolve(_duration, "T_Perlin_Noise_M", true);
        break;

    default:;
    }


}
