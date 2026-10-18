#include "InputCom.h"
#include "GameInstance.h"

InputCom::InputCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{device, context}
{
}

InputCom::InputCom(const InputCom& prototype)
    : Component{prototype}
    , _movementInput(prototype._movementInput)
{
}

InputCom::~InputCom()
{
}

HRESULT InputCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT InputCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    _movementInput = {};

    return S_OK;
}

void InputCom::Update_Input()
{
    // 매 프레임 입력 초기화
    _movementInput = {};

     if (GAME->KeyPress(KEY_TYPE::W))
        _movementInput.moveZ += 1.f;

    if (GAME->KeyPress(KEY_TYPE::S))
        _movementInput.moveZ -= 1.f;

    if (GAME->KeyPress(KEY_TYPE::A))
        _movementInput.moveX -= 1.f;

    if (GAME->KeyPress(KEY_TYPE::D))
        _movementInput.moveX += 1.f;

    _movementInput.isRunPressed = GAME->KeyPress(KEY_TYPE::SHIFT);
    _movementInput.isJumpDown = GAME->KeyDown(KEY_TYPE::SPACE);
    _movementInput.isWalkDown = GAME->KeyDown(KEY_TYPE::CTRL);

    _movementInput.isInteractionDown = GAME->KeyDown(KEY_TYPE::E);
    _movementInput.isSuddenAttack = GAME->KeyDown(KEY_TYPE::F);

    _movementInput.isMouseLDown = GAME->KeyDown(KEY_TYPE::LBUTTON);
    _movementInput.isMouseRPress = GAME->KeyPress(KEY_TYPE::RBUTTON);
    _movementInput.isMouseRUp = GAME->KeyUp(KEY_TYPE::RBUTTON);
}

bool InputCom::Has_MoveInput() const
{
    return _movementInput.moveX != 0.f || _movementInput.moveZ != 0.f;
}

Shared<InputCom> InputCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<InputCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : InputCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> InputCom::Clone(void* arg)
{
    auto clone = make_shared<InputCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : InputCom");
        return nullptr;
    }

    return clone;
}

void InputCom::Free()
{
    Component::Free();
}
