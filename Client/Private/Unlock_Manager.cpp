#include "Unlock_Manager.h"

#include "ClientInstance.h"
#include "CostumeManager.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "SkillTreeCom.h"
#include "Spawn_Prefab.h"
#include "Client_Constants.h"
#include "BattleEncounterProfile.h"

namespace
{
    // 룰 ID
    constexpr char kRuleKillMimeUnlockBaguette[] = "kill_mime_unlock_baguette_costume";
    constexpr char kRuleKillPotatoUnlockPotato[] = "kill_potatobag_unlock_potato_costume";
    constexpr char kRuleKillDuallisteUnlockSkill[] = "kill_dualliste_unlock_gradient_skill";
    constexpr char kRuleKillBishopSpawnPortal[] = "kill_bishop_spawn_portal";
    constexpr char kRuleKillTWLancelier[] = "kill_tw_lancelier_unlock_party";

    // 실제 적 키.
    constexpr const char* kEnemyMime = Constants::Prefab::MB_Mime;
    constexpr const char* kEnemyPotatoBag = Constants::Prefab::MB_PotatoBag;
    constexpr const char* kEnemyBishop = Constants::Prefab::MB_Bishop;

    // 얘는 일단 아직 안씀
    constexpr char kEnemyDualliste[] = "SL_Dualliste";

    // TODO : 그래디언트

    constexpr const char* kBishopRewardPortalPrefab = Constants::Prefab::Portal_BishopReward;
    constexpr const wchar_t* kBishopRewardPortalLayer = L"Layer_Puzzle";
    constexpr char kBaguettePresentationId[] = "Baguette";
    constexpr char kPotatoBagPresentationId[] = "PotatoBag";

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

    string Normalize_GetClothesPresentationId(const string& presetId)
    {
        if (presetId == "Baguette")
            return kBaguettePresentationId;

        if (presetId == "Potato" || presetId == "PotatoBag")
            return kPotatoBagPresentationId;

        return {};
    }

}

HRESULT Unlock_Manager::Initialize()
{
    Reset_DefaultHeroUnlocks();
    _pendingGetCharacterHeroes.clear();
    _pendingGetClothesPresetIds.clear();

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
    // TODO : 쌍검투사 -> 그래디언트 열기
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

    // 튜토리얼 랜서러
    {
        UnlockRule rule{};
        rule.ruleId = kRuleKillTWLancelier;
        rule.triggerType = TriggerType::EnemyKilled;
        rule.triggerKey = Constants::Prefab::MB_Lancelier;
        rule.triggerBattleFieldId = BattleEncounterIds::LancelierTutorial; // TW_Lancelier 전투 99번에서만 발동한다.

        // 루네 해금
        UnlockReward luneReward{};
        luneReward.type = RewardType::UnlockHero;
        luneReward.heroId = HeroId::Lune;
        rule.rewards.emplace_back(luneReward);

        // 마엘 해금
        UnlockReward maelleReward{};
        maelleReward.type = RewardType::UnlockHero;
        maelleReward.heroId = HeroId::Maelle;
        rule.rewards.emplace_back(maelleReward);

        _rules.emplace_back(std::move(rule));
    }

}

void Unlock_Manager::Handle_BattleVictory(
    const vector<string>& enemyKeys,
    uint32 battleFieldId,
    uint32 rewardLevelIndex,
    const Vec3* rewardOrigin)
{
    if (enemyKeys.empty())
        return;

    for (const UnlockRule& rule : _rules)
    {
        if (Is_Completed(rule.ruleId))
            continue;

        if (rule.triggerType != TriggerType::EnemyKilled)
            continue;

        // TW_ 와의 전투 -> BattleField 99에서만 발동
        if (rule.triggerBattleFieldId != 0 && rule.triggerBattleFieldId != battleFieldId)
            continue;

        if (find(enemyKeys.begin(), enemyKeys.end(), rule.triggerKey) == enemyKeys.end())
            continue;

        Try_CompleteRule(rule, rewardLevelIndex, rewardOrigin);
    }
}

bool Unlock_Manager::Is_HeroUnlocked(HeroId heroId) const
{
    return _unlockedHeroIds.find(heroId) != _unlockedHeroIds.end();
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
        Apply_Reward(reward, rewardLevelIndex, rewardOrigin, true);
    }

    LOG_INFO("UnlockManager: completed rule={}", rule.ruleId);
}

void Unlock_Manager::Reset_DefaultHeroUnlocks()
{
    _unlockedHeroIds.clear();
    _unlockedHeroIds.emplace(HeroId::Gustave); // 리셋해도 처음에는 구스타브 등록
}

void Unlock_Manager::Queue_GetClothesPresentation(const string& presetId)
{
    const string presentationId = Normalize_GetClothesPresentationId(presetId);
    if (presentationId.empty())
        return;

    if (find(_pendingGetClothesPresetIds.begin(), _pendingGetClothesPresetIds.end(), presentationId) !=
        _pendingGetClothesPresetIds.end())
    {
        return;
    }

    _pendingGetClothesPresetIds.emplace_back(presentationId);
}

void Unlock_Manager::Apply_Reward(
    const UnlockReward& reward,
    uint32 rewardLevelIndex,
    const Vec3* rewardOrigin,
    bool queuePresentation)
{
    switch (reward.type)
    {
    case RewardType::UnlockCostumePreset:
    {
        CostumeManager* coustumeManager = CLIENT->Get_CostumeManager();
        if (coustumeManager)
        {
            const bool hasCostumePreset = coustumeManager->Get_Costume(reward.heroId, reward.presetId) != nullptr;
            coustumeManager->Unlock_Preset(reward.heroId, reward.presetId);

            if (hasCostumePreset && queuePresentation)
                Queue_GetClothesPresentation(reward.presetId);
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

    case RewardType::UnlockHero:
    {
        if (reward.heroId == HeroId::END)
            break;

        // reward.heroId가 set에 없어서 이번에 새로 들어갔으면 true, 이미 set에 있었으면 삽입 안되고 false가 나옴
        const bool inserted = _unlockedHeroIds.emplace(reward.heroId).second;
        if (inserted && queuePresentation)
        {
            _pendingGetCharacterHeroes.emplace_back(reward.heroId);
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

vector<HeroId> Unlock_Manager::Consume_GetCharacterHeroes()
{
    vector<HeroId> result = std::move(_pendingGetCharacterHeroes);
    _pendingGetCharacterHeroes.clear();

    return result;
}

vector<string> Unlock_Manager::Consume_GetClothesPresetIds()
{
    vector<string> result = std::move(_pendingGetClothesPresetIds);
    _pendingGetClothesPresetIds.clear();

    return result;
}

json Unlock_Manager::Create_SaveJson() const
{
    json root = json::object();
    root["completed_rule_ids"] = json::array();

    for (const string& ruleId : _completedRuleIds)
    {
        root["completed_rule_ids"].emplace_back(ruleId);
    }

    root["unlocked_hero_ids"] = json::array();
    for (HeroId heroId : _unlockedHeroIds)
    {
        root["unlocked_hero_ids"].emplace_back(static_cast<int>(heroId));
    }

    return root;
}

bool Unlock_Manager::Apply_SaveJson(const json& data, bool replayWorldRewards)
{
    _completedRuleIds.clear();
    Reset_DefaultHeroUnlocks(); // 구스타브 초기화
    _pendingGetCharacterHeroes.clear();
    _pendingGetClothesPresetIds.clear();

    if (!data.is_object())
        return true;

    const auto completedIt = data.find("completed_rule_ids");
    if (completedIt != data.end() && completedIt->is_array())
    {
        for (const json& entry : *completedIt)
        {
            if (!entry.is_string())
                continue;

            const string ruleId = entry.get<string>();
            _completedRuleIds.emplace(ruleId);

            const UnlockRule* rule = Find_Rule(ruleId);
            if (rule == nullptr)
                continue;

            for (const UnlockReward& reward : rule->rewards)
            {
                if (reward.type == RewardType::UnlockCostumePreset)
                {
                    // 저장 데이터 복원 때도 의상 획득 UI를 보여주기 위해 presentation queue에 올린다.
                    Apply_Reward(reward, UINT32_MAX, nullptr, true);
                    continue;
                }

                if (reward.type == RewardType::UnlockHero)
                {
                    // 저장 데이터 복원 때도 해금 캐릭터를 보여주기 위해 que에 true 세팅. 나중에 무기도 UI 띄우기
                    Apply_Reward(reward, UINT32_MAX, nullptr, true);
                    continue;
                }

                if (replayWorldRewards && reward.type == RewardType::SpawnWorldObject)
                {
                    Apply_Reward(reward, UINT32_MAX, nullptr, false);
                    continue;
                }
            }
        }
    }

    const auto heroIt = data.find("unlocked_hero_ids");
    if (heroIt != data.end() && heroIt->is_array())
    {
        for (const json& entry : *heroIt)
        {
            if (!entry.is_number_integer())
                continue;

            UnlockReward reward{};
            reward.type = RewardType::UnlockHero;
            reward.heroId = static_cast<HeroId>(entry.get<int>());

            Apply_Reward(reward, UINT32_MAX, nullptr, true);
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
    _unlockedHeroIds.clear();
    _pendingGetCharacterHeroes.clear();
    _pendingGetClothesPresetIds.clear();

    Base::Free();
}
