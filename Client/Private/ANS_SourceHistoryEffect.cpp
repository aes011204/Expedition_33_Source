#include "ANS_SourceHistoryEffect.h"

#include "EffectCom.h"

IMPLEMENT_REFLECTION(ANS_SourceHistoryEffect)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Effect 이름", _effectName);
    PROPERTY_WSTRING("대상 파츠", _partTag);
    PROPERTY_WSTRING("레이어 태그", _layerTag);
    PROPERTY_ENUM("Source Binding", _sourceBindingMode, SourceHistorySourceBindingMode);
    PROPERTY_STRING("소켓 이름", _socketName);
    PROPERTY_ENUM("Source Point", _sourcePointPolicy, SourceHistorySourcePointPolicy);

    return true;
}

void ANS_SourceHistoryEffect::On_Begin(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner);
    if (effectCom == nullptr)
        return;

    effectCom->Play_SourceHistoryEffect(_effectName, _partTag, _layerTag, _sourcePointPolicy, _sourceBindingMode, _socketName);
}

void ANS_SourceHistoryEffect::On_Tick(const AnimNotifyContext&)
{
}

void ANS_SourceHistoryEffect::On_End(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner);
    if (effectCom == nullptr)
        return;

    effectCom->Stop_SourceHistoryEffect(_effectName, _partTag, _sourceBindingMode, _socketName);
}
