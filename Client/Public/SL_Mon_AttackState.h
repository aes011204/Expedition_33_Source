#pragma once

#include "SL_Mon_State.h"

NS_BEGIN(Engine)
class ModelCom;
class MovementCom;
NS_END

NS_BEGIN(Client)

class SL_Mon_AttackState final : public SL_Mon_State
{
public:
    explicit SL_Mon_AttackState(const Shared<SL_Dualliste>& owner);
    ~SL_Mon_AttackState() override = default;

public:
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;
    void Cache_PlayerAnchor(const Vec3& playerPosition, const Vec3& playerRight, const Vec3& playerUp, const Vec3& playerForward);
    bool Try_GetCachedPlayerAnchor(Vec3& outPlayerPosition, Vec3& outPlayerRight, Vec3& outPlayerUp, Vec3& outPlayerForward) const;
    void Clear_CachedPlayerAnchor();

private:
    Shared<MovementCom> _movementCom{};
    Shared<ModelCom> _modelCom{};
    Vec3 _cachedPlayerPos = Vec3::Zero;
    Vec3 _cachedPlayerRight = Vec3::Right;
    Vec3 _cachedPlayerUp = Vec3::Up;
    Vec3 _cachedPlayerForward = Vec3::Look;
    bool _hasCachedPlayerAnchor = false;
public:
    static Shared<SL_Mon_AttackState> Create(const Shared<SL_Dualliste>& owner);
};

NS_END
