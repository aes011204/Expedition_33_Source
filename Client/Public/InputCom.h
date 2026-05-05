#pragma once

#include "Component.h"

NS_BEGIN(Client)

class InputCom : public Component
{
    GENERATED_COMPONENT(InputCom)

public:
    // 현재 프레임에 입력된 값
    struct InputFrame
    {
        float moveX = 0.f;
        float moveZ = 0.f;

        bool  isRunPressed = false;
        bool  isJumpDown= false;
    };

public:
    explicit InputCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    explicit InputCom(const InputCom& Prototype);
    virtual ~InputCom();

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

    void    Update_Input();

    const InputFrame& Get_Frame() { return _frame; }

    bool    Has_MoveInput() const;     // W, S 입력 들어왔는지
    bool    Has_AnyInput()  const;     // WASD 중 아무거나 들어왔을 때

private:
    InputFrame _frame;

public:
    static Shared<InputCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
