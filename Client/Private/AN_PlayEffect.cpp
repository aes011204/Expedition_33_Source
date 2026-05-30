#include "AN_PlayEffect.h"

#include "EffectCom.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(AN_PlayEffect)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("이펙트 이름", _effectName);
    PROPERTY_ENUM("Target", _target, EffectCom::EffectPlayTarget);
    PROPERTY_WSTRING("파츠 이름", _partTag);
    PROPERTY_STRING("소켓 이름", _socketName);
    PROPERTY_WSTRING("레이어 태그", _layerTag);
    PROPERTY_VEC3("로컬 오프셋", _localOffset);

    return true;
}

void AN_PlayEffect::Execute(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner);
    if (effectCom == nullptr)
        return;

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = _effectName;
    desc.target = _target;
    desc.partTag = _partTag;
    desc.socketName = _socketName;
    desc.layerTag = _layerTag;
    desc.localOffset = _localOffset;
    desc.followTarget = false;

    effectCom->Play_Effect(desc);
}
