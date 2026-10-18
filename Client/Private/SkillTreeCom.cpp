#include "SkillTreeCom.h"
#include <GameObject.h>
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "PlayerStatCom.h"

SkillTreeCom::SkillTreeCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component {device, context}

{
}

SkillTreeCom::~SkillTreeCom()
{
}

HRESULT SkillTreeCom::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
   
    return S_OK;
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
   
   auto & runtimeStat = playerStatCom->Get_PlayerRuntimeStat();
   runtimeStat.CurSkillPoint -= skillTreeDesc->unlockCost;
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

    return HasEnoughSkillPoint(skillId) && HasRequiredSkill(skillId) == 1;
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

    return  skillTreeDesc->unlockCost <=playerStatCom->Get_PlayerRuntimeStat().CurSkillPoint;
}

void SkillTreeCom::ForceUnlock(const string& skillId)
{
    if (skillId.empty())
        return;

    if (IsUnlocked(skillId))
        return;

    auto skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillId);
    if (!skillTreeDesc)
        return;

    _unlockedSkillId.push_back(skillId);
}

json SkillTreeCom::Create_SaveJson() const
{
    json root = json::object();
    root["unlocked_skill_ids"] = _unlockedSkillId;

    return root;
}

bool SkillTreeCom::Apply_SaveJson(const json& data)
{
    _unlockedSkillId.clear();

    if (!data.is_object())
        return true;

    const auto it = data.find("unlocked_skill_ids");
    if (it == data.end() || !it->is_array())
        return true;

    unordered_set<string> dedup{};
    for (const json& entry : *it)
    {
        if (!entry.is_string())
            continue;

        const string skillId = entry.get<string>();
        if (dedup.insert(skillId).second)
            _unlockedSkillId.emplace_back(skillId);
    }

    return true;
}

bool SkillTreeCom::HasRequiredSkill(const string& skillId)
{
    auto requirementDesc = CLIENT->Get_SkillTreeRequirementData();

    if (!requirementDesc)
        return false;

    auto iter = requirementDesc->find(skillId);

    if(iter == requirementDesc->end())
        return true;

   const auto& requirementSkills = iter->second;

    for( auto requirement : requirementSkills)
    {
        auto unlockedSkillIter = find(_unlockedSkillId.begin(), _unlockedSkillId.end(), requirement);

        if(unlockedSkillIter != _unlockedSkillId.end())
            return true;
    }

    return false;
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
