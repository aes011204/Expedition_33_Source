#include "TrailPreviewPlayerObject.h"

#include "Body_Player.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "TrailPreviewWeaponObject.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr auto kSourcePrefabName{ "PB_Gustave" };

    bool Is_RestorableVisualPart(const wstring& partTag)
    {
        return partTag == L"Part_Body" || partTag == L"Part_Head" || partTag == L"Part_Hair";
    }
}

TrailPreviewPlayerObject::TrailPreviewPlayerObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Player_BattleBase{ device, context }
{
}

TrailPreviewPlayerObject::TrailPreviewPlayerObject(const TrailPreviewPlayerObject& prototype)
    : Player_BattleBase{ prototype }
{
}

TrailPreviewPlayerObject::~TrailPreviewPlayerObject()
{
    Free();
}

HRESULT TrailPreviewPlayerObject::Initialize(void*)
{
    ContainerObjectDesc desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(Character_BattleBase::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);
    CHECK_FAILED(Restore_VisualPartsFromPrefab(), E_FAIL);

    _deferPreviewWeaponCreation = false;
    if (auto* body = dynamic_cast<Body_Player*>(Get_PartObject(L"Part_Body")))
        CHECK_FAILED(Ready_WeaponPart(body), E_FAIL);

    Set_Name(L"TrailPreview_Player");
    Set_SourcePrefabName(Internal::kSourcePrefabName);
    Set_PreviewVisible(false);

    if (const auto* body = dynamic_cast<Body_Player*>(Get_PartObject(L"Part_Body")))
    {
        if (const Shared<ModelCom> model = body->Get_Model())
            model->Refresh_AnimStateMappingFromOwnerContext();
    }

    return S_OK;
}

Shared<TrailPreviewWeaponObject> TrailPreviewPlayerObject::Get_PreviewWeapon() const
{
    const auto iter = _partObjects.find(L"Part_Weapon");
    if (iter == _partObjects.end())
        return nullptr;

    return dynamic_pointer_cast<TrailPreviewWeaponObject>(iter->second);
}

void TrailPreviewPlayerObject::Set_PreviewVisible(bool visible)
{
    Set_Visible(visible);

    if (const Shared<TrailPreviewWeaponObject> weapon = Get_PreviewWeapon())
        weapon->Set_Visible(visible);
}

HRESULT TrailPreviewPlayerObject::Restore_VisualPartsFromPrefab()
{
    const std::filesystem::path prefabPath =
        std::filesystem::path(GAME->Get_AssetRoot()) / L"Data" / L"json" / L"Prefabs" / L"PB_Gustave.prefab.json";

    const json prefabRoot = GAME->Load_JsonFile(prefabPath);
    if (!prefabRoot.is_object())
    {
        LOG_ERROR("[TrailPreview] failed to load visual fixture prefab. path={}", prefabPath.string());
        return E_FAIL;
    }

    const auto partsIter = prefabRoot.find("parts");
    if (partsIter == prefabRoot.end() || !partsIter->is_array())
    {
        LOG_ERROR("[TrailPreview] visual fixture prefab has no parts array. path={}", prefabPath.string());
        return E_FAIL;
    }

    Set_SourcePrefabName(Internal::kSourcePrefabName);

    uint32 restoredCount = 0;
    for (const json& partNode : *partsIter)
    {
        if (SUCCEEDED(Restore_VisualPartFromPrefab(partNode)))
            ++restoredCount;
    }

    if (restoredCount < 3)
    {
        LOG_WARN("[TrailPreview] restored {}/3 visual parts from PB_Gustave prefab.", restoredCount);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT TrailPreviewPlayerObject::Restore_VisualPartFromPrefab(const json& partNode)
{
    if (!partNode.is_object())
        return E_FAIL;

    const string partTagString = partNode.value("part_tag", string{});
    const wstring partTag = String::ToWString(partTagString);
    if (!Internal::Is_RestorableVisualPart(partTag))
        return E_FAIL;

    const auto objectIter = partNode.find("object");
    if (objectIter == partNode.end() || !objectIter->is_object())
    {
        LOG_WARN("[TrailPreview] visual part '{}' has no object payload.", partTagString);
        return E_FAIL;
    }

    PartObject* partObject = Get_PartObject(partTag);
    if (partObject == nullptr)
    {
        LOG_WARN("[TrailPreview] visual part '{}' was not created before prefab restore.", partTagString);
        return E_FAIL;
    }

    partObject->From_Json(*objectIter);
    return S_OK;
}

HairChainPhysicsDesc TrailPreviewPlayerObject::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    desc.profile = HairPhysicsProfile::Gustave;
    desc.bonePrefix = "Strand_";
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

bool TrailPreviewPlayerObject::Try_GetWeaponAttackDesc(WeaponAttachDesc& outDesc) const
{
    outDesc = {};
    return false;
}

HRESULT TrailPreviewPlayerObject::Ready_WeaponPart(Body_Player* body)
{
    CHECK_NULL(body, E_FAIL);
    if (_deferPreviewWeaponCreation)
        return S_OK;

    if (Get_PartObject(L"Part_Weapon") != nullptr)
        return E_FAIL;

    PartObject::PARTOBJECT_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();

    const Shared<TrailPreviewWeaponObject> weapon = TrailPreviewWeaponObject::Create(_device, _context);
    CHECK_NULL(weapon, E_FAIL);
    CHECK_FAILED(weapon->Initialize(&weaponDesc), E_FAIL);
    weapon->Set_Owner(GetSharedPtr<GameObject>());

    _partObjects.emplace(L"Part_Weapon", weapon);
    _partObjectUpdateOrder.push_back(weapon);

    return S_OK;
}

Shared<TrailPreviewPlayerObject> TrailPreviewPlayerObject::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewPlayerObject>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewPlayerObject");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewPlayerObject::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewPlayerObject>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewPlayerObject");
        MSG_BOX("Failed to Clone : TrailPreviewPlayerObject");
        return nullptr;
    }

    return instance;
}

void TrailPreviewPlayerObject::Free()
{
    __super::Free();
}

NS_END
