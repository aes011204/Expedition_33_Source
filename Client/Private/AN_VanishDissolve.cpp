#include "AN_VanishDissolve.h"

#include "EffectCom.h"
#include "GameObject.h"

IMPLEMENT_REFLECTION(AN_VanishDissolve)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

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

    effectCom->Start_VanishDissolve(0.7f, "T_Perlin_Noise_M", true);
}
