#include "SkillTreeCom.h"

#include <GameObject.h>

#include "ClientInstance.h"
#include "CombatStatCom.h"

SkillTreeCom::SkillTreeCom(const ComPtr<Device>& device, const ComPtr<Context>& context) : Component {device, context}
{
}

SkillTreeCom::~SkillTreeCom()
{
}

bool SkillTreeCom::IsUnlocked(const string& skillId)
{
    auto iter = find(_unlockedSkillId.begin(), _unlockedSkillId.end(), skillId);

    if(iter == _unlockedSkillId.end())
        return false;

    return true;
}

void SkillTreeCom::TryUnlock(const string& skillId)
{
    if(!CanUnlock(skillId))
     return;

    auto skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillId);

    if(!skillTreeDesc)
        return;

    auto owner = _owner.lock();

    if(!owner)
        return;

    auto combatStat = owner->Get_Component<CombatStatCom>();

    if(!combatStat)
        return;

    if (combatStat->Spend_SkillPoint(skillTreeDesc->unlockCost))
        return;

    _unlockedSkillId.push_back(skillId);
}

bool SkillTreeCom::CanUnlock(const string& skillId)
{
    auto skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillId);

    if (!skillTreeDesc)
        return false;

    auto owner = _owner.lock();
    if (!owner)
        return false;

    Shared<CombatStatCom> combatStat = owner->Get_Component<CombatStatCom>();
    if (!combatStat)
        return false;

    return combatStat->Get_SkillPoint() >= skillTreeDesc->unlockCost;
}

bool SkillTreeCom::HasEnoughSkillPoint(const string& skillId)
{
    auto skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillId);

    if (!skillTreeDesc)
        return false;

    auto owner = _owner.lock();
    if (!owner)
        return false;

    Shared<CombatStatCom> combatStat = owner->Get_Component<CombatStatCom>();
    if (!combatStat)
        return false;

    return  skillTreeDesc->unlockCost <= combatStat->Get_SkillPoint();
}

bool SkillTreeCom::HasRequiredSkill(const string& skillId)
{
    auto skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillId);

    if (!skillTreeDesc)
        return false;

    if (skillTreeDesc->requiredSkillId.empty())
        return true;

    auto iter = find(_unlockedSkillId.begin(), _unlockedSkillId.end(), skillTreeDesc->requiredSkillId);

    if(iter == _unlockedSkillId.end())
        return false;

    return true;
}

Shared<SkillTreeCom> SkillTreeCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SkillTreeCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : SkillTreeCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> SkillTreeCom::Clone(void* arg)
{
    auto clone = make_shared<SkillTreeCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SkillTreeCom");
        return nullptr;
    }

    return clone;
}

void SkillTreeCom::Free()
{
    __super::Free();
}
