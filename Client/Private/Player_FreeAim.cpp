#include "pch.h"
#include "Player_FreeAim.h"

#include <Camera.h>

#include "OffsetAnimationModifier.h"
#include "Body_Player.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"

Player_FreeAim::Player_FreeAim(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner } {}
HRESULT Player_FreeAim::Initialize()
{
    return Player_State::Initialize();
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_FreeAim::EnterInvoke()
{
    _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Idle), 0.1f, true);

    if (auto model = _bodyPlayer->Get_Model())
    {
        for (auto& modifier : model->Get_Modifiers())
        {
            if (auto offsetMod = dynamic_pointer_cast<OffsetAnimationModifier>(modifier))
            {
                _offsetModifier = offsetMod;
                _offsetModifier->Set_Active(true);
                break;
            }
        }
    }

    return true;
}

void Player_FreeAim::Update(float timeDelta)
{
    if (false == IsEnterFreeAim())
    {
        if (IsEnterMove())
        {
            _stateCom->Change_State(PlayerState::Move);
            return;
        }
        else
        {
            _stateCom->Change_State(PlayerState::Idle);
            return;
        }
    }

   /* Vec2 mouseDelta = GAME->Get_MouseDelta();
    if (mouseDelta.x != 0.f)
    {
        _owner.lock()->Get_Transform()->Rotate(Vec3::Up, mouseDelta.x * _rotationSensitivity);
    }*/

    if (_offsetModifier)
    {
        if (auto camera = GAME->Get_ActiveCamera())
        {
            _offsetModifier->Set_AimPitch(camera->Get_Pitch());
        }
    }

}

void Player_FreeAim::LateUpdate(float timeDelta)
{

}

void Player_FreeAim::ExitInvoke()
{
    if (_offsetModifier)
    {
        _offsetModifier->Set_AimPitch(0.f);
        _offsetModifier->Set_Active(false);
    }
}

Shared<Player_FreeAim> Player_FreeAim::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_FreeAim>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER FREE AIM] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_FreeAim::Free()
{
    Player_State::Free();
}
