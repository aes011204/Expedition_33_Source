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
    };

    struct UnlockReward
    {
        RewardType  type = RewardType::UnlockCostumePreset;

        // 코스튬 해금용
        HeroId      heroId = HeroId::END;
        string      presetId{};

        // 스킬 해금용 - 그래디언트
        string      skillId{};

        // 월드 오브젝트 스폰용. 이거 필요할지?
        LevelType   targetLevel = LevelType::MapTest;
        wstring     layerTag{};
        string      prefabName{};
        Vec3        spawnPosition = Vec3::Zero;
        bool        useRewardOrigin = false;
        Vec3        spawnOffsetFromOrigin = Vec3::Zero;
        Vec3        spawnRotation = Vec3::Zero;
    };

    struct UnlockRule
    {
        string ruleId{};
        TriggerType triggerType = TriggerType::EnemyKilled;
        string triggerKey{}; // MB_Mime, MB_PotatoBag 등

        vector<UnlockReward> rewards{};
    };

public:
    HRESULT Initialize();

    // 전투 승리 후 넘겨준 적 prefab 키 목록 기준으로 해금 처리
    void Handle_BattleVictory(
        const vector<string>& enemyKeys,
        uint32 rewardLevelIndex = UINT32_MAX,
        const Vec3* rewardOrigin = nullptr);
    bool Is_Completed(const string& ruleId) const;

    // 완료 룰 ID로 저장 -> 불러오기까지 구현하려고
    json Create_SaveJson() const;

    // 체크포인트 복원 시에는 완료 상태만 되살리고, 포탈같은건 worldObjects 쪽으로
    bool Apply_SaveJson(const json& data, bool replayWorldRewards);

private:
    void Register_DefaultRules();
    void Try_CompleteRule(const UnlockRule& rule, uint32 rewardLevelIndex, const Vec3* rewardOrigin);

    void Apply_Reward(
        const UnlockReward& reward,
        uint32 rewardLevelIndex = UINT32_MAX,
        const Vec3* rewardOrigin = nullptr);
    const UnlockRule* Find_Rule(const string& ruleId) const;

private:
    vector<UnlockRule> _rules{};
    uset<string> _completedRuleIds{};

public:
    static Unique<Unlock_Manager> Create();
    void Free() override;

};

NS_END
