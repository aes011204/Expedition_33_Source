#include "pch.h"
#include "Player_FreeAim.h"

#include <Camera.h>

#include "OffsetAnimationModifier.h"
#include "Body_Player.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"

Player_FreeAim::Player_FreeAim(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner } {}

HRESULT Player_FreeAim::Initialize()
{
    return Player_State::Initialize();
}

void Player_FreeAim::Begin_Play()
{
    Player_State::Begin_Play();

    if (auto model = _bodyPlayer->Get_Model())
    {
        for (auto& modifier : model->Get_Modifiers())
        {
            if (auto offsetMod = dynamic_pointer_cast<OffsetAnimationModifier>(modifier))
            {
                const wstring& tag = modifier->Get_ModifierTag();

                if (tag == L"AimOffset") {
                    _offsetModifier = offsetMod;
                }
                else if (tag == L"Walk_F") _walkModifiers[0] = offsetMod;
                else if (tag == L"Walk_B") _walkModifiers[1] = offsetMod;
                else if (tag == L"Walk_L") _walkModifiers[2] = offsetMod;
                else if (tag == L"Walk_R") _walkModifiers[3] = offsetMod;
                else if (tag == L"Walk_LB") _walkModifiers[4] = offsetMod;
                else if (tag == L"Walk_RB") _walkModifiers[5] = offsetMod;
            }
        }
    }
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_FreeAim::EnterInvoke()
{
    _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Idle), 0.1f, true);
    if (_offsetModifier) _offsetModifier->Set_Active(true);

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

    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();
    if (IsFreeAimShoot())
    {
        if (animIndex != ETOI(plAnim::Pl_FreeAim_Shoot))
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Shoot), 0.25f, false);
            _bodyPlayer->Set_AnimationSpeed(2.f);
        }
    }
    else
    {
        if (animIndex == ETOI(plAnim::Pl_FreeAim_Shoot) && _bodyPlayer->Is_AnimationFinished())
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Idle), 0.25f, true);
            _bodyPlayer->Set_AnimationSpeed(1.f);
        }
    }

    Update_DirectionalModifiers();

    if (_offsetModifier)
    {
        if (auto camera = GAME->Get_ActiveCamera())
        {
            _offsetModifier->Set_AimPitch(camera->Get_Pitch());

            auto rigid = _controllerCom->Get_RigidControl();
            rigid.rigidLookDir = camera->Get_Transform()->Get_WorldForward();
            rigid.rigidWalk = true;
            _controllerCom->Set_RigidControl(rigid);
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
        _offsetModifier->Set_Active(false);
    }

    for (int i = 0; i < 6; ++i)
    {
        if (_walkModifiers[i])
            _walkModifiers[i]->Set_Active(false);
    }
}

void Player_FreeAim::Update_DirectionalModifiers()
{
    MovementCom::MovementInput input = _controllerCom->Get_MovementInput();

    int targetWalkIndex = -1; // 0:F, 1:B, 2:LF, 3:RF, 4:LB, 5:RB

    float moveX = input.moveX;
    float moveZ = input.moveZ;

    if (abs(moveZ) > 0.1f || abs(moveX) > 0.1f)
    {
        if (moveZ < -0.1f && abs(moveX) > 0.1f) {
            targetWalkIndex = (moveX < 0.f) ? 4 : 5; // LB(4), RB(5)
        }
        else if (abs(moveZ) > abs(moveX) * 1.5f) {
            targetWalkIndex = (moveZ > 0.f) ? 0 : 1;
        }
        else {
            targetWalkIndex = (moveX < 0.f) ? 2 : 3; // LF(2), RF(3)
        }
    }

    for (int i = 0; i < 6; ++i)
    {
        if (_walkModifiers[i])
            _walkModifiers[i]->Set_Active(i == targetWalkIndex);
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
