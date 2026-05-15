#include "Battle_Lune.h"

IMPLEMENT_REFLECTION(Battle_Lune)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "Battle_Lune";
    info.category = "Character";
    return true;
}

Battle_Lune::Battle_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Player_BattleBase(device, context)
{
}

Battle_Lune::Battle_Lune(const Battle_Lune& prototype)
    : Player_BattleBase(prototype)
{
}

HairChainPhysicsDesc Battle_Lune::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};
    desc.profile = HairPhysicsProfile::Lune;
    desc.bonePrefix = "Strand_";
    desc.stiffness = 0.04f;
    desc.damping = 0.42f;
    desc.windScale = 0.08f;
    desc.gravity = Vec3(0.f, -0.45f, 0.f);
    desc.movementInertiaScale = 0.12f;
    desc.maxMovementInertia = 1.6f;
    desc.maxMovementOffset = 0.035f;
    desc.poseRestoreStrength = 0.18f;
    desc.maxBendAngleDegree = 28.f;
    desc.teleportDistance = 300.f;
    desc.solverIterations = 2;

    return desc;
}

Shared<Battle_Lune> Battle_Lune::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Battle_Lune>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Battle_Lune");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Battle_Lune::Clone(void* arg)
{
    auto clone = make_shared<Battle_Lune>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Battle_Lune");
        return nullptr;
    }

    return clone;
}

void Battle_Lune::Free()
{
    Player_BattleBase::Free();
}
