#pragma once

#include "SL_State.h"

NS_BEGIN(Client)

class SL_Dodge final : public SL_State
{
public:
    explicit SL_Dodge(const Shared<SL_Maelle>& owner);
    ~SL_Dodge() override = default;

public:
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void ExitInvoke() override;
    bool Request_Move(const SL_MoveRequest& request, float timeDelta) override;

public:
    void Set_DodgeRequest(const SL_MoveRequest& request) { _request = request; }

private:
    SL_DodgeDirection Resolve_DodgeDirection(const SL_MoveRequest& request) const;
    SoulLikeAnim Resolve_DodgeAnim(SL_DodgeDirection direction) const;

private:
    SL_MoveRequest      _request{};
    SL_DodgeDirection   _direction = SL_DodgeDirection::Fwd;
    SoulLikeAnim        _anim = SoulLikeAnim::Dodge_Fwd;

public:
    static Shared<SL_Dodge> Create(const Shared<SL_Maelle>& owner);
};

NS_END
