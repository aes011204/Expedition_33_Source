#include "Unlock_Manager.h"

#include "ClientInstance.h"
#include "CostumeManager.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "SkillTreeCom.h"
#include "Spawn_Prefab.h"
#include "Client_Constants.h"
#include "BattleEncounterProfile.h"
#include "PlayerInventory.h"

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

    // 픽토스
    constexpr char kPictosRoulette[] = "Roulette";
    constexpr char kPictosGlassCannon[] = "GlassCannon";

    constexpr const char* kBishopRewardPortalPrefab = Constants::Prefab::Portal_BishopReward;
    constexpr const wchar_t* kBishopRewardPortalLayer = L"Layer_Puzzle";
    constexpr char kBaguettePresentationId[] = "Baguette";
    constexpr char kPotatoBagPresentationId[] = "PotatoBag";

    const Vec3 kBishopRewardPortalSpawnPosition = Vec3(143.70f, 21.65f, 0.54f);
    const Vec3 kBishopRewardPortalRotation = Vec3::Zero;
    const Vec3 kBishopRewardHeroWarpPosition = Vec3(122.10f, 21.61f, -2.56f);
    const Vec3 kBishopRewardHeroWarpRotation = Vec3(0.f, 80.3f, 0.f);

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
        reward.spawnPosition = kBishopRewardPortalSpawnPosition;
        reward.useRewardOrigin = false;
        reward.spawnRotation = kBishopRewardPortalRotation;
        return reward;
    }

    Unlock_Manager::UnlockReward Make_PictosReward(const char* pictosId)
    {
        Unlock_Manager::UnlockReward reward{};
        reward.type = Unlock_Manager::RewardType::UnlockPictos;
        reward.pictosId = pictosId;
        return reward;
    }

    Unlock_Manager::UnlockReward Make_ActiveHeroWarpReward()
    {
        Unlock_Manager::UnlockReward reward{};
        reward.type = Unlock_Manager::RewardType::WarpActiveHero;
        reward.heroWarpPosition = kBishopRewardHeroWarpPosition;
        reward.heroWarpRotation = kBishopRewardHeroWarpRotation;
        return reward;
    }

    Unlock_Manager::UnlockReward Make_WeaponReward(const char* characterId, const char* weaponId)
    {
        Unlock_Manager::UnlockReward reward{};
        reward.type = Unlock_Manager::RewardType::UnlockWeapon;
        reward.characterId = characterId;
        reward.weaponId = weaponId;
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

    string Normalize_GetWeaponPresentationId(const string& characterId, const string& weaponId)
    {
        if (characterId == "Gustave")
        {
            if (weaponId == "Baguette")
                return "Gustave_Baguette";

            if (weaponId == "Tireso" || weaponId == "Candy")
                return "Gustave_Candy";

            if (weaponId == "Lanceram")
                return "Gustave_Lanceram";
        }

        if (characterId == "Lune")
        {
            if (weaponId == "Baguette")
                return "Lune_Baguette";

            if (weaponId == "Colim")
                return "Lune_Colim";

            if (weaponId == "Snowim")
                return "Lune_Snowim";
        }

        if (characterId == "Maelle")
        {
            if (weaponId == "Baguette")
                return "Maelle_Baguette";

            if (weaponId == "Medalum")
                return "Maelle_Medalum";

            if (weaponId == "Plenum" || weaponId == "Sekarum")
                return "Maelle_Sekarum";
        }

        return {};
    }

}

HRESULT Unlock_Manager::Initialize()
{
    Reset_DefaultHeroUnlocks();
    _pendingGetCharacterHeroes.clear();
    _pendingGetClothesPresetIds.clear();
    _pendingGetWeaponPresentationIds.clear();
    _pendingGetPictosPresentationIds.clear();
    _hasPendingActiveHeroWarp = false;
    _pendingActiveHeroWarpPosition = Vec3::Zero;
    _pendingActiveHeroWarpRotation = Vec3::Zero;

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

        rule.rewards.emplace_back(Make_WeaponReward("Gustave", "Baguette"));
        rule.rewards.emplace_back(Make_WeaponReward("Lune", "Baguette"));
        rule.rewards.emplace_back(Make_WeaponReward("Maelle", "Baguette"));

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
        UnlockRule rule{};
        rule.ruleId = kRuleKillDuallisteUnlockSkill;
        rule.triggerType = TriggerType::EnemyKilled;
        rule.triggerKey = kEnemyDualliste;

        rule.rewards.emplace_back(Make_PictosReward(kPictosGlassCannon));

        _rules.emplace_back(std::move(rule));
    }

    // 비숍 죽이고 포탈 생성 + 룰렛 픽토스 해금
    {
        UnlockRule rule{};
        rule.ruleId = kRuleKillBishopSpawnPortal;
        rule.triggerType = TriggerType::EnemyKilled;
        rule.triggerKey = kEnemyBishop;

        rule.rewards.emplace_back(Make_WorldSpawnReward());
        rule.rewards.emplace_back(Make_PictosReward(kPictosRoulette));
        rule.rewards.emplace_back(Make_ActiveHeroWarpReward());

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

vector<string> Unlock_Manager::Handle_BattleVictory(
    const vector<string>& enemyKeys,
    uint32 battleFieldId,
    uint32 rewardLevelIndex,
    const Vec3* rewardOrigin)
{
    vector<string> presentedPictosIds{};

    if (enemyKeys.empty())
        return presentedPictosIds;

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

        Try_CompleteRule(rule, rewardLevelIndex, rewardOrigin, &presentedPictosIds);
    }

    return presentedPictosIds;
}

bool Unlock_Manager::Is_HeroUnlocked(HeroId heroId) const
{
    return _unlockedHeroIds.find(heroId) != _unlockedHeroIds.end();
}

void Unlock_Manager::Try_CompleteRule(const UnlockRule& rule, uint32 rewardLevelIndex, const Vec3* rewardOrigin, vector<string>* outPresentedPictosIds)
{
    if (_completedRuleIds.contains(rule.ruleId))
        return;

    // rule 찾으면
    _completedRuleIds.emplace(rule.ruleId);

    const size_t beforePictosPresentationCount = _pendingGetPictosPresentationIds.size();

    // 룰 완료 순간 즉시 보상 적용
    for (const UnlockReward& reward : rule.rewards)
    {
        Apply_Reward(reward, rewardLevelIndex, rewardOrigin, true);
    }

    if (outPresentedPictosIds != nullptr)
    {
        for (size_t i = beforePictosPresentationCount; i < _pendingGetPictosPresentationIds.size(); ++i)
            outPresentedPictosIds->push_back(_pendingGetPictosPresentationIds[i]); // 이번 전투 결과창에서 새 획득 Pictos만 보여준다.
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

void Unlock_Manager::Queue_GetWeaponPresentation(const string& characterId, const string& weaponId)
{
    const string presentationId = Normalize_GetWeaponPresentationId(characterId, weaponId);
    if (presentationId.empty())
        return;

    if (find(_pendingGetWeaponPresentationIds.begin(), _pendingGetWeaponPresentationIds.end(), presentationId) !=
        _pendingGetWeaponPresentationIds.end())
    {
        return;
    }

    _pendingGetWeaponPresentationIds.emplace_back(presentationId);
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
    case RewardType::UnlockPictos:
    {
        const string& pictosId = reward.pictosId;
        if (pictosId.empty())
            break;

        const auto* pictosDesc = CLIENT->Get_PictosDesc(pictosId);
        if (pictosDesc == nullptr)
        {
            LOG_WARN("UnlockManager: invalid pictos reward id={}", pictosId);
            break;
        }

        const auto inventory = CLIENT->Get_PlayerInventory();
        if (inventory == nullptr)
            break;

        const bool alreadyUnlocked = inventory->Is_PictosUnlocked(pictosId);
        inventory->Unlock_Pictos(pictosId);

        if (!alreadyUnlocked && queuePresentation)
            Queue_GetPictosPresentation(pictosId);

        break;
    }

    case RewardType::UnlockWeapon:
    {
        if (reward.characterId.empty() || reward.weaponId.empty())
            break;

        const GameDataTable::WeaponDesc* weaponDesc =
            CLIENT->Get_WeaponDesc(reward.characterId, reward.weaponId);
        if (weaponDesc == nullptr)
        {
            LOG_WARN("UnlockManager: invalid weapon reward character={}, weapon={}",
                reward.characterId, reward.weaponId);
            break;
        }

        const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory();
        if (inventory == nullptr)
            break;

        const bool alreadyUnlocked =
            inventory->Is_WeaponUnlocked(reward.characterId, reward.weaponId);

        inventory->Unlock_Weapon(
            weaponDesc->characterId,
            weaponDesc->weaponId,
            weaponDesc->level,
            0);

        if (!alreadyUnlocked && queuePresentation)
            Queue_GetWeaponPresentation(weaponDesc->characterId, weaponDesc->weaponId);

        break;
    }

    case RewardType::WarpActiveHero:
    {
        if (!queuePresentation)
            break;

        _hasPendingActiveHeroWarp = true;
        _pendingActiveHeroWarpPosition = reward.heroWarpPosition;
        _pendingActiveHeroWarpRotation = reward.heroWarpRotation;
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

vector<string> Unlock_Manager::Consume_GetWeaponPresentationIds()
{
    vector<string> result = std::move(_pendingGetWeaponPresentationIds);
    _pendingGetWeaponPresentationIds.clear();

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

bool Unlock_Manager::Apply_SaveJson(const json& data, bool replayWorldRewards, bool queuePresentation)
{
    _completedRuleIds.clear();
    Reset_DefaultHeroUnlocks(); // 구스타브 초기화
    _pendingGetCharacterHeroes.clear();
    _pendingGetClothesPresetIds.clear();
    _pendingGetWeaponPresentationIds.clear();
    _pendingGetPictosPresentationIds.clear();
    _hasPendingActiveHeroWarp = false;
    _pendingActiveHeroWarpPosition = Vec3::Zero;
    _pendingActiveHeroWarpRotation = Vec3::Zero;

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
                    Apply_Reward(reward, UINT32_MAX, nullptr, queuePresentation);
                    continue;
                }

                if (reward.type == RewardType::UnlockHero)
                {
                    // 저장 데이터 복원 때도 해금 캐릭터를 보여주기 위해 que에 true 세팅. 나중에 무기도 UI 띄우기
                    Apply_Reward(reward, UINT32_MAX, nullptr, queuePresentation);
                    continue;
                }

                if (reward.type == RewardType::UnlockPictos)
                {
                    // 완료된 룰을 복원할 때도 픽토스 인벤토리 상태를 맞춘다.
                    Apply_Reward(reward, UINT32_MAX, nullptr, queuePresentation);
                    continue;
                }

                if (reward.type == RewardType::UnlockWeapon)
                {
                    // 완료된 룰을 복원할 때도 무기 인벤토리 상태를 맞춘다.
                    Apply_Reward(reward, UINT32_MAX, nullptr, queuePresentation);
                    continue;
                }

                if (replayWorldRewards && reward.type == RewardType::SpawnWorldObject)
                {
                    Apply_Reward(reward, UINT32_MAX, nullptr, queuePresentation);
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

            Apply_Reward(reward, UINT32_MAX, nullptr, queuePresentation);
        }
    }

    return true;
}

vector<string> Unlock_Manager::Consume_GetPictosPresentationIds()
{
    vector<string> result = std::move(_pendingGetPictosPresentationIds);
    _pendingGetPictosPresentationIds.clear();

    return result;
}

void Unlock_Manager::Queue_GetPictosPresentation(const string& pictosId)
{
    if (pictosId.empty())
        return;

    const GameDataTable::PictosDesc* pictosDesc = CLIENT->Get_PictosDesc(pictosId);
    if (pictosDesc == nullptr || pictosDesc->getItemUi.empty())
        return;

    if (find(_pendingGetPictosPresentationIds.begin(), _pendingGetPictosPresentationIds.end(), pictosId) !=
        _pendingGetPictosPresentationIds.end())
    {
        return;
    }

    _pendingGetPictosPresentationIds.emplace_back(pictosId);
}

bool Unlock_Manager::Consume_PendingActiveHeroWarp(Vec3& outPosition, Vec3& outRotation)
{
    if (!_hasPendingActiveHeroWarp)
        return false;

    outPosition = _pendingActiveHeroWarpPosition;
    outRotation = _pendingActiveHeroWarpRotation;

    _hasPendingActiveHeroWarp = false;
    _pendingActiveHeroWarpPosition = Vec3::Zero;
    _pendingActiveHeroWarpRotation = Vec3::Zero;
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
    _pendingGetWeaponPresentationIds.clear();
    _pendingGetPictosPresentationIds.clear();
    _hasPendingActiveHeroWarp = false;
    _pendingActiveHeroWarpPosition = Vec3::Zero;
    _pendingActiveHeroWarpRotation = Vec3::Zero;

    Base::Free();
}
