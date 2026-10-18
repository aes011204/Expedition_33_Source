#include "BT_Set_TargetAI.h"
#include "ClientInstance.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "MB_Simon.h"
#include "MonsterStatCom.h"
#include "GameInstance.h"
#include "Monster_BattleBase.h"

IMPLEMENT_REFLECTION(BT_Set_TargetAI)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_Set_TargetAI::BT_Set_TargetAI()
{
}

BT_Set_TargetAI::~BT_Set_TargetAI()
{
}

void BT_Set_TargetAI::OnStart()
{
}

BNode::NODE_STATE BT_Set_TargetAI::Logic_Update(float deltaTime)
{

    const Shared<BlackBoard> bb = _AICtrl.lock()->Get_BlackBoard();
    CHECK_NULL(bb, BNode::NODE_STATE::FAILED);

    const any* heroesValue = bb->Find_Value("Heroes");
    if (heroesValue == nullptr)
        return BNode::NODE_STATE::FAILED;

    const auto& heroes = any_cast<const vector<BattleChar>&>(*heroesValue);
    if (heroes.empty())
        return BNode::NODE_STATE::FAILED;

    auto owner = dynamic_pointer_cast <Monster_BattleBase>(_owner.lock());

    const Shared<MonsterStatCom> stat = _owner.lock()->Get_Component<MonsterStatCom>();
    int32 skillIndex = bb->Get_Value<int32>("SkillIndex");
    auto skill = CLIENT->Get_Skill(stat->Get_SkillKey()[skillIndex]);

    if (dynamic_pointer_cast<MB_Simon>(_owner.lock()) != nullptr &&
        skill != nullptr &&
        !skill->targetingType.empty() &&
        (skill->targetingType[0] == TargetType::AllEnemies ||
            skill->targetingType[0] == TargetType::AllEnemiesJump))
    {
        for (int32 i = 0; i < static_cast<int32>(heroes.size()); ++i)
        {
            if (heroes[i].heroId != HeroId::Lune)
                continue;

            if (heroes[i].statCom == nullptr ||
                heroes[i].statCom->Get_AliveStatus() != EAliveStatus::Alive)
                continue;

            bb->Set_Value("TargetIndex", i);
            return BNode::NODE_STATE::SUCCESS;
        }
    }

    float bestScore = -FLT_MAX;
    int32 targetIndex = -1;
    //targetIndex = Math::RandNum(0, static_cast<int>(heroes.size()) - 1);
    for (int i = 0; i < heroes.size(); i++)
    {

        float score = owner->Score_Target(skill, heroes[i],i );

        if (score > bestScore)
        {
            bestScore = score;
            targetIndex = i;
        }
    }

    bb->Set_Value("TargetIndex", targetIndex);

    bool isAllTargetSkill = false;
    for (const TargetType targetType : skill->targetingType)
    {
        if (targetType == TargetType::AllEnemies || targetType == TargetType::AllEnemiesJump)
        {
            isAllTargetSkill = true;
            break;
        }
    }

    if (!isAllTargetSkill)
        owner->Push_TargetHistory(targetIndex);
    return BNode::NODE_STATE::SUCCESS;
}

void BT_Set_TargetAI::OnFinish()
{
}

Shared<BT_Set_TargetAI> BT_Set_TargetAI::Create()
{
    return make_shared<BT_Set_TargetAI>();
}

void BT_Set_TargetAI::Free()
{
    Task::Free();
}
