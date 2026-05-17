#include "PB_Gustave.h"

IMPLEMENT_REFLECTION(PB_Gustave)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "PB_Gustave";
    info.category = "Character";
    return true;
}

PB_Gustave::PB_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Player_BattleBase(device, context)
{
}

PB_Gustave::PB_Gustave(const PB_Gustave& prototype)
    : Player_BattleBase(prototype)
{
}

HairChainPhysicsDesc PB_Gustave::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    desc.profile = HairPhysicsProfile::Gustave;
    desc.bonePrefix = "Strand_";

    // Gustave: 짧은 머리 체인이 많아서 과하게 찰랑이면 어색함.
    desc.stiffness = 0.065f;
    desc.damping = 0.55f;
    desc.windScale = 0.04f;
    desc.gravity = Vec3(0.f, -0.5f, 0.f);

    desc.movementInertiaScale = 0.065f;
    desc.maxMovementInertia = 0.9f;
    desc.maxMovementOffset = 0.02f;

    desc.poseRestoreStrength = 0.26f;
    desc.maxBendAngleDegree = 18.f;

    desc.teleportDistance = 300.f;
    desc.solverIterations = 2;

    return desc;
}

Shared<PB_Gustave> PB_Gustave::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PB_Gustave>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : PB_Gustave");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> PB_Gustave::Clone(void* arg)
{
    auto clone = make_shared<PB_Gustave>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : PB_Gustave");
        return nullptr;
    }

    return clone;
}

void PB_Gustave::Free()
{
    Player_BattleBase::Free();
}
