#include "pch.h"
#include "Player_WorldAction.h"

#include "Body_Player.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "PW_Gustave.h"

Player_WorldAction::Player_WorldAction(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner }
{
}

HRESULT Player_WorldAction::Initialize()
{
    return Player_State::Initialize();
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_WorldAction::EnterInvoke()
{
    auto state = _stateCom->Get_CurrentPlayerState();
    if (state == PlayerState::WorldAction) return false;

    _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_Explo), 0.2f, false);

    // 코드 스폰
    //const Shared<Player_WorldBase> owner = _owner.lock();
    //if (dynamic_pointer_cast<PW_Gustave>(owner))
    //{
    //    const Shared<EffectCom> effectCom = owner->Get_Component<EffectCom>();
    //    if (effectCom == nullptr ||
    //        FAILED(effectCom->Play_AttachedEffect(
    //            GAME->Current_LevelIndex(),
    //            L"DistortionTest",
    //            L"Part_Body",
    //            "FX_Hand_R1",
    //            L"Layer_Effect",
    //            Vec3::Zero)))
    //    {
    //        LOG_WARN("Player_WorldAction DistortionTest hand attach failed.");
    //    }
    //}

    return true;
}

void Player_WorldAction::Update(float timeDelta)
{
    PlayerControllerCom::RigidControl rigidControl{};
    rigidControl.rigidMovement = true;
    rigidControl.rigidJump = true;
    _controllerCom->Set_RigidControl(rigidControl);

    bool isFinished = _bodyPlayer->Is_AnimationFinished();
    float curProgress = _bodyPlayer->Get_AnimationProgress();

    if (curProgress >= 0.9f)
    {
        if (Has_MoveInput())
        {
            _stateCom->Change_State(PlayerState::Move);
        }
        else
        {
            _stateCom->Change_State(PlayerState::Idle);
        }
    }
}

void Player_WorldAction::LateUpdate(float timeDelta)
{
    
}

void Player_WorldAction::ExitInvoke()
{
    
}


Shared<Player_WorldAction> Player_WorldAction::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_WorldAction>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER WORLD ACTION] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_WorldAction::Free()
{
    Player_State::Free();
}
