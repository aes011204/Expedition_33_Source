#include "PlayerControllerCom.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "InputCom.h"
#include "MovementCom.h"

IMPLEMENT_REFLECTION(PlayerControllerCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BOOL("Use Controll Yaw", _bUseControllerRotationYaw);
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

	if (GAME->Is_PlayerInputBlocked())
	{
		return;
	}

    if (!_controlEnabled)
    {
        return;
    }

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
	if (GAME->Is_PlayerInputBlocked())
	{
		_movementCom->Apply_Input({});
		return;
	}

    if (!_controlEnabled)
    {
        // 위치 이동은 Hero Manager에서 직접 처리
        _movementCom->Apply_Input({});
        return;
    }

    Check_RigidInput();

	_movementCom->Apply_Input(_currentInput);
	_movementCom->Update_Movement(timeDelta);
}

bool PlayerControllerCom::Has_MoveInput() const
{
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

const MovementCom::MovementInput& PlayerControllerCom::Get_MovementInput() const
{
    return _currentInput;
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
