#pragma once

#include "Base.h"
#include "PhysX_Types.h"

NS_BEGIN(physx)
class PxController;
NS_END

NS_BEGIN(Engine)

class PhysX_Manager;

class ENGINE_DLL PhysicsCharacterController final : public Base
{
public:
    PhysicsCharacterController() = default;
    ~PhysicsCharacterController() override = default;

public:
    HRESULT Initialize(
        PhysX_Manager* physXManager,
        const CharacterControllerDesc& desc,
        const Vec3& initialFootPosition);

    CharacterControllerMoveResult Move(const Vec3& displacement, float timeDelta);

    void Set_FootPosition(const Vec3& footPosition);
    Vec3 Get_FootPosition() const;

private:
    PhysX_Manager* _physXManager = nullptr;
    PxController* _controller = nullptr;
    CharacterControllerDesc _desc{};

public:
    static Shared<PhysicsCharacterController> Create(
        PhysX_Manager* physXManager,
        const CharacterControllerDesc& desc,
        const Vec3& initialFootPosition);

    void Free() override;
};

NS_END
