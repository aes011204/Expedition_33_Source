#include "AN_VanishDissolve.h"

#include "EffectCom.h"
#include "GameObject.h"

IMPLEMENT_REFLECTION(AN_VanishDissolve)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("Dissolve Type", _type, DissolveType);
    PROPERTY_FLOAT("지속시간", _duration);

    return true;
}

void AN_VanishDissolve::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.owner == nullptr)
        return;

    const Shared<GameObject> monsterObject = context.owner->Get_Owner();
    if (monsterObject == nullptr)
        return;

    const Shared<EffectCom> effectCom = monsterObject->Get_Component<EffectCom>();
    if (effectCom == nullptr)
        return;

    switch (_type)
    {
    case DissolveType::Switch:
        effectCom->Start_SwitchDissolve(_duration, "T_Perlin_Noise_M");
        break;
    case DissolveType::Vanish:
        effectCom->Start_VanishDissolve(_duration, "T_Perlin_Noise_M", true);
        break;

    default: ;
    }

    
}
