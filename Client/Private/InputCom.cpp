#include "InputCom.h"
#include "GameInstance.h"

InputCom::InputCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{device, context}
{
}

InputCom::InputCom(const InputCom& Prototype)
    : Component{Prototype}
    , _frame(Prototype._frame)
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

    _frame = {};

    return S_OK;
}

void InputCom::Update_Input()
{
    // 매 프레임 입력 초기화
    _frame = {};

     if (GAME->KeyPress(KEY_TYPE::W))
        _frame.moveZ += 1.f;

    if (GAME->KeyPress(KEY_TYPE::S))
        _frame.moveZ -= 1.f;

    if (GAME->KeyPress(KEY_TYPE::A))
        _frame.moveX -= 1.f;

    if (GAME->KeyPress(KEY_TYPE::D))
        _frame.moveX += 1.f;

    _frame.isRunPressed = GAME->KeyPress(KEY_TYPE::SHIFT);
    _frame.isJumpDown = GAME->KeyDown(KEY_TYPE::SPACE);
}

bool InputCom::Has_MoveInput() const
{
    return _frame.moveX != 0.f || _frame.moveZ != 0.f;
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
