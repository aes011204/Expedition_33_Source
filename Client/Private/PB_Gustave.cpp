#include "PB_Gustave.h"

#include "ClientInstance.h"
#include "GameDataTable.h"
#include "PlayerStatCom.h"

IMPLEMENT_REFLECTION(PB_Gustave)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "PB_Gustave";
    info.category = "Character";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);
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

void PB_Gustave::Get_WeaponAttachDescs(vector<WeaponAttachDesc>& outDescs) const
{
    outDescs.clear();

    const string editorPreviewWeaponId = Get_EditorPreviewWeaponId();
    const string equippedWeaponId = !editorPreviewWeaponId.empty()
        ? editorPreviewWeaponId
        : (_combatStatCom ? _combatStatCom->Get_PlayerRuntimeStat().CurEquippedWeaponId : "");

    const GameDataTable::WeaponDesc* weapon = equippedWeaponId.empty()
        ? nullptr
        : CLIENT->Get_WeaponDesc("Gustave", equippedWeaponId);

    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs("Gustave");
    if (weapon == nullptr && weapons != nullptr && !weapons->empty())
        weapon = &weapons->front(); // 장착값이 아직 없을 때만 기본 무기로 fallback 한다.

    WeaponAttachDesc rightHandDesc{};
    rightHandDesc.partTag = L"Part_Weapon";
    rightHandDesc.weaponPrefabName = (weapon != nullptr && !weapon->weaponPrefabName.empty())
        ? weapon->weaponPrefabName
        : Constants::Prefab::Gustave_Lanceram;
    rightHandDesc.socketName = "Weapon_R";
    outDescs.emplace_back(std::move(rightHandDesc));

    WeaponAttachDesc leftHandDesc{};
    leftHandDesc.partTag = L"Part_WeaponL";
    leftHandDesc.weaponPrefabName = (weapon != nullptr && !weapon->subWeaponPrefabName.empty())
        ? weapon->subWeaponPrefabName
        : Constants::Prefab::Gustave_Lanceram_Gun;
    leftHandDesc.socketName = "Weapon_L";
    outDescs.emplace_back(std::move(leftHandDesc));
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
