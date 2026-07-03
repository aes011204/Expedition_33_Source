#pragma once

#include "Base.h"

NS_BEGIN(Client)

class Unlock_Manager : public Base
{
public:
    enum class TriggerType
    {
        EnemyKilled,
    };

    enum class RewardType
    {
        UnlockCostumePreset,
        UnlockSkill,
        SpawnWorldObject,
        UnlockHero,
        UnlockPictos,
        UnlockWeapon,
        WarpActiveHero,
    };

    struct UnlockReward
    {
        RewardType  type = RewardType::UnlockCostumePreset;

        // 코스튬 해금용
        HeroId      heroId = HeroId::END;
        string      presetId{};

        // 스킬 해금용 - 그래디언트
        string      skillId{};

        // 픽토스 해금용
        string      pictosId{};

        // 무기 해금용
        string      characterId{};
        string      weaponId{};

        // 월드 오브젝트 스폰용. 이거 필요할지?
        LevelType   targetLevel = LevelType::MapTest;
        wstring     layerTag{};
        string      prefabName{};
        Vec3        spawnPosition = Vec3::Zero;
        bool        useRewardOrigin = false;
        Vec3        spawnOffsetFromOrigin = Vec3::Zero;
        Vec3        spawnRotation = Vec3::Zero;

        // 액티브 히어로 워프용. 전투 승리 확인 후 월드 UI에서 1회 소비한다.
        Vec3        heroWarpPosition = Vec3::Zero;
        Vec3        heroWarpRotation = Vec3::Zero;
    };

    struct UnlockRule
    {
        string ruleId{};
        TriggerType triggerType = TriggerType::EnemyKilled;
        string triggerKey{};             // MB_Mime, MB_PotatoBag 등
        uint32 triggerBattleFieldId = 0; // 0이면 적 프리펩 이름만 검사

        vector<UnlockReward> rewards{};
    };

public:
    HRESULT Initialize();

    // 전투 승리 후 넘겨준 적 prefab 키 목록 기준으로 해금 처리
    vector<string> Handle_BattleVictory(
        const vector<string>& enemyKeys,
        uint32 battleFieldId,
        uint32 rewardLevelIndex = UINT32_MAX,
        const Vec3* rewardOrigin = nullptr);

    bool Is_HeroUnlocked(HeroId heroId) const;
    bool Is_Completed(const string& ruleId) const;

    vector<HeroId> Consume_GetCharacterHeroes();
    vector<string> Consume_GetClothesPresetIds();
    vector<string> Consume_GetWeaponPresentationIds();
    void Queue_GetWeaponPresentation(const string& characterId, const string& weaponId);

public:
    // 완료 룰 ID로 저장 -> 불러오기까지 구현하려고
    json Create_SaveJson() const;

    // 체크포인트 복원 시에는 완료 상태만 되살리고, 포탈같은건 worldObjects 쪽으로
    bool Apply_SaveJson(const json& data, bool replayWorldRewards, bool queuePresentation = true);

    vector<string> Consume_GetPictosPresentationIds();
    void Queue_GetPictosPresentation(const string& pictosId);
    bool Consume_PendingActiveHeroWarp(Vec3& outPosition, Vec3& outRotation);

private:
    void Register_DefaultRules();
    void Try_CompleteRule(const UnlockRule& rule, uint32 rewardLevelIndex, const Vec3* rewardOrigin, vector<string>* outPresentedPictosIds = nullptr);

    void Reset_DefaultHeroUnlocks();
    void Queue_GetClothesPresentation(const string& presetId);

    void Apply_Reward(
        const UnlockReward& reward,
        uint32 rewardLevelIndex = UINT32_MAX,
        const Vec3* rewardOrigin = nullptr,
        bool queuePresentation = true);

    const UnlockRule* Find_Rule(const string& ruleId) const;

private:
    vector<UnlockRule> _rules{};
    uset<string> _completedRuleIds{};

    uset<HeroId> _unlockedHeroIds{};
    vector<HeroId> _pendingGetCharacterHeroes{};
    vector<string> _pendingGetClothesPresetIds{};
    vector<string> _pendingGetWeaponPresentationIds{};
    vector<string> _pendingGetPictosPresentationIds{};
    bool _hasPendingActiveHeroWarp = false;
    Vec3 _pendingActiveHeroWarpPosition = Vec3::Zero;
    Vec3 _pendingActiveHeroWarpRotation = Vec3::Zero;

public:
    static Unique<Unlock_Manager> Create();
    void Free() override;

};

NS_END
