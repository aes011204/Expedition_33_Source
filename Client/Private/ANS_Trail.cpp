#include "ANS_Trail.h"

#include "EffectCom.h"

IMPLEMENT_REFLECTION(ANS_Trail)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Effect 이름", _effectName);
    PROPERTY_WSTRING("대상 파츠", _partTag);

    return true;
}

void ANS_Trail::On_Begin(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    Shared<GameObject> effectOwner = context.owner->GetSharedPtr<GameObject>();
    Shared<EffectCom> effectCom = effectOwner ? effectOwner->Get_Component<EffectCom>() : nullptr;

    if (effectCom == nullptr && effectOwner)
    {
        effectOwner = effectOwner->Get_Owner();
        if (effectOwner)
            effectCom = effectOwner->Get_Component<EffectCom>();
    }

    if (effectCom == nullptr)
        return;

    effectCom->Play_TrailEffect(_effectName, _partTag, _layerTag);
}

void ANS_Trail::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);

}

void ANS_Trail::On_End(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    Shared<GameObject> effectOwner = context.owner->GetSharedPtr<GameObject>();
    Shared<EffectCom> effectCom = effectOwner ? effectOwner->Get_Component<EffectCom>() : nullptr;

    if (effectCom == nullptr && effectOwner)
    {
        effectOwner = effectOwner->Get_Owner();
        if (effectOwner)
            effectCom = effectOwner->Get_Component<EffectCom>();
    }

    effectCom->Stop_TrailEffect(_effectName, _partTag);
}
