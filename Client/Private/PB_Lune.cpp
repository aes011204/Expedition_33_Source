#include "PB_Lune.h"

#include "ClientInstance.h"
#include "GameDataTable.h"

IMPLEMENT_REFLECTION(PB_Lune)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "PB_Lune";
    info.category = "Character";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);
    return true;
}

PB_Lune::PB_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Player_BattleBase(device, context)
{
}

PB_Lune::PB_Lune(const PB_Lune& prototype)
    : Player_BattleBase(prototype)
{
}

HairChainPhysicsDesc PB_Lune::Get_HairChainPhysicsDesc() const
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

void PB_Lune::Get_WeaponAttachDescs(vector<WeaponAttachDesc>& outDescs) const
{
    outDescs.clear();

    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs("Lune");
    const GameDataTable::WeaponDesc* weapon = (weapons != nullptr && !weapons->empty()) ? &weapons->front() : nullptr;

    WeaponAttachDesc desc{};
    desc.partTag = L"Part_Weapon";
    desc.weaponPrefabName = (weapon != nullptr && !weapon->weaponPrefabName.empty())
        ? weapon->weaponPrefabName
        : Constants::Prefab::Lune_Colim; // fallback

    desc.useBodyFollow = false;
    desc.bodyFollowOffset = Vec3::Zero;
    desc.socketName = nullptr;

    outDescs.emplace_back(std::move(desc));
}

Shared<PB_Lune> PB_Lune::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PB_Lune>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : PB_Lune");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> PB_Lune::Clone(void* arg)
{
    auto clone = make_shared<PB_Lune>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : PB_Lune");
        return nullptr;
    }


    return clone;
}

void PB_Lune::Free()
{
    Player_BattleBase::Free();
}
