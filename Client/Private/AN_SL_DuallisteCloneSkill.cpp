#include "AN_SL_DuallisteCloneSkill.h"

#include "ClientInstance.h"
#include "ContainerObject.h"
#include "GameObject.h"
#include "SL_Base.h"
#include "SL_Dualliste.h"
#include "SL_DuallisteCloneSkillManager.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(AN_SL_DuallisteCloneSkill)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "AN_SL_DuallisteCloneSkill";
    info.category = "SoulLike";

    PROPERTY_STRING("Prefab Name", _prefabName);
    PROPERTY_STRING("Skill Key", _skillKey);
    PROPERTY_STRING("Animation Name", _animationName);
    PROPERTY_VEC3("Spawn Offset", _spawnOffset);
    PROPERTY_FLOAT("Goal Offset", _goalOffset, 0.f, 20.f, 0.05f);
    PROPERTY_FLOAT("Fallback Damage Multiplier", _fallbackDamageMultiplier, 0.f, 100.f, 0.1f);
    PROPERTY_FLOAT("Animation Speed", _animationSpeed, 0.01f, 10.f, 0.05f);
    PROPERTY_FLOAT("Move Speed", _moveSpeed, 0.f, 100.f, 0.1f);
    PROPERTY_FLOAT("Dissolve Duration", _dissolveDuration, 0.01f, 10.f, 0.01f);

    return true;
}

void AN_SL_DuallisteCloneSkill::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const Shared<SL_Dualliste> owner = Resolve_OwnerDualliste(context.owner);
    if (owner == nullptr)
        return;

    if (_prefabName.empty() || _animationName.empty())
        return;

    const Shared<SL_Base> target = Resolve_Target(owner);
    if (target == nullptr)
        return;

    const Shared<SL_DuallisteCloneSkillManager> manager = owner->Get_CloneSkillManager();
    if (manager == nullptr)
        return;

    if (!manager->Request_CloneSkill(
        _prefabName,
        _skillKey,
        _animationName,
        target,
        _spawnOffset,
        _goalOffset,
        _fallbackDamageMultiplier,
        _animationSpeed,
        _moveSpeed,
        _dissolveDuration))
    {
        LOG_WARN("AN_SL_DuallisteCloneSkill request failed. prefab={}, anim={}",
            _prefabName.c_str(),
            _animationName.c_str());
    }
}

Shared<SL_Dualliste> AN_SL_DuallisteCloneSkill::Resolve_OwnerDualliste(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (const Shared<SL_Dualliste> owner = dynamic_pointer_cast<SL_Dualliste>(current))
            return owner;

        if (dynamic_pointer_cast<ContainerObject>(current) && current->Get_Owner() == nullptr)
            return dynamic_pointer_cast<SL_Dualliste>(current);

        current = current->Get_Owner();
    }

    return nullptr;
}

Shared<SL_Base> AN_SL_DuallisteCloneSkill::Resolve_Target(const Shared<SL_Dualliste>& owner) const
{
    UNREFERENCED_PARAMETER(owner);

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    return controller ? dynamic_pointer_cast<SL_Base>(controller->Get_Player()) : nullptr;
}
