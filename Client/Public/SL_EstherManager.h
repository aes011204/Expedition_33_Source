#pragma once

#include "Base.h"
#include "Client_Defines.h"

NS_BEGIN(Client)

class Player_BattleBase;
class SL_Base;
class SL_Maelle;
class SoulLikeController;

enum class SL_EstherType
{
    GustaveOvercharge,
    LuneImmolation,
};

class SL_EstherManager : public Base
{
private:
    struct EstherRuntime
    {
        SL_EstherType type = SL_EstherType::GustaveOvercharge;
        Weak<Player_BattleBase> actor{};
        Weak<SL_Base> target{};
        uint32 levelIndex = 0;
        string skillKey{};
        string animationName{};
        Vec3 targetAnchorWorldPosition = Vec3::Zero;

        float animationSpeed = 1.f;
        float previousAnimationTimeSec = 0.f;
        uint32 expectedAttackCount = 1;
        uint32 appliedAttackCount = 0;
        bool  luneChargeEffectPlayed = false;
        bool  luneMeteorEffectPlayed = false;
        bool  luneBeamEffectPlayed = false;
        bool  luneFinalExplosionEffectPlayed = false;
        bool  targetSlowApplied = false;
    };

public:
    void    Update(float timeDelta);
    void    Reset();
    bool    Request_Esther(SL_EstherType estherType);
    bool    Notify_Attack(GameObject* notifyOwner);
    void    Add_GaugeFromParry();
    float   Get_GaugeRatio() const;
    bool    Is_GaugeFull() const;

    void    Fill_GaugeForDebug();

private:
    HRESULT Initialize(const Shared<SoulLikeController>& controller);

    bool    Begin_Esther(SL_EstherType estherType);
    void    Update_ActiveEsther(float timeDelta);
    void    Finish_ActiveEsther();

    Shared<SL_Base> Resolve_Target() const;
    Shared<Player_BattleBase> Spawn_EstherActor(SL_EstherType estherType, const Shared<SL_Base>& target);

    bool Play_EstherAnimation(const Shared<Player_BattleBase>& actor, const string& animationName, float animationSpeed);
    bool Is_NotifyOwnerActiveEsther(GameObject* notifyOwner) const;
    bool Apply_EstherDamage(const EstherRuntime& runtime, float damageRatio, bool applyBreak);
    void Apply_TargetSlow(EstherRuntime& runtime);
    void Restore_TargetSlow(EstherRuntime& runtime);

    Vec3 Resolve_SpawnPosition(SL_EstherType estherType, const Shared<SL_Maelle>& player, const Shared<SL_Base>& target) const;
    Vec3 Resolve_SpawnRotation(const Vec3& spawnPosition, const Shared<SL_Base>& target) const;

private:
    Weak<SoulLikeController> _controller{};
    optional<EstherRuntime> _activeEsther{};
    float _estherGauge = 0.f;
    float _displayEstherGauge = 0.f;

public:
    static Shared<SL_EstherManager> Create(const Shared<SoulLikeController>& owner);
    void Free() override;


};

NS_END
