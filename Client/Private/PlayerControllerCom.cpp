#include "PlayerControllerCom.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "InputCom.h"
#include "MovementCom.h"

#include "Debug_Profile.h"

IMPLEMENT_REFLECTION(PlayerControllerCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BOOL("Use Control Yaw", _bUseControllerRotationYaw);
    PROPERTY_FLOAT("Test 값", _testRefectionValue, 0.f, 3.f, 0.05f);

    return true;
}

PlayerControllerCom::PlayerControllerCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Component(device, context)
{
}

PlayerControllerCom::PlayerControllerCom(const PlayerControllerCom& Prototype)
	: Component(Prototype)
{
}

HRESULT PlayerControllerCom::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT PlayerControllerCom::Initialize(void* arg)
{
	CHECK_FAILED(Component::Initialize(arg), E_FAIL);

	return S_OK;
}

void PlayerControllerCom::BeginPlay()
{
	Component::BeginPlay();

	auto owner = Get_Owner();
	CHECK_NULL(owner);

	_movementCom = owner->Get_Component<MovementCom>();
	CHECK_NULL(_movementCom);

	_inputCom = owner->Get_Component<InputCom>();
	CHECK_NULL(_inputCom);
}

void PlayerControllerCom::Update_Controller(float timeDelta)
{
    Update_InputState();
    Update_Movement(timeDelta);
}

void PlayerControllerCom::Update_InputState()
{
    _currentInput = {};

	if (GAME->Is_WorldGameplayInputBlocked())
        return;

    if (!_controlEnabled)
        return; // follower는 키보드 입력 X

	_inputCom->Update_Input();
    _currentInput = _inputCom->Get_MovementInput();

    if (_currentInput.isWalkDown)
    {
        _isWalkToggled = !_isWalkToggled;
    }

    _currentInput.isWalkDown = _isWalkToggled;
}

void PlayerControllerCom::Update_Movement(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("PlayerController::UpdateMovement");

	if (GAME->Is_WorldGameplayInputBlocked())
	{
        if (!_movementCom)
            return;

        MovementCom::MovementInput blockedInput{};
        blockedInput.smoothStopOnNoMoveInput = true; // UI/상호작용 진입으로 입력만 끊긴 경우, 기존 속도를 짧게 감속시킨다.

		_movementCom->Clear_FollowMoveCommand(); 
        _movementCom->Apply_Input(blockedInput);
        _movementCom->Update_Movement(timeDelta);
        return;
	}

    if (_followControl.enabled)
    {
        if (!_followControl.hasMoveIntent || _followControl.worldDir.LengthSquared() <= 0.f)
        {
            _movementCom->Clear_FollowMoveCommand();
            _movementCom->Apply_Input({});
            _movementCom->Stop_HorizontalMovement();

            _movementCom->Update_Movement(timeDelta);

            _followControl.blocked = false; // 이동 의도가 없으면 blocked도 해제
            return;
        }

        MovementCom::FollowMoveCommand command{};
        command.enabled = true;
        command.worldDir = _followControl.worldDir;
        command.walkFollow = _followControl.walkFollow;
        command.sprintFollow = _followControl.sprintFollow; // 실제 속도는 Movement에서 전환

        auto owner = Get_Owner();
        auto transform = owner ? owner->Get_Transform() : nullptr;
        const Vec3 beforePos = transform ? transform->Get_WorldPosition() : Vec3::Zero;

        _movementCom->Set_FollowMoveCommand(command);
        _movementCom->Apply_Input({}); // follower는 jump/input을 받지 않음
        _movementCom->Update_Movement(timeDelta);

        if (transform)
        {
            Vec3 actualMove = transform->Get_WorldPosition() - beforePos;
            actualMove.y = 0.f;

            _followControl.blocked = actualMove.Length() <= 0.001f;
        }

        return;
    }

    if (!_controlEnabled)
    {
        _movementCom->Apply_Input({});
        return;
    }

    Check_RigidInput();

    _movementCom->Clear_FollowMoveCommand(); // active hero는 follow command를 쓰지 않는다.
    _movementCom->Apply_Input(_currentInput);
    _movementCom->Update_Movement(timeDelta);
}

Vec3 PlayerControllerCom::Get_Velocity() const
{
    return _movementCom->Get_Velocity();
}

bool PlayerControllerCom::Has_MoveInput() const
{
    if (_followControl.enabled)
        return _followControl.hasMoveIntent;

    return _currentInput.moveX != 0.f || _currentInput.moveZ != 0.f;
}

bool PlayerControllerCom::Is_WalkToggled() const
{
    return _isWalkToggled;
}

bool PlayerControllerCom::Is_Grounded() const
{
    return _movementCom->Is_Grounded();
}

bool PlayerControllerCom::Is_RightMouseClicked() const
{
    return _currentInput.isMouseLDown;
}

const MovementCom::MovementInput& PlayerControllerCom::Get_MovementInput() const
{
    return _currentInput;
}

bool PlayerControllerCom::Has_LocomotionIntent() const
{
    if (_followControl.enabled)
        return _followControl.hasMoveIntent;

    return _currentInput.moveX != 0.f || _currentInput.moveZ != 0.f;
}

bool PlayerControllerCom::Should_SprintMove() const
{
    if (_followControl.enabled)
        return !_followControl.walkFollow && _followControl.sprintFollow; // 가까운 거리 Walk 추종 중에는 Sprint로 올리지 않는다.

    return _currentInput.isRunPressed;
}

bool PlayerControllerCom::Should_WalkMove() const
{
    if (_followControl.enabled)
        return _followControl.walkFollow;

    return _isWalkToggled;
}

bool PlayerControllerCom::Is_MovementBlocked() const
{
    return _followControl.enabled && _followControl.blocked;
}

bool PlayerControllerCom::Allows_ActionInput() const
{
    return !_followControl.enabled;
}

void PlayerControllerCom::Check_RigidInput()
{
    if (_rigidControl.rigidMovement)
    {
        _currentInput.moveX = 0.f;
        _currentInput.moveZ = 0.f;
    }

    if (_rigidControl.rigidJump)
    {
        _currentInput.isJumpDown = false;
    }

    if (_rigidControl.rigidLook)
    {
        _currentInput.isLockLook = true;
    }

    if (_rigidControl.rigidWalk)
    {
        _currentInput.isWalkDown = true;
        _currentInput.isRunPressed = false;
    }

    if (_rigidControl.rigidLookDir.LengthSquared() > 0.f)
    {
        _currentInput.forcedDirection = _rigidControl.rigidLookDir;
    }

    if (_rigidControl.rigidVelocity != Vec3::Zero)
    {
        _currentInput.moveX = 0.f;
        _currentInput.moveZ = 0.f;

        Vec3 vel = _rigidControl.rigidVelocity;
        _movementCom->Set_Velocity(vel);
    }

    _rigidControl = {};
}

Shared<PlayerControllerCom> PlayerControllerCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<PlayerControllerCom>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		LOG_ERROR("Failed to Created : PlayerControllerCom");
		return nullptr;
	}

	return instance;
}

Shared<Component> PlayerControllerCom::Clone(void* arg)
{
	auto clone = make_shared<PlayerControllerCom>(*this);

	if (FAILED(clone->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : PlayerControllerCom");
		return nullptr;
	}

	return clone;
}

void PlayerControllerCom::Free()
{
	Component::Free();
}
