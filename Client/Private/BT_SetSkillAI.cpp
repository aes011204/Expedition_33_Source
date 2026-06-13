#include "BT_SetSkillAI.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "Monster_BattleBase.h"
#include "MonsterStatCom.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(BT_SetSkillAI)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_SetSkillAI::BT_SetSkillAI()
{
}

BT_SetSkillAI::~BT_SetSkillAI()
{
}

void BT_SetSkillAI::OnStart()
{
}

BNode::NODE_STATE BT_SetSkillAI::Logic_Update(float deltaTime)
{
    vector<SkillCandidate> candidates; // 스킬 후보 

    const Shared<BlackBoard> bb = _AICtrl.lock()->Get_BlackBoard();

    const Shared<MonsterStatCom> stat = _owner.lock()->Get_Component<MonsterStatCom>();
    CHECK_NULL(stat, BNode::NODE_STATE::FAILED);

    auto owner = dynamic_pointer_cast <Monster_BattleBase>(_owner.lock());

    vector<string> SkillIDs = stat->Get_SkillKey();

    if (SkillIDs.empty())
        return BNode::NODE_STATE::FAILED;
    //int32 SkillNum = static_cast<int32>(SkillIDs.size());

    const any* heroesValue = bb->Find_Value("Heroes");
    if (heroesValue == nullptr)
        return BNode::NODE_STATE::FAILED;

    const auto& heroes = any_cast<const vector<BattleChar>&>(*heroesValue);

    if (heroes.empty())
        return BNode::NODE_STATE::FAILED;

    Monster_BattleBase::SkillChoiceContext context = owner->Build_SkillChoiceContext(bb, heroes);

    for (int i =0; SkillIDs.size(); i++)
    {
        const Skill_Data* skill = CLIENT->Get_Skill(SkillIDs[i]);

        if(context.phaseJustChanged == true)
        {
            if(context.phase == Monster_BattleBase::BattlePhase::Critical &&skill->forceOnPhaseCritical == true)
            {
                SelectSkill(bb, i);
                return BNode::NODE_STATE::SUCCESS;
            }
            if (context.phase == Monster_BattleBase::BattlePhase::Opening&&skill->forceOnPhaseEnter == true)
            {
                SelectSkill(bb, i);
                return BNode::NODE_STATE::SUCCESS;
            }
        }

        //if (currentTurn < skill->minTurn)
        //    continue;

       /* if (skill->maxUseCount >= 0 && usedCount >= skill->maxUseCount)
            continue;

        if (skill.useCooldown && Is_OnCooldown(skillKey))
            continue;*/

        float score = owner->Score_Skill( skill, context, heroes);

        if (score < -999)
            continue;

        candidates.push_back({ i, score });

    }

    int32 bestSkillIndex = -1;
    float bestScore = -FLT_MAX;

    for (const SkillCandidate& candidate : candidates)
    {
        if (candidate.score > bestScore)
        {
            bestScore = candidate.score;
            bestSkillIndex = candidate.skillIndex;
        }
    }

    SelectSkill(bb, bestSkillIndex);

    return BNode::NODE_STATE::SUCCESS;
}

void BT_SetSkillAI::SelectSkill(const Shared<BlackBoard>& bb, int32 SkillIndex)
{

    bb->Set_Value("SkillIndex", SkillIndex);
    bb->Set_Value("ActionType", static_cast<int>(EBattleContext::Skill));
}

void BT_SetSkillAI::OnFinish()
{
}

Shared<BT_SetSkillAI> BT_SetSkillAI::Create()
{
    return make_shared<BT_SetSkillAI>();
}

void BT_SetSkillAI::Free()
{
    Task::Free();
}
