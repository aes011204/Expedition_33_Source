#include "PB_Maelle.h"

IMPLEMENT_REFLECTION(PB_Maelle)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "PB_Maelle";
    info.category = "Character";
    return true;
}

PB_Maelle::PB_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Player_BattleBase(device, context)
{
}

PB_Maelle::PB_Maelle(const PB_Maelle& prototype)
    : Player_BattleBase(prototype)
{
}

HairChainPhysicsDesc PB_Maelle::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    desc.profile = HairPhysicsProfile::Maelle;
    desc.bonePrefix = "Strand_";

    // Maelle: Strand_0_1 total 46, Strand_5/6도 길어서 Lune보다 더 흐르게.
    desc.stiffness = 0.035f;
    desc.damping = 0.38f;
    desc.windScale = 0.12f;
    desc.gravity = Vec3(0.f, -0.35f, 0.f);

    desc.movementInertiaScale = 0.12f;
    desc.maxMovementInertia = 1.6f;
    desc.maxMovementOffset = 0.045f;

    desc.poseRestoreStrength = 0.15f;
    desc.maxBendAngleDegree = 34.f;

    desc.teleportDistance = 300.f;
    desc.solverIterations = 3;

    return desc;
}

bool PB_Maelle::Try_GetWeaponAttackDesc(WeaponAttachDesc& outDesc) const
{
    outDesc.modelTag = L"Model_Medalum";
    outDesc.socketName = "Weapon_r";

    return true;
}

Shared<PB_Maelle> PB_Maelle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PB_Maelle>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : PB_Maelle");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> PB_Maelle::Clone(void* arg)
{
    auto clone = make_shared<PB_Maelle>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : PB_Maelle");
        return nullptr;
    }

    return clone;
}

void PB_Maelle::Free()
{
    Player_BattleBase::Free();
}
