#include "PhysX_Manager.h"

PhysX_Manager::PhysX_Manager()
{
}

PhysX_Manager::~PhysX_Manager()
{
}

HRESULT PhysX_Manager::Initialize()
{
    _foundation = PxCreateFoundation(PX_PHYSICS_VERSION, _allocator, _errorCallback);
    CHECK_NULL(_foundation, E_FAIL);

#ifdef _DEBUG
    _pvd = PxCreatePvd(*_foundation);
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    _pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
#endif

    _physics = PxCreatePhysics(PX_PHYSICS_VERSION, *_foundation, PxTolerancesScale(), true,
#ifdef _DEBUG
        _pvd
#else
        nullptr
#endif
    );
    CHECK_NULL(_physics, E_FAIL);

    _dispatcher = PxDefaultCpuDispatcherCreate(2);

    PxSceneDesc sceneDesc(_physics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.f, -9.81f, 0.f);
    sceneDesc.cpuDispatcher = _dispatcher;
    sceneDesc.filterShader = PxDefaultSimulationFilterShader;

    _scene = _physics->createScene(sceneDesc);
    CHECK_NULL(_scene, E_FAIL);

    return S_OK;
}

void PhysX_Manager::Update(float timeDelta)
{
    if (!_scene || timeDelta <= 0.f)
    {
        return;
    }

    if (_scene->simulate(timeDelta))
        _scene->fetchResults(true);
}

Unique<PhysX_Manager> PhysX_Manager::Create()
{
    auto instance = make_unique<PhysX_Manager>();

    if (FAILED(instance->Initialize()))
    {
        return nullptr;
    }
    return instance;
}

void PhysX_Manager::Free()
{
    if (_scene)
    {
        _scene->release();
        _scene = nullptr;
    }

    if (_dispatcher)
    {
        _dispatcher->release();
        _dispatcher = nullptr;
    }

    if (_physics)
    {
        _physics->release();
        _physics = nullptr;
    }

#ifdef _DEBUG
    if (_pvd)
    {
        PxPvdTransport* transport = _pvd->getTransport();
        _pvd->release();
        _pvd = nullptr;

        if (transport)
        {
            transport->release();
        }
    }
#endif
    if (_foundation)
    {
        _foundation->release();
        _foundation = nullptr;
    }
}
