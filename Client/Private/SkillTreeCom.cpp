#include "SkillTreeCom.h"
#include <GameObject.h>
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "PlayerStatCom.h"

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

    auto playerStatCom = owner->Get_Component<PlayerStatCom>();

    if(!playerStatCom)
        return;

    //TODO 플레이어 skillpoint 깎는거

    if (playerStatCom->Get_PlayerDesc().skillPoint - skillTreeDesc->unlockCost)
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

    Shared<PlayerStatCom> playerStatCom = owner->Get_Component<PlayerStatCom>();

    if (!playerStatCom)
        return false;


    return playerStatCom->Get_PlayerDesc().skillPoint >= skillTreeDesc->unlockCost;
}

bool SkillTreeCom::HasEnoughSkillPoint(const string& skillId)
{
    auto skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillId);

    if (!skillTreeDesc)
        return false;

    auto owner = _owner.lock();
    if (!owner)
        return false;

    Shared<PlayerStatCom> playerStatCom = owner->Get_Component<PlayerStatCom>();

    if (!playerStatCom)
        return false;

    return  skillTreeDesc->unlockCost <=playerStatCom->Get_PlayerDesc().skillPoint;
}

bool SkillTreeCom::HasRequiredSkill(const string& skillId)
{
    auto requirementDesc = CLIENT->Get_SkillTreeRequirementData();

    if (!requirementDesc)
        return false;

    auto iter = requirementDesc->find(skillId);

    if(iter == requirementDesc->end()) //선행 조건이 없으면 조건 만족한 것으로 봄
        return false;

   const auto& requirementSkills = iter->second;

    for( auto requirement : requirementSkills)
    {
        auto unlockedSkillIter = find(_unlockedSkillId.begin(), _unlockedSkillId.end(), requirement);

        if(unlockedSkillIter == _unlockedSkillId.end())
            return true;
    }

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
