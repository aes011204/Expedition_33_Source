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
    PROPERTY_FLOAT("Test 값", _testRelfectionValue, 0.f, 3.f, 0.05f);

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
	if (GAME->Is_PlayerInputBlocked())
	{
		MovementCom::MoveInput moveInput{};
		_movementCom->Apply_Input(moveInput);
		return;
	}

	_inputCom->Update_Input();

	MovementCom::MoveInput moveInput{};
	const InputCom::InputFrame& inputFrame = _inputCom->Get_Frame();

	moveInput.moveX = inputFrame.moveX;
	moveInput.moveZ = inputFrame.moveZ;
	moveInput.isRunPressed = inputFrame.isRunPressed;
	moveInput.isJumpDown = inputFrame.isJumpDown;

	_movementCom->Apply_Input(moveInput);
	_movementCom->Update_Movement(timeDelta);

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
