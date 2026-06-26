#pragma once

#include "Base.h"
#include "Client_Defines.h"

#include <optional>

NS_BEGIN(Client)

class Player_BattleBase;
class SL_Base;
class SL_Dualliste;

class SL_DuallisteCloneSkillManager : public Base
{
private:
    struct CloneSkillRuntime
    {
        Weak<Player_BattleBase> actor{};
        Weak<SL_Base> target{};
        uint32 levelIndex = 0;

        string prefabName{};
        string skillKey{};
        string animationName{};

        float fallbackDamageMultiplier = 1.f;
        float animationSpeed = 1.f;
        bool  attackApplied = false;
    };

public:
    void Update(float timeDelta);
    void Reset();

    bool Request_CloneSkill(
        const string& prefabName,
        const string& skillKey,
        const string& animationName,
        const Shared<SL_Base>& target,
        float fallbackDamageMultiplier = 1.f,
        float animationSpeed = 1.f);

    bool Notify_Attack(GameObject* notifyOwner);

private:
    HRESULT Initialize(const Shared<SL_Dualliste>& owner);

    bool Begin_CloneSkill(CloneSkillRuntime runtime);
    void Update_ActiveClone(float timeDelta);
    void Finish_ActiveClone();

    Shared<Player_BattleBase> Spawn_CloneActor(const CloneSkillRuntime& runtime);
    bool Play_CloneAnimation(const Shared<Player_BattleBase>& actor, const string& animationName, float animationSpeed);
    bool Is_NotifyOwnerActiveClone(GameObject* notifyOwner) const;
    void Apply_CloneDamage(const CloneSkillRuntime& runtime);

    Vec3 Resolve_SpawnPosition(const Shared<SL_Dualliste>& owner, const Shared<SL_Base>& target) const;
    Vec3 Resolve_SpawnRotation(const Vec3& spawnPosition, const Shared<SL_Base>& target) const;

private:
    Weak<SL_Dualliste> _owner{};
    std::optional<CloneSkillRuntime> _activeClone{};

public:
    static Shared<SL_DuallisteCloneSkillManager> Create(const Shared<SL_Dualliste>& owner);
    void Free() override;
};

NS_END
