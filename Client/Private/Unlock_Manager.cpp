#include "Unlock_Manager.h"

#include "ClientInstance.h"
#include "CostumeManager.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "SkillTreeCom.h"
#include "Spawn_Prefab.h"
#include "Client_Constants.h"

namespace
{
	// 룰 ID
	constexpr char kRuleKillMimeUnlockBaguette[] = "kill_mime_unlock_baguette_costume";
	constexpr char kRuleKillPotatoUnlockPotato[] = "kill_potatobag_unlock_potato_costume";
	constexpr char kRuleKillDuallisteUnlockSkill[] = "kill_dualliste_unlock_gradient_skill";
	constexpr char kRuleKillBishopSpawnPortal[] = "kill_bishop_spawn_portal";

	// 실제 적 키.
	constexpr const char* kEnemyMime = Constants::Prefab::MB_Mime;
	constexpr const char* kEnemyPotatoBag = Constants::Prefab::MB_PotatoBag;
	constexpr const char* kEnemyBishop = Constants::Prefab::MB_Bishop;

	// 얘는 일단 아직 안씀
	constexpr char kEnemyDualliste[] = "SL_Dualliste";

	// TODO : 그래디언트

	constexpr const char* kBishopRewardPortalPrefab = Constants::Prefab::Portal_BishopReward;
	constexpr const wchar_t* kBishopRewardPortalLayer = L"Layer_Puzzle";

	// 소스 몬스터 위치를 못 받을 때만 쓰는 fallback 좌표.
	const Vec3 kBishopRewardPortalFallbackPosition = Vec3(-87.f, 19.47f, -118.f);
	const Vec3 kBishopRewardPortalOffsetFromBishop = Vec3(0.f, 0.f, 3.f);
	const Vec3 kBishopRewardPortalRotation = Vec3::Zero;

	Unlock_Manager::UnlockReward Make_CostumeReward(HeroId heroId, const char* presetId)
	{
		Unlock_Manager::UnlockReward reward{};
		reward.type = Unlock_Manager::RewardType::UnlockCostumePreset;
		reward.heroId = heroId;
		reward.presetId = presetId;

		return reward;
	}

	Unlock_Manager::UnlockReward Make_SkillReward(HeroId heroId, const char* skillId)
	{
		Unlock_Manager::UnlockReward reward{};
		reward.type = Unlock_Manager::RewardType::UnlockSkill;
		reward.heroId = heroId;
		reward.skillId = skillId;
		return reward;
	}

	Unlock_Manager::UnlockReward Make_WorldSpawnReward()
	{
		Unlock_Manager::UnlockReward reward{};
		reward.type = Unlock_Manager::RewardType::SpawnWorldObject;
		reward.targetLevel = LevelType::GamePlay;
		reward.layerTag = kBishopRewardPortalLayer;
		reward.prefabName = kBishopRewardPortalPrefab;
		reward.spawnPosition = kBishopRewardPortalFallbackPosition;
		reward.useRewardOrigin = true;
		reward.spawnOffsetFromOrigin = kBishopRewardPortalOffsetFromBishop;
		reward.spawnRotation = kBishopRewardPortalRotation;
		return reward;
	}

}

HRESULT Unlock_Manager::Initialize()
{
	Register_DefaultRules();

	return S_OK;
}

void Unlock_Manager::Register_DefaultRules()
{
	_rules.clear();

	// 바게트 해금
	{
		UnlockRule rule{};
		rule.ruleId = kRuleKillMimeUnlockBaguette;
		rule.triggerType = TriggerType::EnemyKilled;
		rule.triggerKey = kEnemyMime;

		rule.rewards.emplace_back(Make_CostumeReward(HeroId::Gustave, "Baguette"));
		rule.rewards.emplace_back(Make_CostumeReward(HeroId::Lune, "Baguette"));
		rule.rewards.emplace_back(Make_CostumeReward(HeroId::Maelle, "Baguette"));

		_rules.emplace_back(std::move(rule));
	}
	// 감자 해금
	{
		UnlockRule rule{};
		rule.ruleId = kRuleKillPotatoUnlockPotato;
		rule.triggerType = TriggerType::EnemyKilled;
		rule.triggerKey = kEnemyPotatoBag;

		rule.rewards.emplace_back(Make_CostumeReward(HeroId::Gustave, "Potato"));
		rule.rewards.emplace_back(Make_CostumeReward(HeroId::Lune, "Potato"));
		rule.rewards.emplace_back(Make_CostumeReward(HeroId::Maelle, "Potato"));

		_rules.emplace_back(std::move(rule));
	}
	// TODO : 쌍검
	{

	}

	// 비숍 죽이고 포탈 생성
	{
		UnlockRule rule{};
		rule.ruleId = kRuleKillBishopSpawnPortal;
		rule.triggerType = TriggerType::EnemyKilled;
		rule.triggerKey = kEnemyBishop;
		rule.rewards.emplace_back(Make_WorldSpawnReward());
		_rules.emplace_back(std::move(rule));
	}

}

void Unlock_Manager::Handle_BattleVictory(
	const vector<string>& enemyKeys,
	uint32 rewardLevelIndex,
	const Vec3* rewardOrigin)
{
	if (enemyKeys.empty())
		return;

	uset<string> uniqueKeys(enemyKeys.begin(), enemyKeys.end());

	for (const UnlockRule& rule : _rules)
	{
		if (_completedRuleIds.contains(rule.ruleId))
			continue;

		if (rule.triggerType != TriggerType::EnemyKilled)
			continue;

		if (!uniqueKeys.contains(rule.triggerKey))
			continue;

		Try_CompleteRule(rule, rewardLevelIndex, rewardOrigin);
	}
}

void Unlock_Manager::Try_CompleteRule(const UnlockRule& rule, uint32 rewardLevelIndex, const Vec3* rewardOrigin)
{
	if (_completedRuleIds.contains(rule.ruleId))
		return;

	// rule 찾으면
	_completedRuleIds.emplace(rule.ruleId);

	// 룰 완료 순간 즉시 보상 적용
	for (const UnlockReward& reward : rule.rewards)
	{
		Apply_Reward(reward, rewardLevelIndex, rewardOrigin);
	}

	LOG_INFO("UnlockManager: completed rule={}", rule.ruleId);
}

void Unlock_Manager::Apply_Reward(
	const UnlockReward& reward,
	uint32 rewardLevelIndex,
	const Vec3* rewardOrigin)
{
	switch (reward.type)
	{
	case RewardType::UnlockCostumePreset:
	{
		CostumeManager* coustumeManager = CLIENT->Get_CostumeManager();
		if (coustumeManager)
		{
			coustumeManager->Unlock_Preset(reward.heroId, reward.presetId);
		}

		break;
	}

	case RewardType::UnlockSkill:
	{
		const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(reward.heroId);
		if (hero == nullptr)
			break;

		const Shared<SkillTreeCom> skillTree = hero->Get_Component<SkillTreeCom>();
		if (skillTree != nullptr)
			skillTree->ForceUnlock(reward.skillId);
		break;
	}

	case RewardType::SpawnWorldObject:
	{
		const bool hasRewardLevelOverride = rewardLevelIndex != UINT32_MAX;
		const uint32 spawnLevelIndex =
			hasRewardLevelOverride ? rewardLevelIndex : ETOI(reward.targetLevel);

		if (!hasRewardLevelOverride &&
			reward.targetLevel != static_cast<LevelType>(GAME->Current_LevelIndex()))
		{
			break;
		}

		Vec3 spawnPosition = reward.spawnPosition;
		if (reward.useRewardOrigin && rewardOrigin != nullptr)
			spawnPosition = *rewardOrigin + reward.spawnOffsetFromOrigin;

		const Shared<GameObject> spawned = Spawn_Prefab::Prefab(reward.prefabName)
			.AtLevel(spawnLevelIndex)
			.InLayer(reward.layerTag)
			.Position(spawnPosition)
			.Rotation(reward.spawnRotation)
			.Spawn();

		if (spawned != nullptr)
		{
			spawned->Set_RuntimeAuthoringLoaded(true); // 체크포인트 캡처 대상 유지
			LOG_INFO("UnlockManager: spawned reward prefab={}, level={}, position=({}, {}, {})",
				reward.prefabName, spawnLevelIndex, spawnPosition.x, spawnPosition.y, spawnPosition.z);
		}
		else
		{
			LOG_WARN("UnlockManager: failed to spawn reward prefab={}, level={}",
				reward.prefabName, spawnLevelIndex);
		}

		break;
	}

	default:;
	}
}

bool Unlock_Manager::Is_Completed(const string& ruleId) const
{
	return _completedRuleIds.contains(ruleId);
}

json Unlock_Manager::Create_SaveJson() const
{
	json root = json::object();
	root["completed_rule_ids"] = json::array();

	for (const string& ruleId : _completedRuleIds)
	{
	    root["completed_rule_ids"].emplace_back(ruleId);
	}

	return root;
}

bool Unlock_Manager::Apply_SaveJson(const json& data, bool replayWorldRewards)
{
	_completedRuleIds.clear();

    if (!data.is_object())
        return true;

    const auto it = data.find("completed_rule_ids");
    if (it == data.end() || !it->is_array())
        return true;

    for (const json& entry : *it)
    {
        if (!entry.is_string())
            continue;

        const string ruleId = entry.get<string>();
        _completedRuleIds.emplace(ruleId);

        // 체크포인트 복원 시에는 false로 들어온다.
        // 나중에 "세이브파일 -> 레벨 진입" 파이프라인이 생기면 true로 재생 가능.
        if (!replayWorldRewards)
            continue;

        const UnlockRule* rule = Find_Rule(ruleId);
        if (rule == nullptr)
            continue;

        for (const UnlockReward& reward : rule->rewards)
        {
            if (reward.type == RewardType::SpawnWorldObject)
                Apply_Reward(reward);
        }
    }

    return true;
}

const Unlock_Manager::UnlockRule* Unlock_Manager::Find_Rule(const string& ruleId) const
{
	for (const UnlockRule& rule : _rules)
	{
        if (rule.ruleId == ruleId)
        {
            return &rule;
        }
	}

	return nullptr;
}

Unique<Unlock_Manager> Unlock_Manager::Create()
{
	auto instance = make_unique<Unlock_Manager>();

    if (FAILED(instance->Initialize()))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void Unlock_Manager::Free()
{
	_rules.clear();
    _completedRuleIds.clear();

    Base::Free();
}
