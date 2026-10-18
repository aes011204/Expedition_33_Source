#include "WorldUpDownRopeSpot.h"

#include "Bounding_AABB.h"
#include "ColliderCom.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "Player_WorldBase.h"
#include "ShaderCom.h"
#include "TransformCom.h"

namespace
{
    constexpr const wchar_t* kTopEntryColliderTag = L"Com_RopeTopEntryCollider";
    constexpr const wchar_t* kBottomEntryColliderTag = L"Com_RopeBottomEntryCollider";
    constexpr const wchar_t* kRopeModelComponentTag = L"Com_RopeModel";
    constexpr const wchar_t* kDefaultRopeModelPath = L"Map/Level_Main_GestralVillage_V2/SM_Scaffold_Rope.model";

    bool NearlyEqual(const Vec3& lhs, const Vec3& rhs)
    {
        constexpr float kEpsilon = 0.0001f;
        return fabsf(lhs.x - rhs.x) <= kEpsilon &&
            fabsf(lhs.y - rhs.y) <= kEpsilon &&
            fabsf(lhs.z - rhs.z) <= kEpsilon;
    }
}

IMPLEMENT_REFLECTION(WorldUpDownRopeSpot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "WorldUpDownRope";
    info.category = "Trigger";

    PROPERTY_INT("Priority", _priority);

    PROPERTY_BOOL("Rope Effect Auto Play", _ropeEffectAutoPlay);
    PROPERTY_WSTRING("Rope Effect Name", _ropeEffectName);
    PROPERTY_VEC3("Rope Effect Offset", _ropeEffectOffset);
    PROPERTY_VEC3("Rope Effect Rotation", _ropeEffectRotation);
    PROPERTY_VEC3("Rope Effect Scale", _ropeEffectScale);
    PROPERTY_VEC2("Rope Center XZ", _ropeCenterXZ);

    PROPERTY_VEC3("Top Collider Center", _topColliderCenter);
    PROPERTY_VEC3("Top Collider Extents", _topColliderExtents);
    PROPERTY_FLOAT("Top Rope Idle Y", _topRopeIdleY);
    PROPERTY_VEC3("Top Start Point Offset", _topStartPointOffset);
    PROPERTY_VEC3("Top Target Point Offset", _topTargetPointOffset);
    PROPERTY_VEC3("Top Rope Point Offset", _topRopePointOffset);

    PROPERTY_VEC3("Bottom Collider Center", _bottomColliderCenter);
    PROPERTY_VEC3("Bottom Collider Extents", _bottomColliderExtents);
    PROPERTY_FLOAT("Bottom Rope Idle Y", _bottomRopeIdleY);
    PROPERTY_VEC3("Bottom Start Point Offset", _bottomStartPointOffset);
    PROPERTY_VEC3("Bottom Target Point Offset", _bottomTargetPointOffset);
    PROPERTY_VEC3("Bottom Rope Point Offset", _bottomRopePointOffset);

    PROPERTY_FLOAT("Warp End Progress", _warpEndProgress, 0.01f, 1.f, 0.01f);
    PROPERTY_WSTRING("Prompt Text", _promptText);

    return true;
}

WorldUpDownRopeSpot::WorldUpDownRopeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : WorldInteractionSpot(device, context)
{
    _warpEndProgress = 0.95f;
    _promptText = L"Rope";
}

WorldUpDownRopeSpot::WorldUpDownRopeSpot(const WorldUpDownRopeSpot& prototype)
    : WorldInteractionSpot(prototype)
    , _ropeEffectAutoPlay(prototype._ropeEffectAutoPlay)
    , _ropeEffectName(prototype._ropeEffectName)
    , _ropeEffectOffset(prototype._ropeEffectOffset)
    , _ropeEffectRotation(prototype._ropeEffectRotation)
    , _ropeEffectScale(prototype._ropeEffectScale)
    , _ropeCenterXZ(prototype._ropeCenterXZ)
    , _topColliderCenter(prototype._topColliderCenter)
    , _topColliderExtents(prototype._topColliderExtents)
    , _topRopeIdleY(prototype._topRopeIdleY)
    , _topStartPointOffset(prototype._topStartPointOffset)
    , _topTargetPointOffset(prototype._topTargetPointOffset)
    , _topRopePointOffset(prototype._topRopePointOffset)
    , _bottomColliderCenter(prototype._bottomColliderCenter)
    , _bottomColliderExtents(prototype._bottomColliderExtents)
    , _bottomRopeIdleY(prototype._bottomRopeIdleY)
    , _bottomStartPointOffset(prototype._bottomStartPointOffset)
    , _bottomTargetPointOffset(prototype._bottomTargetPointOffset)
    , _bottomRopePointOffset(prototype._bottomRopePointOffset)
{
}

HRESULT WorldUpDownRopeSpot::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

    return S_OK;
}

void WorldUpDownRopeSpot::BeginPlay()
{
    __super::BeginPlay();

    if (_ropeEffectAutoPlay)
        Play_RopeEffect();
}

void WorldUpDownRopeSpot::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if (_effectCom != nullptr)
        _effectCom->Update(timeDelta);
}

void WorldUpDownRopeSpot::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);
    Sync_RuntimeTuning();

    if (_topEntryCollider != nullptr && _topEntryCollider->Get_IsActive())
    {
        _topEntryCollider->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_topEntryCollider);
    }

    if (_bottomEntryCollider != nullptr && _bottomEntryCollider->Get_IsActive())
    {
        _bottomEntryCollider->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_bottomEntryCollider);
    }

    if (!Is_Visible() || _model == nullptr || _shader == nullptr)
        return;

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT WorldUpDownRopeSpot::Render()
{
    if (_model == nullptr || _shader == nullptr)
        return S_OK;

    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED(_shader->Reset_DrawFeatures(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
        CHECK_FAILED(_shader->Begin(material->Get_PassIndex()), E_FAIL);
        CHECK_FAILED(_model->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

HRESULT WorldUpDownRopeSpot::Render_Shadow()
{
    if (_model == nullptr || _shader == nullptr || _transformCom == nullptr)
        return S_OK;

    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_ShadowCB(_shader), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        CHECK_FAILED(_shader->Begin(1), E_FAIL);
        CHECK_FAILED(_model->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

bool WorldUpDownRopeSpot::Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const
{
    if (_model == nullptr || _transformCom == nullptr)
        return false;

    Vec3 localMin{}, localMax{};
    if (!_model->Try_Get_LocalBounds(localMin, localMax))
        return false;

    BoundingBox localBox{};
    BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));
    localBox.Transform(outWorldBounds, _model->Get_PreLocalTransformMatrix() * _transformCom->Get_WorldMatrix());

    return true;
}

void WorldUpDownRopeSpot::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnBeginOverlap(self, other);
    Set_ActiveEntryFromCollider(self, other);
}

void WorldUpDownRopeSpot::OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnStayOverlap(self, other);
    Set_ActiveEntryFromCollider(self, other);
}

void WorldUpDownRopeSpot::OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    __super::OnEndOverlap(self, other);

    const RopeEntry endedEntry = Resolve_EntryFromCollider(self);
    if (endedEntry == RopeEntry::None)
        return;

    Shared<Player_WorldBase> activeHero{};
    if (!Is_ActiveHeroCollider(other, &activeHero))
        return;

    if (activeHero == nullptr)
        return;

    if (_topEntryCollider != nullptr && _topEntryCollider->Is_Overlapping(other))
    {
        _activeEntry = RopeEntry::Top;
        if (!activeHero->Is_WorldInteracting())
            activeHero->Set_WorldInteractionCandidate(GetSharedPtr<GameObject>());
        return;
    }

    if (_bottomEntryCollider != nullptr && _bottomEntryCollider->Is_Overlapping(other))
    {
        _activeEntry = RopeEntry::Bottom;
        if (!activeHero->Is_WorldInteracting())
            activeHero->Set_WorldInteractionCandidate(GetSharedPtr<GameObject>());
        return;
    }

    if (_activeEntry == endedEntry)
        _activeEntry = RopeEntry::None;

    activeHero->Clear_WorldInteractionCandidate(this);
    activeHero->Clear_WorldInteractionRequest(this);
}

bool WorldUpDownRopeSpot::Can_Interact(const Shared<Player_WorldBase>& activeHero) const
{
    if (!__super::Can_Interact(activeHero))
        return false;

    return Is_EntryActive();
}

bool WorldUpDownRopeSpot::Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const
{
    if (!Can_Interact(activeHero))
        return false;

    const bool isTopEntry = _activeEntry == RopeEntry::Top;
    const Vec3 topPoint = Resolve_WorldPoint(_topColliderCenter);
    const Vec3 bottomPoint = Resolve_WorldPoint(_bottomColliderCenter);
    const Vec3 topIdlePoint = Build_RopeIdlePoint(_topRopeIdleY);
    const Vec3 bottomIdlePoint = Build_RopeIdlePoint(_bottomRopeIdleY);
    Vec3 downTargetRopePoint = bottomIdlePoint;
    if (isTopEntry && _transformCom != nullptr)
    {
        Vec3 localZ = Vec3::TransformNormal(Vec3::Look, _transformCom->Get_WorldMatrix());
        if (localZ.LengthSquared() > 0.0001f)
        {
            localZ.Normalize();
            downTargetRopePoint += localZ * 0.5f;
        }
    }

    outRequest = {};
    outRequest.type = WorldInteractionType::RopeTouch;
    outRequest.ropeDirection = isTopEntry ? RopeDirection::Down : RopeDirection::Up;
    outRequest.startPoint = isTopEntry ? topPoint : bottomPoint;
    outRequest.targetPoint = isTopEntry ? bottomPoint : topPoint;
    outRequest.wallPoint = Resolve_WorldPoint(_wallPointOffset);
    outRequest.ropePoint = isTopEntry ? topIdlePoint : bottomIdlePoint;
    outRequest.targetRopePoint = isTopEntry ? downTargetRopePoint : topIdlePoint;
    outRequest.warpEndProgress = std::clamp(_warpEndProgress, 0.01f, 1.f);

    return true;
}

InteractionPromptType WorldUpDownRopeSpot::Get_InteractionPromptType(const Shared<Player_WorldBase>& activeHero) const
{
    if (!Can_Interact(activeHero))
        return InteractionPromptType::None;

    if (_activeEntry == RopeEntry::Top)
        return InteractionPromptType::Down;

    if (_activeEntry == RopeEntry::Bottom)
        return InteractionPromptType::Up;

    return InteractionPromptType::None;
}

void WorldUpDownRopeSpot::Sync_RuntimeTuning()
{
    Sync_EntryCollider(
        _topEntryCollider,
        _topColliderCenter,
        _topColliderExtents,
        _lastSyncedTopColliderCenter,
        _lastSyncedTopColliderExtents,
        _hasSyncedTopCollider);

    Sync_EntryCollider(
        _bottomEntryCollider,
        _bottomColliderCenter,
        _bottomColliderExtents,
        _lastSyncedBottomColliderCenter,
        _lastSyncedBottomColliderExtents,
        _hasSyncedBottomCollider);

    Sync_RopeEffectSettings();
}

HRESULT WorldUpDownRopeSpot::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader), E_FAIL);
    CHECK_FAILED(Ready_ModelComponent(), E_FAIL);
    CHECK_FAILED(Ready_EffectComponent(), E_FAIL);
    CHECK_FAILED(Ready_EntryCollider(kTopEntryColliderTag, _topColliderCenter, _topColliderExtents, _topEntryCollider), E_FAIL);
    CHECK_FAILED(Ready_EntryCollider(kBottomEntryColliderTag, _bottomColliderCenter, _bottomColliderExtents, _bottomEntryCollider), E_FAIL);

    return S_OK;
}

void WorldUpDownRopeSpot::Sync_EntryCollider(
    const Shared<ColliderCom>& collider,
    Vec3& center,
    Vec3& extents,
    Vec3& lastSyncedCenter,
    Vec3& lastSyncedExtents,
    bool& hasSynced)
{
    if (collider == nullptr)
        return;

    ColliderShapeDesc desc = collider->Get_ShapeDesc();

    if (!hasSynced)
    {
        lastSyncedCenter = desc.center;
        lastSyncedExtents = desc.extents;
        center = desc.center;
        extents = desc.extents;
        hasSynced = true;
        return;
    }

    const bool colliderChanged =
        !NearlyEqual(desc.center, lastSyncedCenter) ||
        !NearlyEqual(desc.extents, lastSyncedExtents);
    const bool tuningChanged =
        !NearlyEqual(center, lastSyncedCenter) ||
        !NearlyEqual(extents, lastSyncedExtents);

    if (colliderChanged && !tuningChanged)
    {
        center = desc.center;
        extents = desc.extents;
        lastSyncedCenter = desc.center;
        lastSyncedExtents = desc.extents;
        return;
    }

    if (!tuningChanged)
        return;

    desc.center = center;
    desc.extents = extents;
    collider->Set_ShapeDesc(desc);
    lastSyncedCenter = center;
    lastSyncedExtents = extents;
}

void WorldUpDownRopeSpot::Sync_RopeEffectSettings()
{
    if (_effectCom == nullptr)
        return;

    EffectCom::InspectorEffectSettings settings = _effectCom->Get_InspectorEffectSettings();
    settings.effectName = _ropeEffectName;
    settings.localOffset = _ropeEffectOffset;
    settings.localRotation = _ropeEffectRotation;
    settings.scale = _ropeEffectScale;
    settings.followTarget = true;
    settings.requiresManualRelease = true;
    _effectCom->Set_InspectorEffectSettings(settings);
}

HRESULT WorldUpDownRopeSpot::Ready_ModelComponent()
{
    const std::filesystem::path modelPath =
        std::filesystem::path(GAME->Get_AssetRoot()) / kDefaultRopeModelPath;

    _model = ModelCom::Create(
        _device,
        _context,
        ModelType::NonAnim,
        String::ToString(modelPath.wstring()).c_str(),
        Matrix::CreateScale(0.001f));

    CHECK_NULL(_model, E_FAIL);

    Register_ModelComponent();

    return S_OK;
}

HRESULT WorldUpDownRopeSpot::Ready_EffectComponent()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    Sync_RopeEffectSettings();

    return S_OK;
}

void WorldUpDownRopeSpot::Register_ModelComponent()
{
    if (_model == nullptr)
        return;

    _model->Set_Owner(GetSharedPtr<GameObject>());

    if (_components.find(kRopeModelComponentTag) == _components.end())
        _components.emplace(kRopeModelComponentTag, static_pointer_cast<Component>(_model));
}

HRESULT WorldUpDownRopeSpot::Ready_EntryCollider(
    const wstring& componentTag,
    const Vec3& center,
    const Vec3& extents,
    Shared<ColliderCom>& outCollider)
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = center;
    aabbDesc.extents = extents;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), outCollider, &aabbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_AABB"), componentTag), E_FAIL);

    outCollider->Set_CollisionPreset(Collision_Preset::Trigger);
    outCollider->Set_IsActive(true);

    return S_OK;
}

HRESULT WorldUpDownRopeSpot::Bind_ShaderResources()
{
    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    return S_OK;
}

void WorldUpDownRopeSpot::Play_RopeEffect()
{
    if (_effectCom == nullptr)
        return;

    if (_ropeEffectName.empty())
        return;

    EffectCom::InspectorEffectSettings settings = _effectCom->Get_InspectorEffectSettings();
    settings.effectName = _ropeEffectName;
    settings.localOffset = _ropeEffectOffset;
    settings.localRotation = _ropeEffectRotation;
    settings.scale = _ropeEffectScale;
    settings.followTarget = true;
    settings.requiresManualRelease = true;
    _effectCom->Set_InspectorEffectSettings(settings);

    _effectCom->Stop_AllAttachedEffects();

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = _ropeEffectName;
    desc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    desc.layerTag = L"Layer_Effect";
    desc.localOffset = _ropeEffectOffset;
    desc.localRotation = _ropeEffectRotation;
    desc.scale = _ropeEffectScale;
    desc.followTarget = true;
    desc.requiresManualRelease = true;

    _effectCom->Play_Effect(desc);
}

void WorldUpDownRopeSpot::Set_ActiveEntryFromCollider(const Shared<ColliderCom>& self, const Shared<ColliderCom>& other)
{
    const RopeEntry entry = Resolve_EntryFromCollider(self);
    if (entry == RopeEntry::None)
        return;

    Shared<Player_WorldBase> activeHero{};
    if (!Is_ActiveHeroCollider(other, &activeHero))
        return;

    if (activeHero == nullptr || activeHero->Is_WorldInteracting())
        return;

    _activeEntry = entry;
    activeHero->Set_WorldInteractionCandidate(GetSharedPtr<GameObject>());
}

WorldUpDownRopeSpot::RopeEntry WorldUpDownRopeSpot::Resolve_EntryFromCollider(const Shared<ColliderCom>& collider) const
{
    if (collider == nullptr)
        return RopeEntry::None;

    if (collider.get() == _topEntryCollider.get())
        return RopeEntry::Top;

    if (collider.get() == _bottomEntryCollider.get())
        return RopeEntry::Bottom;

    return RopeEntry::None;
}

bool WorldUpDownRopeSpot::Is_EntryActive() const
{
    return _activeEntry == RopeEntry::Top || _activeEntry == RopeEntry::Bottom;
}

Vec3 WorldUpDownRopeSpot::Build_RopeIdlePoint(float idleY) const
{
    return Resolve_WorldPoint(Vec3(_ropeCenterXZ.x, idleY, _ropeCenterXZ.y));
}

Shared<WorldUpDownRopeSpot> WorldUpDownRopeSpot::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<WorldUpDownRopeSpot>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : WorldUpDownRope");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> WorldUpDownRopeSpot::Clone(void* arg)
{
    auto clone = make_shared<WorldUpDownRopeSpot>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : WorldUpDownRope");
        return nullptr;
    }

    return clone;
}

void WorldUpDownRopeSpot::Free()
{
    WorldInteractionSpot::Free();
}
