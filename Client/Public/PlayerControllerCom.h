#pragma once

#include "Component.h"
#include "MovementCom.h"

NS_BEGIN(Engine)
    class MovementCom;
NS_END

NS_BEGIN(Client)

class InputCom;
class PlayerStateMachineCom;

class PlayerControllerCom : public Component
{
    GENERATED_COMPONENT(PlayerControllerCom)

public:
    PlayerControllerCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PlayerControllerCom(const PlayerControllerCom& Prototype);
    virtual ~PlayerControllerCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

    void    Update_Controller(float timeDelta);

public:
    bool    Has_MoveInput() const;
    bool    Is_WalkToggled() const;
    const MovementCom::MovementInput& Get_MovementInput() const;
    bool    Get_UseControlYaw() const { return _bUseControllerRotationYaw; }
    void    Set_UseControlYaw(bool check) { _bUseControllerRotationYaw = check; }


private:
    // Owner가 어차피 weak라 순환참조 걱정없어서 Shared 사용
    Shared<MovementCom>             _movementCom{ nullptr };
    Shared<InputCom>                _inputCom{ nullptr };

private:
    // 카메라 회전값에 따라서 플레이에 Yaw를 회전시킬 것인지?
    // 일단 33원정대 스타일에서는 필요없을거같음.
    bool                 _isWalkToggled{ false };
    bool                 _bUseControllerRotationYaw = true;
    float                _testRelfectionValue = 1.f;

public:
    static Shared<PlayerControllerCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
