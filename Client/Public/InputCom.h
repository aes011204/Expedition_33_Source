#pragma once

#include "Component.h"
#include "MovementCom.h"

NS_BEGIN(Client)

class InputCom : public Component
{
    GENERATED_COMPONENT(InputCom)

public:
    explicit InputCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    explicit InputCom(const InputCom& Prototype);
    virtual ~InputCom() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

    void    Update_Input();

    const MovementCom::MovementInput& Get_MovementInput() const { return _movementInput; }
    bool    Has_MoveInput() const;     // 입력이 들어왔는지

private:
    MovementCom::MovementInput _movementInput{};

public:
    static Shared<InputCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
