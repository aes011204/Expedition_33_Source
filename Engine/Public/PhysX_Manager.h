#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class PhysX_Manager final : public Base
{
public:
    PhysX_Manager();
    ~PhysX_Manager() override;

public:
    PxPhysics* Get_Physics() const { return _physics; }
    PxScene* Get_Scene() const { return _scene; }

public:
    HRESULT Initialize();
    void Update(float timeDelta);

private:
    PxDefaultAllocator _allocator;
    PxDefaultErrorCallback _errorCallback;

    PxFoundation* _foundation = {};
    PxPhysics* _physics = {};
    PxScene* _scene = {};
    PxDefaultCpuDispatcher* _dispatcher = {};

#ifdef _DEBUG
    PxPvd* _pvd = {};
#endif

public:
    static Unique<PhysX_Manager> Create();
    void Free() override;
};

NS_END
