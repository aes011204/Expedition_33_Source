#include "PreviewPlayer.h"

#include "Body_Player.h"
#include "ClientInstance.h"
#include "EffectCom.h"
#include "FaceAccessory_Player.h"
#include "FacialAnimationModifier.h"
#include "FacialClip.h"
#include "GameDataTable.h"
#include "GameInstance.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "ResourceLoader.h"
#include "Spawn_Prefab.h"
#include "Weapon.h"
#include "WeaponPrefabLoader.h"

REGISTER_GLOBAL_ENUM(Client::PreviewAnimState, "PreviewAnimState")

IMPLEMENT_REFLECTION(PreviewPlayer)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PreviewPlayer";
    info.category = "UI Preview";

    PROPERTY_STRING("HeroId", _heroIdName);

    return true;
}

PreviewPlayer::PreviewPlayer(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : ContainerObject{ device, context }
{
}

PreviewPlayer::PreviewPlayer(const PreviewPlayer& prototype)
    : ContainerObject{ prototype }
{
}

HRESULT PreviewPlayer::Initialize_Prototype()
{
    return S_OK;
}

HRESULT PreviewPlayer::Initialize(void* arg)
{
    CHECK_FAILED(ContainerObject::Initialize(arg), E_FAIL);

    Shared<EffectCom> effectCom{};
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), effectCom), E_FAIL);

    if (arg != nullptr)
    {
        const PreviewPlayerDesc* desc = static_cast<PreviewPlayerDesc*>(arg);
        _heroId = desc->heroId;
        _heroIdName = Get_HeroIdName();
    }

    if (_heroId == HeroId::END && !_heroIdName.empty())
        _heroId = Parse_HeroIdName(_heroIdName);

    if (_heroId != HeroId::END)
    {
        CHECK_FAILED(Ready_DefaultCostumeParts(), E_FAIL);
        CHECK_FAILED(Sync_FromRuntimeData(), E_FAIL);
    }

    Set_Visible(false); // preview prefab은 선생성되어도 TAB 전 월드에 렌더되면 안 된다.
    return S_OK;
}

void PreviewPlayer::From_Json(const json& data)
{
    _changeWeaponPreviewWeaponOffsets.clear();

    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        _heroIdName = customProperties.value("_heroIdName", customProperties.value("HeroId", _heroIdName));
        _heroId = Parse_HeroIdName(_heroIdName);

        if (customProperties.contains("_changeWeaponPreviewWeaponOffsets") &&
            customProperties["_changeWeaponPreviewWeaponOffsets"].is_array())
        {
            for (const json& offsetJson : customProperties["_changeWeaponPreviewWeaponOffsets"])
            {
                if (!offsetJson.is_object())
                    continue;

                const string prefabName = offsetJson.value("prefabName", string{});
                if (prefabName.empty())
                    continue;

                PreviewWeaponTransformOffset offset{};

                if (offsetJson.contains("position") &&
                    offsetJson["position"].is_array() &&
                    offsetJson["position"].size() >= 3)
                {
                    offset.position.x = offsetJson["position"][0].get<float>();
                    offset.position.y = offsetJson["position"][1].get<float>();
                    offset.position.z = offsetJson["position"][2].get<float>();
                }

                if (offsetJson.contains("rotation") &&
                    offsetJson["rotation"].is_array() &&
                    offsetJson["rotation"].size() >= 3)
                {
                    offset.rotation.x = offsetJson["rotation"][0].get<float>();
                    offset.rotation.y = offsetJson["rotation"][1].get<float>();
                    offset.rotation.z = offsetJson["rotation"][2].get<float>();
                }

                _changeWeaponPreviewWeaponOffsets[prefabName] = offset;
            }
        }
    }

    if (_heroId == HeroId::END)
        _heroId = Parse_HeroIdName(_heroIdName);

    if (_heroId != HeroId::END && _partObjects.empty())
        CHECK_FAILED(Ready_DefaultCostumeParts());

    ContainerObject::From_Json(data);

    if (_heroId != HeroId::END)
        CHECK_FAILED(Sync_FromRuntimeData());

    Set_Visible(false);
}

void PreviewPlayer::BeginPlay()
{
    ContainerObject::BeginPlay();

    if (Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body"))))
        Apply_PreviewAnimStateMapping(body);

    Play_PreviewAnimation(
        PreviewAnimState::Overview_Intro, PreviewAnimState::Overview_Idle);
}

void PreviewPlayer::Update_Preview(float timeDelta, bool facialEnabled)
{
    Try_BeginPlay();

    if (_isPlayingPreviewIntro)
    {
        if (Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body"))))
        {
            if (body->Get_AnimationProgress() >= 0.95f &&
                body->Set_AnimationByState(ETOI(_pendingPreviewIdleAnim), 0.1f, true))
            {
                _isPlayingPreviewIntro = false;
                _pendingPreviewIdleAnim = PreviewAnimState::Overview_Idle;
            }
        }
    }

    Update_GameMenuFacialPreview(timeDelta, facialEnabled);

    // ObjectManager 기본 tick은 막고, preview가 열려 있을 때만 UIPreviewManager가 수동으로 part를 갱신한다.
    ContainerObject::Update(timeDelta);
    ContainerObject::Late_Update(timeDelta);

    if (const Shared<EffectCom> effectCom = Get_Component<EffectCom>())
        effectCom->Tick_AttachedEffectInstances(timeDelta); // 메뉴 pause 중 Layer_Effect가 멈춰도 preview trail emitter는 수동 갱신한다.
}

HRESULT PreviewPlayer::Render_PreviewParts()
{
    Try_BeginPlay();

    for (const auto& [partTag, partObject] : Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);

        if (partObject == nullptr || !partObject->Is_Visible())
            continue;

        CHECK_FAILED(partObject->Render(), E_FAIL);
    }

    return S_OK;
}

void PreviewPlayer::Play_PreviewAnimation(PreviewAnimState introAnim, PreviewAnimState idleAnim)
{
    _currentPreviewIntroAnim = introAnim;
    _currentPreviewIdleAnim = idleAnim;

    Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body")));
    if (body == nullptr)
        return;

    if (body->Set_AnimationByState(ETOI(introAnim), 0.25f, false))
    {
        _pendingPreviewIdleAnim = idleAnim;
        _isPlayingPreviewIntro = true;
        return;
    }

    body->Set_AnimationByState(ETOI(idleAnim), 0.25f, true);
    _isPlayingPreviewIntro = false;
    _pendingPreviewIdleAnim = idleAnim;
}

void PreviewPlayer::Update_GameMenuFacialPreview(float timeDelta, bool enabled)
{
    UNREFERENCED_PARAMETER(timeDelta);

    Head_Player* head = dynamic_cast<Head_Player*>(Get_PartObject(TEXT("Part_Head")));
    if (head == nullptr)
        return;

    const Shared<ModelCom> headModel = head->Get_Model();
    if (headModel == nullptr)
        return;

    if (!enabled)
    {
        if (_gameMenuFacialEnabled && _gameMenuFacialModifier != nullptr)
            _gameMenuFacialModifier->Set_Active(false);

        _gameMenuFacialEnabled = false;
        return;
    }

    if (_gameMenuFacialClip == nullptr)
    {
        const wchar_t* clipRelPath = Get_GameMenuFacialClipPath();
        if (clipRelPath == nullptr)
            return;

        const fs::path clipPath = fs::path(GAME->Get_AssetRoot()) / clipRelPath;
        Shared<FacialClip> clip = FacialClip::Create();
        if (clip == nullptr || FAILED(clip->Load_Binary(String::ToString(clipPath.wstring()))))
        {
            LOG_WARN("[PreviewPlayer] failed to load facial clip: {}", String::ToString(clipPath.wstring()));
            return;
        }

        _gameMenuFacialClip = clip;
    }

    if (_gameMenuFacialModifier == nullptr)
    {
        FacialAnimationModifier::FacialModifierDesc modifierDesc{};
        modifierDesc.modifierTag = L"PreviewPlayer_GameMenuFacial";
        modifierDesc.clip = _gameMenuFacialClip;
        modifierDesc.loop = true;
        modifierDesc.suppressMouthArea = true;

        _gameMenuFacialModifier = FacialAnimationModifier::Create(&modifierDesc);
        if (_gameMenuFacialModifier == nullptr)
            return;

        headModel->Add_AnimModifier(_gameMenuFacialModifier);
    }

    _gameMenuFacialEnabled = true;
    _gameMenuFacialModifier->Set_Active(true);
}

HRESULT PreviewPlayer::Sync_FromRuntimeData()
{
    if (CostumeManager* costumeManager = CLIENT->Get_CostumeManager())
    {
        if (const CostumePreset* preset = costumeManager->Get_CurrentCostume(_heroId))
            CHECK_FAILED(Apply_CostumePreset(*preset), E_FAIL);
    }

    Clear_WeaponPreview(); // ChangeWeapon 외 preview 패널에서는 장착 무기가 남아 보이면 안 된다.
    return S_OK;
}

HRESULT PreviewPlayer::Apply_CostumePreset(const CostumePreset& preset)
{
    if (preset.parts.bodyModelTag.empty() || preset.parts.headModelTag.empty())
        return E_FAIL;

    Capture_DefaultCostumePartTransformsIfNeeded();

    CHECK_FAILED(Ensure_CostumeVariantBuilt(preset), E_FAIL);
    CHECK_FAILED(Activate_CostumeVariant(preset), E_FAIL);

    return S_OK;
}

HRESULT PreviewPlayer::Prewarm_CostumeVariants()
{
    Try_BeginPlay(); // 숨겨진 의상 variant들도 아래에서 BeginPlay까지 미리 태우기 위한 선행 조건이다.

    CostumeManager* costumeManager = CLIENT->Get_CostumeManager();
    if (costumeManager == nullptr)
        return S_OK;

    const vector<CostumePreset>* presets = costumeManager->Get_Costumes(_heroId);
    if (presets == nullptr)
        return S_OK;

    for (const CostumePreset& preset : *presets)
    {
        if (FAILED(Ensure_CostumeVariantBuilt(preset)))
        {
            LOG_WARN(
                "PreviewPlayer: skip costume prewarm. hero={}, preset={}",
                static_cast<int32>(_heroId),
                preset.presetId);
            continue; // preview 선로딩 실패는 패널 오픈 자체를 막지 않는다.
        }

        const auto iter = _costumeVariantsByPreset.find(preset.presetId);
        if (iter != _costumeVariantsByPreset.end())
        {
            if (FAILED(Warmup_CostumeVariantParts(iter->second)))
            {
                LOG_WARN(
                    "PreviewPlayer: skip costume part warmup. hero={}, preset={}",
                    static_cast<int32>(_heroId),
                    preset.presetId);
            }
        }
    }

    return S_OK;
}

HRESULT PreviewPlayer::Ready_DefaultCostumeParts()
{
    Body_Player::BODY_PLAYER_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = Get_DefaultBodyModelTag();

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Body_Player"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body")));
    CHECK_NULL(body, E_FAIL);

    Head_Player::HEAD_PLAYER_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = Get_DefaultHeadModelTag();
    headDesc.socketMatrixPointer = nullptr;

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Head_Player"), TEXT("Part_Head"), &headDesc), E_FAIL);

    Head_Player* head = dynamic_cast<Head_Player*>(Get_PartObject(TEXT("Part_Head")));
    CHECK_NULL(head, E_FAIL);
    head->Set_LeaderPoseComponent(body);

    if (const wchar_t* beardModelTag = Get_DefaultBeardModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC beardDesc{};
        beardDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
        beardDesc.modelType = ModelType::NonAnim;
        beardDesc.modelTag = beardModelTag;
        beardDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");
        CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_FaceAccessory_Player"), TEXT("Part_Beard"), &beardDesc), E_FAIL);
    }

    if (const wchar_t* mustacheModelTag = Get_DefaultMustacheModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC mustacheDesc{};
        mustacheDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
        mustacheDesc.modelType = ModelType::NonAnim;
        mustacheDesc.modelTag = mustacheModelTag;
        mustacheDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");
        CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_FaceAccessory_Player"), TEXT("Part_Mustache"), &mustacheDesc), E_FAIL);
    }

    if (const wchar_t* hairModelTag = Get_DefaultHairModelTag())
    {
        Hair_Player::HAIR_PLAYER_DESC hairDesc{};
        hairDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
        hairDesc.modelType = ModelType::Anim;
        hairDesc.modelTag = hairModelTag;
        hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
        hairDesc.usePoseFollowModifier = true;
        hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");
        CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), TEXT("Part_Hair"), &hairDesc), E_FAIL);
    }

    Capture_DefaultCostumePartTransformsIfNeeded();

    CostumeVariantParts defaultParts{};
    defaultParts.body = dynamic_pointer_cast<Body_Player>(_partObjects[TEXT("Part_Body")]);
    defaultParts.head = dynamic_pointer_cast<Head_Player>(_partObjects[TEXT("Part_Head")]);

    if (const auto iter = _partObjects.find(TEXT("Part_Hair")); iter != _partObjects.end())
        defaultParts.hair = dynamic_pointer_cast<Hair_Player>(iter->second);
    if (const auto iter = _partObjects.find(TEXT("Part_Beard")); iter != _partObjects.end())
        defaultParts.beard = dynamic_pointer_cast<FaceAccessory_Player>(iter->second);
    if (const auto iter = _partObjects.find(TEXT("Part_Mustache")); iter != _partObjects.end())
        defaultParts.mustache = dynamic_pointer_cast<FaceAccessory_Player>(iter->second);
    if (const auto iter = _partObjects.find(TEXT("Part_Glasses")); iter != _partObjects.end())
        defaultParts.glasses = dynamic_pointer_cast<FaceAccessory_Player>(iter->second);

    defaultParts.baseTransforms = _defaultCostumePartTransforms;
    defaultParts.presetTransforms = _defaultCostumePartTransforms;

    if (CostumeManager* costumeManager = CLIENT->Get_CostumeManager())
    {
        if (const CostumePreset* defaultPreset = costumeManager->Get_Costume(_heroId, "Default"))
            defaultParts.presetTransforms = Build_CostumePresetTransforms(*defaultPreset, defaultParts.baseTransforms);
    }

    _costumeVariantsByPreset["Default"] = defaultParts;
    _activeCostumePresetId = "Default";

    return S_OK;
}

HRESULT PreviewPlayer::Ensure_CostumeVariantBuilt(const CostumePreset& preset)
{
    if (_costumeVariantsByPreset.contains(preset.presetId))
        return S_OK;

    CostumeVariantParts variant{};
    variant.body = Create_CostumeBodyVariant(preset.parts.bodyModelTag);
    CHECK_NULL(variant.body, E_FAIL);

    variant.head = Create_CostumeHeadVariant(preset.parts.headModelTag, variant.body.get());
    CHECK_NULL(variant.head, E_FAIL);

    variant.beard = Create_CostumeFaceAccessoryVariant(preset.parts.beardModelTag, variant.head.get());
    variant.mustache = Create_CostumeFaceAccessoryVariant(preset.parts.mustacheModelTag, variant.head.get());

    variant.hair = Create_CostumeHairVariant(preset.parts.hairModelTag, variant.head.get());
    if (!preset.parts.hairModelTag.empty())
        CHECK_NULL(variant.hair, E_FAIL);

    variant.glasses = Create_CostumeFaceAccessoryVariant(preset.parts.glassesModelTag, variant.head.get());
    variant.baseTransforms = Capture_CostumeVariantPartTransforms(variant);
    variant.presetTransforms = Build_CostumePresetTransforms(preset, variant.baseTransforms);

    const auto ApplyDirectTransform = [&variant](const wstring& partTag, PartObject* partObject)
    {
        if (partObject == nullptr || partObject->Get_Transform() == nullptr)
            return;

        const auto iter = variant.presetTransforms.find(partTag);
        if (iter == variant.presetTransforms.end())
            return;

        partObject->Get_Transform()->Set_Position(iter->second.position);
        partObject->Get_Transform()->Set_RotationEuler(iter->second.rotation);
        partObject->Get_Transform()->Set_Scale(iter->second.scale);
    };

    ApplyDirectTransform(TEXT("Part_Body"), variant.body.get());
    ApplyDirectTransform(TEXT("Part_Head"), variant.head.get());
    ApplyDirectTransform(TEXT("Part_Beard"), variant.beard.get());
    ApplyDirectTransform(TEXT("Part_Mustache"), variant.mustache.get());
    ApplyDirectTransform(TEXT("Part_Hair"), variant.hair.get());
    ApplyDirectTransform(TEXT("Part_Glasses"), variant.glasses.get());

    if (variant.body)
        variant.body->Set_Visible(false);
    if (variant.head)
        variant.head->Set_Visible(false);
    if (variant.hair)
        variant.hair->Set_Visible(false);
    if (variant.beard)
        variant.beard->Set_Visible(false);
    if (variant.mustache)
        variant.mustache->Set_Visible(false);
    if (variant.glasses)
        variant.glasses->Set_Visible(false);

    _costumeVariantsByPreset.emplace(preset.presetId, move(variant));
    return S_OK;
}

HRESULT PreviewPlayer::Activate_CostumeVariant(const CostumePreset& preset)
{
    const auto iter = _costumeVariantsByPreset.find(preset.presetId);
    if (iter == _costumeVariantsByPreset.end())
        return E_FAIL;

    float resumeFrame = 0.f;
    if (Body_Player* currentBody = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body"))))
    {
        if (const Shared<ModelCom> currentModel = currentBody->Get_Model())
            resumeFrame = currentModel->Get_CurrentFrame(); // 의상 교체 전 애니메이션 위치를 새 바디에 이어받는다.
    }

    for (auto& [cachedPresetId, parts] : _costumeVariantsByPreset)
    {
        const bool isTarget = cachedPresetId == preset.presetId;

        if (parts.body)
            parts.body->Set_Visible(isTarget);
        if (parts.head)
            parts.head->Set_Visible(isTarget);
        if (parts.hair)
            parts.hair->Set_Visible(isTarget);
        if (parts.beard)
            parts.beard->Set_Visible(isTarget);
        if (parts.mustache)
            parts.mustache->Set_Visible(isTarget);
        if (parts.glasses)
            parts.glasses->Set_Visible(isTarget);
    }

    if (_activeCostumePresetId != preset.presetId)
    {
        _activeCostumePresetId = preset.presetId;
        Rebind_ActiveCostumeVariant(_activeCostumePresetId);
    }

    Apply_CostumePartTransforms(iter->second.presetTransforms);
    Reapply_CostumeBaseMaterials();

    if (Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body"))))
        Apply_PreviewAnimStateMapping(body);

    Reapply_CurrentPreviewAnimation(resumeFrame);
    return S_OK;
}

HRESULT PreviewPlayer::Apply_EquippedWeaponFromRuntimeData()
{
    const Shared<Player_WorldBase> worldHero = CLIENT->Find_Hero(_heroId);
    const Shared<PlayerStatCom> statCom = worldHero ? worldHero->Get_Component<PlayerStatCom>() : nullptr;
    if (statCom == nullptr)
        return S_OK;

    string weaponId = statCom->Get_PlayerRuntimeStat().CurEquippedWeaponId;
    const GameDataTable::WeaponDesc* weaponDesc = CLIENT->Get_WeaponDesc(Get_CharacterId(), weaponId);

    if (weaponDesc == nullptr)
    {
        const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs(Get_CharacterId());
        if (weapons == nullptr || weapons->empty())
            return S_OK;

        weaponDesc = &weapons->front();
        weaponId = weaponDesc->weaponId;
    }

    if (weaponId == _activeWeaponId)
        return S_OK;

    Clear_WeaponPreview();
    _activeWeaponId = weaponId;

    CHECK_FAILED(Attach_WeaponPreview(TEXT("Part_Weapon"), weaponDesc->weaponPrefabName, Get_MainWeaponSocketName()), E_FAIL);

    if (!weaponDesc->subWeaponPrefabName.empty())
        CHECK_FAILED(Attach_WeaponPreview(TEXT("Part_WeaponL"), weaponDesc->subWeaponPrefabName, "Weapon_L"), E_FAIL);

    return S_OK;
}

HRESULT PreviewPlayer::Attach_WeaponPreview(const wstring& partTag, const string& prefabName, const string& socketName)
{
    if (prefabName.empty())
        return S_OK;

    Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body")));
    CHECK_NULL(body, E_FAIL);

    WeaponPrefabLoader::LoadedWeaponPrefab loadedPrefab{};
    if (!WeaponPrefabLoader::Load(prefabName, loadedPrefab))
        return E_FAIL;

    const Matrix* socketMatrix = nullptr;
    if (!socketName.empty())
    {
        socketMatrix = body->Get_SocketBoneMatrixPtr(socketName);
        CHECK_NULL(socketMatrix, E_FAIL);
    }

    Weapon::WEAPON_DESC desc{};
    desc.parentMatrix = &_transformCom->Get_WorldMatrix();
    desc.socketMatrix = socketMatrix;
    desc.modelType = loadedPrefab.modelType;
    desc.modelTag = loadedPrefab.modelTag.c_str();
    desc.applyAttachScaleCompensation = true; // ChangeWeapon preview 장착 weapon도 Battle과 같은 크기 보정을 쓴다.

    Shared<Weapon> weapon = dynamic_pointer_cast<Weapon>(
        GAME->Clone_Prototype(Prototype::GameObject, ETOI(LevelType::Static), TEXT("GameObject_Weapon"), &desc));
    CHECK_NULL(weapon, E_FAIL);

    weapon->From_Json(loadedPrefab.rootJson);
    Apply_ChangeWeaponPreviewWeaponOffset(weapon.get(), prefabName); // PP_* prefab에 선언된 ChangeWeapon 전용 보정만 여기서 더한다.
    weapon->Set_SourcePrefabName(prefabName);
    weapon->Set_Owner(GetSharedPtr<GameObject>());

    if (partTag == TEXT("Part_WeaponL"))
        _subWeapon = weapon;
    else
        _weapon = weapon;

    _partObjects.emplace(partTag, weapon);
    _partObjectUpdateOrder.push_back(weapon);

    if (_hasBegunPlay)
        CHECK_FAILED(weapon->Try_BeginPlay(), E_FAIL);

    return S_OK;
}

void PreviewPlayer::Apply_ChangeWeaponPreviewWeaponOffset(Weapon* weapon, const string& prefabName) const
{
    if (weapon == nullptr || weapon->Get_Transform() == nullptr)
        return;

    const auto iter = _changeWeaponPreviewWeaponOffsets.find(prefabName);
    if (iter == _changeWeaponPreviewWeaponOffsets.end())
        return;

    const auto transform = weapon->Get_Transform();
    transform->Set_Position(transform->Get_Position() + iter->second.position);
    transform->Set_RotationEuler(transform->Get_RotationEuler() + iter->second.rotation);
}

void PreviewPlayer::Rebind_ActiveCostumeVariant(const string& presetId)
{
    const auto iter = _costumeVariantsByPreset.find(presetId);
    if (iter == _costumeVariantsByPreset.end())
        return;

    const CostumeVariantParts& parts = iter->second;

    _partObjects.clear();
    _partObjectUpdateOrder.clear();

    const auto RebindOne = [this](const wstring& partTag, const Shared<PartObject>& partObject)
    {
        if (partObject == nullptr)
            return;

        partObject->Set_Owner(GetSharedPtr<GameObject>());
        _partObjects.emplace(partTag, partObject);
        _partObjectUpdateOrder.push_back(partObject);

        if (_hasBegunPlay)
            CHECK_FAILED(partObject->Try_BeginPlay());
    };

    RebindOne(TEXT("Part_Body"), static_pointer_cast<PartObject>(parts.body));
    RebindOne(TEXT("Part_Head"), static_pointer_cast<PartObject>(parts.head));
    RebindOne(TEXT("Part_Beard"), static_pointer_cast<PartObject>(parts.beard));
    RebindOne(TEXT("Part_Mustache"), static_pointer_cast<PartObject>(parts.mustache));
    RebindOne(TEXT("Part_Hair"), static_pointer_cast<PartObject>(parts.hair));
    RebindOne(TEXT("Part_Glasses"), static_pointer_cast<PartObject>(parts.glasses));

    _weapon.reset();
    _subWeapon.reset();
    _activeWeaponId.clear();
}

HRESULT PreviewPlayer::Warmup_CostumeVariantParts(CostumeVariantParts& parts)
{
    if (!_hasBegunPlay)
        return S_OK;

    const auto WarmupOne = [](const Shared<PartObject>& partObject) -> HRESULT
    {
        if (partObject == nullptr)
            return S_OK;

        CHECK_FAILED(partObject->Try_BeginPlay(), E_FAIL);
        return S_OK;
    };

    CHECK_FAILED(WarmupOne(static_pointer_cast<PartObject>(parts.body)), E_FAIL);
    CHECK_FAILED(WarmupOne(static_pointer_cast<PartObject>(parts.head)), E_FAIL);
    CHECK_FAILED(WarmupOne(static_pointer_cast<PartObject>(parts.beard)), E_FAIL);
    CHECK_FAILED(WarmupOne(static_pointer_cast<PartObject>(parts.mustache)), E_FAIL);
    CHECK_FAILED(WarmupOne(static_pointer_cast<PartObject>(parts.hair)), E_FAIL);
    CHECK_FAILED(WarmupOne(static_pointer_cast<PartObject>(parts.glasses)), E_FAIL);

    return S_OK;
}

void PreviewPlayer::Clear_AllCostumeVariants()
{
    for (auto& [presetId, parts] : _costumeVariantsByPreset)
    {
        UNREFERENCED_PARAMETER(presetId);

        if (parts.glasses)
            parts.glasses->Free();
        if (parts.hair)
            parts.hair->Free();
        if (parts.mustache)
            parts.mustache->Free();
        if (parts.beard)
            parts.beard->Free();
        if (parts.head)
            parts.head->Free();
        if (parts.body)
            parts.body->Free();
    }

    _costumeVariantsByPreset.clear();
    _activeCostumePresetId.clear();
    _partObjects.clear();
    _partObjectUpdateOrder.clear();
}

void PreviewPlayer::Clear_WeaponPreview()
{
    Remove_PartObject(TEXT("Part_Weapon"));
    Remove_PartObject(TEXT("Part_WeaponL"));

    _weapon.reset();
    _subWeapon.reset();
    _activeWeaponId.clear();
}

void PreviewPlayer::Capture_DefaultCostumePartTransformsIfNeeded()
{
    if (!_defaultCostumePartTransforms.empty())
        return;

    for (const auto& [partTag, partObject] : _partObjects)
    {
        if (partObject == nullptr || partObject->Get_Transform() == nullptr)
            continue;

        CostumePartTransform transform{};
        transform.position = partObject->Get_Transform()->Get_Position();
        transform.rotation = partObject->Get_Transform()->Get_RotationEuler();
        transform.scale = partObject->Get_Transform()->Get_Scale();

        _defaultCostumePartTransforms.emplace(partTag, transform);
    }
}

PreviewPlayer::CostumePartTransformMap PreviewPlayer::Capture_CostumeVariantPartTransforms(const CostumeVariantParts& parts) const
{
    CostumePartTransformMap transforms{};

    const auto CaptureOne = [&transforms](const wstring& partTag, PartObject* partObject)
    {
        if (partObject == nullptr || partObject->Get_Transform() == nullptr)
            return;

        CostumePartTransform transform{};
        transform.position = partObject->Get_Transform()->Get_Position();
        transform.rotation = partObject->Get_Transform()->Get_RotationEuler();
        transform.scale = partObject->Get_Transform()->Get_Scale();

        transforms.emplace(partTag, transform);
    };

    CaptureOne(TEXT("Part_Body"), parts.body.get());
    CaptureOne(TEXT("Part_Head"), parts.head.get());
    CaptureOne(TEXT("Part_Beard"), parts.beard.get());
    CaptureOne(TEXT("Part_Mustache"), parts.mustache.get());
    CaptureOne(TEXT("Part_Hair"), parts.hair.get());
    CaptureOne(TEXT("Part_Glasses"), parts.glasses.get());

    return transforms;
}

PreviewPlayer::CostumePartTransformMap PreviewPlayer::Build_CostumePresetTransforms(
    const CostumePreset& preset,
    const CostumePartTransformMap& baseTransforms) const
{
    CostumePartTransformMap transforms = baseTransforms;

    const auto ApplyOverride = [&transforms](const wstring& partTag, const CostumePartLocalTransform& fallbackTransform)
    {
        if (!fallbackTransform.enabled)
            return;

        CostumePartTransform transform{};
        transform.position = fallbackTransform.position;
        transform.rotation = fallbackTransform.rotation;
        transform.scale = fallbackTransform.scale;
        transforms[partTag] = transform;
    };

    ApplyOverride(TEXT("Part_Body"), preset.parts.bodyTransform);
    ApplyOverride(TEXT("Part_Head"), preset.parts.headTransform);
    ApplyOverride(TEXT("Part_Beard"), preset.parts.beardTransform);
    ApplyOverride(TEXT("Part_Mustache"), preset.parts.mustacheTransform);
    ApplyOverride(TEXT("Part_Hair"), preset.parts.hairTransform);
    ApplyOverride(TEXT("Part_Glasses"), preset.parts.glassesTransform);

    return transforms;
}

void PreviewPlayer::Apply_CostumePartTransforms(const CostumePartTransformMap& partTransforms)
{
    Apply_CostumePartTransform(TEXT("Part_Body"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Head"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Beard"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Mustache"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Hair"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Glasses"), partTransforms);
}

void PreviewPlayer::Apply_CostumePartTransform(const wstring& partTag, const CostumePartTransformMap& partTransforms)
{
    const auto iter = partTransforms.find(partTag);
    if (iter == partTransforms.end())
        return;

    PartObject* partObject = Get_PartObject(partTag);
    if (partObject == nullptr || partObject->Get_Transform() == nullptr)
        return;

    partObject->Get_Transform()->Set_Position(iter->second.position);
    partObject->Get_Transform()->Set_RotationEuler(iter->second.rotation);
    partObject->Get_Transform()->Set_Scale(iter->second.scale);
}

void PreviewPlayer::Apply_CostumePartTransform(
    const wstring& partTag,
    const CostumePartTransformMap& partTransforms,
    const CostumePartLocalTransform& fallbackTransform)
{
    if (fallbackTransform.enabled)
    {
        PartObject* partObject = Get_PartObject(partTag);
        if (partObject == nullptr || partObject->Get_Transform() == nullptr)
            return;

        partObject->Get_Transform()->Set_Position(fallbackTransform.position);
        partObject->Get_Transform()->Set_RotationEuler(fallbackTransform.rotation);
        partObject->Get_Transform()->Set_Scale(fallbackTransform.scale);
        return;
    }

    Apply_CostumePartTransform(partTag, partTransforms);
}

void PreviewPlayer::Reapply_CostumePartBaseMaterial(const wstring& partTag)
{
    PartObject* partObject = Get_PartObject(partTag);
    if (partObject == nullptr)
        return;

    Shared<ModelCom> model{};

    if (auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
        model = playablePart->Get_Model();
    else if (auto* faceAccessory = dynamic_cast<FaceAccessory_Player*>(partObject))
        model = faceAccessory->Get_Model();

    if (model == nullptr)
        return;

    model->Apply_BaseMaterialFromBundle_AllSlots();
    (void)model->Normalize_OwnerComponentTagAndBindings();
}

void PreviewPlayer::Reapply_CostumeBaseMaterials()
{
    Reapply_CostumePartBaseMaterial(TEXT("Part_Body"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Head"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Beard"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Mustache"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Hair"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Glasses"));
}

void PreviewPlayer::Reapply_CurrentPreviewAnimation(float startFrame)
{
    Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body")));
    if (body == nullptr)
        return;

    const Shared<ModelCom> model = body->Get_Model();
    if (model == nullptr)
        return;

    if (_isPlayingPreviewIntro)
    {
        if (model->Set_AnimationByState(ETOI(_currentPreviewIntroAnim), false, startFrame))
        {
            _pendingPreviewIdleAnim = _currentPreviewIdleAnim;
            return;
        }
    }

    if (model->Set_AnimationByState(ETOI(_currentPreviewIdleAnim), true, startFrame))
    {
        _isPlayingPreviewIntro = false;
        _pendingPreviewIdleAnim = _currentPreviewIdleAnim;
    }
}

Shared<Body_Player> PreviewPlayer::Create_CostumeBodyVariant(const wstring& modelTag)
{
    if (modelTag.empty())
        return nullptr;

    Body_Player::BODY_PLAYER_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = modelTag;

    Shared<PartObject> clonedPart = static_pointer_cast<PartObject>(
        GAME->Clone_Prototype(Prototype::GameObject, ETOI(LevelType::Static), TEXT("GameObject_Body_Player"), &bodyDesc));
    Shared<Body_Player> body = dynamic_pointer_cast<Body_Player>(clonedPart);
    if (body == nullptr)
        return nullptr;

    body->Set_Owner(GetSharedPtr<GameObject>());

    if (Shared<ModelCom> model = body->Get_Model())
    {
        Apply_PreviewAnimStateMapping(body.get());
        model->Apply_BaseMaterialFromBundle_AllSlots();
        (void)model->Normalize_OwnerComponentTagAndBindings();
    }

    return body;
}

Shared<Head_Player> PreviewPlayer::Create_CostumeHeadVariant(const wstring& modelTag, const Body_Player* body)
{
    if (modelTag.empty() || body == nullptr)
        return nullptr;

    Head_Player::HEAD_PLAYER_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = modelTag;
    headDesc.socketMatrixPointer = nullptr;

    Shared<PartObject> clonedPart = static_pointer_cast<PartObject>(
        GAME->Clone_Prototype(Prototype::GameObject, ETOI(LevelType::Static), TEXT("GameObject_Head_Player"), &headDesc));
    Shared<Head_Player> head = dynamic_pointer_cast<Head_Player>(clonedPart);
    if (head == nullptr)
        return nullptr;

    head->Set_Owner(GetSharedPtr<GameObject>());
    head->Set_LeaderPoseComponent(body);
    return head;
}

Shared<Hair_Player> PreviewPlayer::Create_CostumeHairVariant(const wstring& modelTag, const Head_Player* head)
{
    if (modelTag.empty())
        return nullptr;

    CHECK_NULL(head, nullptr);

    Hair_Player::HAIR_PLAYER_DESC hairDesc{};
    hairDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    hairDesc.modelType = ModelType::Anim;
    hairDesc.modelTag = modelTag;
    hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairDesc.usePoseFollowModifier = true;
    hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    Shared<PartObject> clonedPart = static_pointer_cast<PartObject>(
        GAME->Clone_Prototype(Prototype::GameObject, ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), &hairDesc));
    Shared<Hair_Player> hair = dynamic_pointer_cast<Hair_Player>(clonedPart);
    if (hair == nullptr)
        return nullptr;

    hair->Set_Owner(GetSharedPtr<GameObject>());
    return hair;
}

Shared<FaceAccessory_Player> PreviewPlayer::Create_CostumeFaceAccessoryVariant(const wstring& modelTag, const Head_Player* head)
{
    if (modelTag.empty())
        return nullptr;

    CHECK_NULL(head, nullptr);

    FaceAccessory_Player::FACE_ACCESSORY_DESC accessoryDesc{};
    accessoryDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    accessoryDesc.modelType = ModelType::NonAnim;
    accessoryDesc.modelTag = modelTag.c_str();
    accessoryDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    Shared<PartObject> clonedPart = static_pointer_cast<PartObject>(
        GAME->Clone_Prototype(Prototype::GameObject, ETOI(LevelType::Static), TEXT("GameObject_FaceAccessory_Player"), &accessoryDesc));
    Shared<FaceAccessory_Player> accessory = dynamic_pointer_cast<FaceAccessory_Player>(clonedPart);
    if (accessory == nullptr)
        return nullptr;

    accessory->Set_Owner(GetSharedPtr<GameObject>());
    return accessory;
}

HeroId PreviewPlayer::Parse_HeroIdName(const string& heroIdName) const
{
    if (heroIdName == "Gustave")
        return HeroId::Gustave;
    if (heroIdName == "Lune")
        return HeroId::Lune;
    if (heroIdName == "Maelle")
        return HeroId::Maelle;

    return HeroId::END;
}

string PreviewPlayer::Get_HeroIdName() const
{
    switch (_heroId)
    {
    case HeroId::Gustave: return "Gustave";
    case HeroId::Lune:    return "Lune";
    case HeroId::Maelle:  return "Maelle";
    default:              return "END";
    }
}

string PreviewPlayer::Get_CharacterId() const
{
    return Get_HeroIdName();
}

string PreviewPlayer::Get_MainWeaponSocketName() const
{
    switch (_heroId)
    {
    case HeroId::Gustave:
        return "Weapon_R";
    case HeroId::Lune:
        return "";
    case HeroId::Maelle:
        return "Weapon_r";
    default:
        return "Weapon_R";
    }
}

wstring PreviewPlayer::Get_PreviewAnimMapPath() const
{
    fs::path path = fs::path(GAME->Get_AssetRoot()) / L"Models" / L"Heros";

    switch (_heroId)
    {
    case HeroId::Gustave:
        path /= L"Gustave/GustaveBody/PreviewAnimState.animmap.json";
        break;
    case HeroId::Lune:
        path /= L"Lune/LuneBody/PreviewAnimState.animmap.json";
        break;
    case HeroId::Maelle:
        path /= L"Maelle/MaelleBody/PreviewAnimState.animmap.json";
        break;
    default:
        return L"";
    }

    return path.wstring();
}

bool PreviewPlayer::Apply_PreviewAnimStateMapping(Body_Player* body) const
{
    if (body == nullptr)
        return false;

    const Shared<ModelCom> model = body->Get_Model();
    if (model == nullptr)
        return false;

    const wstring animMapPath = Get_PreviewAnimMapPath();
    if (animMapPath.empty())
        return false;

    return model->Load_AnimStateMappingFromFile(animMapPath);
}

HairChainPhysicsDesc PreviewPlayer::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    switch (_heroId)
    {
    case HeroId::Gustave:
        desc.profile = HairPhysicsProfile::Gustave;
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
        break;

    case HeroId::Maelle:
        desc.profile = HairPhysicsProfile::Maelle;
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
        break;

    case HeroId::Lune:
    default:
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
        break;
    }

    return desc;
}

const wchar_t* PreviewPlayer::Get_DefaultBodyModelTag() const
{
    switch (_heroId)
    {
    case HeroId::Gustave: return L"Model_GustaveBody";
    case HeroId::Lune:    return L"Model_LuneBody";
    case HeroId::Maelle:  return L"Model_MaelleBody";
    default:              return L"Model_LuneBody";
    }
}

const wchar_t* PreviewPlayer::Get_DefaultHeadModelTag() const
{
    switch (_heroId)
    {
    case HeroId::Gustave: return L"Model_GustaveHead";
    case HeroId::Lune:    return L"Model_LuneHead";
    case HeroId::Maelle:  return L"Model_MaelleHead";
    default:              return L"Model_LuneHead";
    }
}

const wchar_t* PreviewPlayer::Get_DefaultHairModelTag() const
{
    switch (_heroId)
    {
    case HeroId::Gustave: return L"Model_GustaveHair";
    case HeroId::Lune:    return L"Model_LuneHair";
    case HeroId::Maelle:  return L"Model_MaelleHair";
    default:              return L"Model_LuneHair";
    }
}

const wchar_t* PreviewPlayer::Get_DefaultBeardModelTag() const
{
    switch (_heroId)
    {
    case HeroId::Gustave:
        // return L"Model_GustaveBeard";
        return nullptr;
    default:
        return nullptr;
    }
}

const wchar_t* PreviewPlayer::Get_DefaultMustacheModelTag() const
{
    switch (_heroId)
    {
    case HeroId::Gustave:
        // return L"Model_GustaveMustache";
        return nullptr;
    default:
        return nullptr;
    }
}

const wchar_t* PreviewPlayer::Get_GameMenuFacialClipPath() const
{
    return L"Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu_Lune.facialclipbin";
}

Shared<PreviewPlayer> PreviewPlayer::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PreviewPlayer>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : PreviewPlayer");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> PreviewPlayer::Clone(void* arg)
{
    auto instance = make_shared<PreviewPlayer>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : PreviewPlayer");
        return nullptr;
    }

    return instance;
}

void PreviewPlayer::Free()
{
    if (_gameMenuFacialModifier)
        _gameMenuFacialModifier->Set_Active(false);

    _gameMenuFacialModifier.reset();
    _gameMenuFacialClip.reset();

    _isPlayingPreviewIntro = false;
    _pendingPreviewIdleAnim = PreviewAnimState::Overview_Idle;

    Clear_WeaponPreview();
    Clear_AllCostumeVariants();

    ContainerObject::Free();
}
