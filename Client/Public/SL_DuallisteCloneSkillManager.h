#pragma once

#include "Base.h"
#include "Client_Defines.h"

NS_BEGIN(Client)

class SL_Base;
class SL_Dualliste;

class SL_DuallisteCloneSkillManager : public Base
{
private:
    struct CloneSkillRuntime
    {
        Weak<GameObject> actor{};
        Weak<SL_Base> target{};
        uint32 levelIndex = 0;

        string prefabName{};
        string skillKey{};
        string animationName{};
        Vec3   spawnOffset = Vec3::Zero;
        Vec3   spawnWorldPos = Vec3::Zero;
        Vec3   goalWorldPos = Vec3::Zero;

        float goalOffset = 1.8f;
        float fallbackDamageMultiplier = 1.f;
        float animationSpeed = 1.f;
        float moveSpeed = 0.f;
        float dissolveDuration = 0.35f;
        bool  attackStarted = false;
        bool  hasSpawnWorldPos = false;
        bool  hasGoalWorldPos = false;
        bool  attackApplied = false;
        bool  isDissolving = false;
        float dissolveElapsed = 0.f;
    };

public:
    void Update(float timeDelta);
    void Reset();

    bool Request_CloneSkill(
        const string& prefabName,
        const string& skillKey,
        const string& animationName,
        const Shared<SL_Base>& target,
        const Vec3& spawnOffset = Vec3::Zero,
        float goalOffset = 1.8f,
        float fallbackDamageMultiplier = 1.f,
        float animationSpeed = 1.f,
        float moveSpeed = 0.f,
        float dissolveDuration = 0.35f);

    bool Notify_Attack(GameObject* notifyOwner);

private:
    HRESULT Initialize(const Shared<SL_Dualliste>& owner);

    bool Begin_CloneSkill(CloneSkillRuntime runtime);
    void Update_ActiveClones(float timeDelta);
    void Finish_Clone(CloneSkillRuntime& runtime);

    Shared<GameObject> Spawn_CloneActor(const CloneSkillRuntime& runtime);
    bool Play_CloneAnimation(const Shared<GameObject>& actor, const string& animationName, float animationSpeed);
    CloneSkillRuntime* Find_CloneByNotifyOwner(GameObject* notifyOwner);
    void Apply_CloneDamage(const CloneSkillRuntime& runtime);
    bool Try_ResolveSpawnWorldPosition(CloneSkillRuntime& runtime) const;
    bool Try_ResolveGoalWorldPosition(CloneSkillRuntime& runtime) const;
    bool Update_CloneMovement(CloneSkillRuntime& runtime, float timeDelta) const;

    Vec3 Resolve_SpawnPosition(const Shared<SL_Dualliste>& owner, const Vec3& spawnOffset) const;
    Vec3 Resolve_SpawnRotation(const Vec3& spawnPosition, const Shared<SL_Base>& target) const;

private:
    static constexpr size_t kMaxActiveClones = 3;
    Weak<SL_Dualliste> _owner{};
    vector<CloneSkillRuntime> _activeClones{};

public:
    static Shared<SL_DuallisteCloneSkillManager> Create(const Shared<SL_Dualliste>& owner);
    void Free() override;
};

NS_END
