#include "Player_WorldBase.h"
#include "Weapon.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "InputCom.h"
#include "MovementCom.h"
#include "ModelCom.h"
#include "PlayerControllerCom.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayerStatCom.h"
#include "PlayableParts.h"
#include "PlayerStateMachineCom.h"
#include "Body_Player.h"
#include "EffectCom.h"
#include "FaceAccessory_Player.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "FacialAnimationModifier.h"
#include "FacialClip.h"
#include "ModelCom.h"
#include "Player_GameMenu.h"
#include "Player_Jump.h"
#include "Player_ParkourAction.h"
#include "SkillTreeCom.h"
#include "CostumeManager.h"
#include "TransformCom.h"

namespace Client::PlayerWorldTuning
{
    constexpr float kFallStateEnterDelaySec = 0.10f;
    constexpr float kFallStateEnterSpeedY = -2.4f;
    constexpr wchar_t kGameMenuFacialModifierTag[] = L"GameMenuFacialPreview";

    constexpr array<Vec3, 5> kDebugWarpPoints =
    {
        Vec3(-26.04f, 15.27f, 86.57f),
        Vec3(-25.69f, 9.59f, 74.02f),
        Vec3(-49.02f, 9.22f, 78.26f),
        Vec3(-66.56f, 15.31f, 57.68f),
        Vec3(-79.30f, 18.26f, 52.95f),
    };

    bool Is_TransformOwnedState(PlayerState state)
    {
        return state == PlayerState::Parkour ||
            state == PlayerState::Climb ||
            state == PlayerState::RopeAction ||
            state == PlayerState::GrappleAction;
    }

    bool Try_EnterParkourFromJumpIntent(
        const Shared<PlayerStateMachineCom>& stateMachineCom,
        Player_ParkourAction::ParkourActionType actionType)
    {
        if (stateMachineCom == nullptr)
            return false;

        const Shared<Player_ParkourAction> parkourAction =
            dynamic_pointer_cast<Player_ParkourAction>(stateMachineCom->Get_State(PlayerState::Parkour));
        if (parkourAction == nullptr)
            return false;

        if (!parkourAction->Try_PrepareAction(actionType))
            return false;

        const bool changed = stateMachineCom->Change_State(PlayerState::Parkour);
        if (!changed)
            parkourAction->Clear_PreparedAction();

        return changed;
    }

    bool Try_EnterParkourFromJumpIntent(const Shared<PlayerStateMachineCom>& stateMachineCom)
    {
        if (Try_EnterParkourFromJumpIntent(stateMachineCom, Player_ParkourAction::ParkourActionType::LowMantle))
            return true;

        return Try_EnterParkourFromJumpIntent(stateMachineCom, Player_ParkourAction::ParkourActionType::HighMantle);
    }

    struct BodyAnimationSnapshot
    {
        bool valid = false;
        uint32 stateIndex = UINT32_MAX;
        float timeSec = 0.f;
        float frameSpeed = 1.f;
        bool isLoop = false;
    };

    BodyAnimationSnapshot Capture_BodyAnimationSnapshot(const Body_Player* body)
    {
        BodyAnimationSnapshot snapshot{};
        if (body == nullptr)
            return snapshot;

        const Shared<ModelCom> model = body->Get_Model();
        if (model == nullptr)
            return snapshot;

        snapshot.valid = true;
        snapshot.stateIndex = body->Get_AnimationStateIndex();
        snapshot.timeSec = model->Get_CurrentAnimationTimeSec();
        snapshot.frameSpeed = body->Get_AnimationSpeed();
        snapshot.isLoop = model->Is_AnimationLooping();

        return snapshot;
    }

    void Restore_BodyAnimationSnapshot(Body_Player* body, const BodyAnimationSnapshot& snapshot)
    {
        if (body == nullptr || !snapshot.valid || snapshot.stateIndex == UINT32_MAX)
            return;

        if (body->Set_AnimationByStateTimeSec(snapshot.stateIndex, 0.f, snapshot.isLoop, snapshot.timeSec))
            body->Set_AnimationSpeed(snapshot.frameSpeed);
    }
}

Player_WorldBase::Player_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_WorldBase{ device, context }
{
}

Player_WorldBase::Player_WorldBase(const Player_WorldBase& prototype)
    : Character_WorldBase{ prototype }
{
}

HRESULT Player_WorldBase::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Player_WorldBase::Initialize(void* arg)
{
    Character_WorldBase::ContainerObjectDesc desc{};
    desc.speedPerSec = 20.f;
    desc.degreePerSec = 180.f;

    CHECK_FAILED(Character_WorldBase::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void Player_WorldBase::BeginPlay()
{
    Character_WorldBase::BeginPlay();
    _fallStateEnterAcc = 0.f;
    _airborneFallTimeAcc = 0.f;

    if (auto* body = dynamic_cast<Body_Player*>(Get_PartObject(L"Part_Body")))
    {
        if (Shared<ModelCom> model = body->Get_Model())
        {
            model->Refresh_AnimStateMappingFromOwnerContext();
        }
    }

    if (_playerStateMachineCom)
        _playerStateMachineCom->BeginPlay_RegisteredStates();

    _playerStateMachineCom->Change_State(PlayerState::Idle);
}

void Player_WorldBase::Priority_Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
        return;

    if (!_partyVisibilityAllowed)
        return;

    __super::Priority_Update(timeDelta);

    if(_isCinematicDriven)
        return;

    // follower도 StateMachine/Controller를 타야 Idle/Run/Sprint 전환과 PhysX 이동이 처리된다.
    _playerControllerCom->Update_InputState();

    const bool stateOwnedTransformBeforeUpdate =
        PlayerWorldTuning::Is_TransformOwnedState(_playerStateMachineCom->Get_CurrentPlayerState());

    _playerStateMachineCom->Update_CurrentState(timeDelta);

    const PlayerState currentStateAfterUpdate = _playerStateMachineCom->Get_CurrentPlayerState();
    const bool stateOwnsTransform = PlayerWorldTuning::Is_TransformOwnedState(currentStateAfterUpdate);

    if (!stateOwnedTransformBeforeUpdate && !stateOwnsTransform)
        _playerControllerCom->Update_Movement(timeDelta);

    const bool isActiveHero = CLIENT->Is_ActiveHero(GetSharedPtr<GameObject>());

    if (isActiveHero && _movementCom && _movementCom->Did_InputJumpStart() && !stateOwnsTransform)
    {
        _fallStateEnterAcc = 0.f; // 실제 점프 입력은 기존처럼 즉시 Jump 상태로 보낸다.
        _airborneFallTimeAcc = 0.f;
        if (const Shared<Player_Jump> jumpState =
            dynamic_pointer_cast<Player_Jump>(_playerStateMachineCom->Get_State(PlayerState::Jump)))
        {
            jumpState->Set_InitialFallTime(0.f);
        }

        if (!Is_ParkourBlockedByCurrentLevel() &&
            PlayerWorldTuning::Try_EnterParkourFromJumpIntent(_playerStateMachineCom))
        {
            _playerStateMachineCom->Update_CurrentState(0.f);
        }
        else if (_playerStateMachineCom->Change_State(PlayerState::Jump))
        {
            _playerStateMachineCom->Update_CurrentState(0.f);
        }
    }
    else if (_movementCom)
    {
        const PlayerState currentState = _playerStateMachineCom->Get_CurrentPlayerState();
        const bool canEnterFallState =
            currentState == PlayerState::Idle ||
            currentState == PlayerState::Move;

        if (_movementCom->Is_Grounded())
        {
            _fallStateEnterAcc = 0.f; // 지면을 다시 밟으면 점프 입력 없는 낙하 진입 누적은 바로 리셋한다.
            _airborneFallTimeAcc = 0.f;
        }
        else if (canEnterFallState)
        {
            _airborneFallTimeAcc += timeDelta;

            if (_movementCom->Get_Velocity().y < PlayerWorldTuning::kFallStateEnterSpeedY)
            {
                _fallStateEnterAcc += timeDelta; // 경사 보행 중 잠깐 비는 공중 프레임은 바로 Jump로 소비하지 않고 짧게 누적 확인한다.

                if (_fallStateEnterAcc >= PlayerWorldTuning::kFallStateEnterDelaySec)
                {
                    const Shared<Player_Jump> jumpState =
                        dynamic_pointer_cast<Player_Jump>(_playerStateMachineCom->Get_State(PlayerState::Jump));
                    if (jumpState != nullptr)
                    {
                        jumpState->Set_InitialFallTime(_airborneFallTimeAcc);
                    }

                    _fallStateEnterAcc = 0.f;
                    _airborneFallTimeAcc = 0.f;
                    if (!_playerStateMachineCom->Change_State(PlayerState::Jump) && jumpState != nullptr)
                        jumpState->Set_InitialFallTime(0.f);
                }
            }
            else
            {
                _fallStateEnterAcc = 0.f;
            }
        }
        else
        {
            _fallStateEnterAcc = 0.f; // Idle/Move 외 상태거나 하강 속도가 약하면 가짜 낙하 누적을 버린다.
            _airborneFallTimeAcc = 0.f;
        }
    }
    else
    {
        _fallStateEnterAcc = 0.f;
        _airborneFallTimeAcc = 0.f;
    }

    Handle_DebugWarpShortcuts();

    _playerStateMachineCom->LateUpdate_CurrentState(timeDelta);
}

void Player_WorldBase::Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
        return;

    if (!_partyVisibilityAllowed)
        return;

    Update_GameMenuFacialPreview(timeDelta, true);

    if (_isCinematicDriven)
    {
        if (_effectCom)
            _effectCom->Update(timeDelta);

        return;
    }

    __super::Update(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);

}

void Player_WorldBase::Late_Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
    {
        Set_Visible(false);
        return;
    }

    if (!_partyVisibilityAllowed)
    {
        Set_Visible(false); // 잠긴 파티원은 Character_WorldBase::Update_Culling이 다시 켜기 전에 차단한다.
        return;
    }

    __super::Late_Update(timeDelta);

    Update_Culling(Vec3(1.f, 2.f, 1.f));
}

void Player_WorldBase::Post_PhysX_Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
        return;

    if (!_partyVisibilityAllowed)
        return;

    __super::Post_PhysX_Update(timeDelta);
}

HRESULT Player_WorldBase::Render()
{
    return S_OK;
}

void Player_WorldBase::Set_CinematicDriven(bool cinematicDriven)
{
    if (_isCinematicDriven == cinematicDriven)
        return;

    _isCinematicDriven = cinematicDriven;

    if (_movementCom)
    {
        _movementCom->Set_CCTTransformSyncSuppressed(cinematicDriven);
        _movementCom->Clear_FollowMoveCommand();
        _movementCom->Apply_Input({});
        _movementCom->Stop_AllMovement();
        _movementCom->Stop_VerticalMovement();
        _movementCom->Sync_RuntimePositionToTransform();
    }

    if (!cinematicDriven && _playerStateMachineCom)
        _playerStateMachineCom->Reenter_CurrentState();
}

void Player_WorldBase::Set_PartyVisibilityAllowed(bool allowed)
{
    _partyVisibilityAllowed = allowed;

    if (!allowed)
        Set_Visible(false);
}

void Player_WorldBase::Update_StatusPreview(float timeDelta)
{
    Try_BeginPlay();

    ContainerObject::Update(timeDelta);
    ContainerObject::Late_Update(timeDelta);
}

bool Player_WorldBase::Is_ParkourBlockedByCurrentLevel() const
{
    return GAME->Current_LevelIndex() == ETOI(LevelType::Stage01);
}

void Player_WorldBase::Update_GameMenuFacialPreview(float timeDelta, bool enabled)
{
    UNREFERENCED_PARAMETER(timeDelta);

    auto* head = dynamic_cast<Head_Player*>(Get_PartObject(L"Part_Head"));
    if (head == nullptr)
    {
        if (!enabled && _gameMenuFacialModifier != nullptr)
            _gameMenuFacialModifier->Set_Active(false);
        return;
    }

    const Shared<ModelCom> headModel = head->Get_Model();
    if (headModel == nullptr)
    {
        if (!enabled && _gameMenuFacialModifier != nullptr)
            _gameMenuFacialModifier->Set_Active(false);
        return;
    }

    if (!enabled)
    {
        if (auto* currentModifier = headModel->Find_AnimModifierByTag(PlayerWorldTuning::kGameMenuFacialModifierTag))
            currentModifier->Set_Active(false);

        if (_gameMenuFacialEnabled && _gameMenuFacialModifier != nullptr)
        {
            _gameMenuFacialModifier->Set_Active(false);
        }

        _gameMenuFacialEnabled = false;
        return;
    }

    if (_gameMenuFacialClip == nullptr)
    {
        const wchar_t* clipRelPath = Get_GameMenuFacialClipPath();
        if (clipRelPath == nullptr)
            return;

        const fs::path clipPath = fs::path(GAME->Get_AssetRoot()) / clipRelPath;
        auto clip = FacialClip::Create();
        if (clip == nullptr || FAILED(clip->Load_Binary(String::ToString(clipPath.wstring()))))
        {
            LOG_WARN("[GameMenuFacialPreview] Failed to load facialclip: {}", String::ToString(clipPath.wstring()));
            return;
        }

        _gameMenuFacialClip = clip;
    }

    auto* currentModifier = headModel->Find_AnimModifierByTag(PlayerWorldTuning::kGameMenuFacialModifierTag);
    if (currentModifier == nullptr)
    {
        FacialAnimationModifier::FacialModifierDesc modifierDesc{};
        modifierDesc.modifierTag = PlayerWorldTuning::kGameMenuFacialModifierTag;
        modifierDesc.clip = _gameMenuFacialClip;
        modifierDesc.loop = true;
        modifierDesc.suppressMouthArea = true;

        _gameMenuFacialModifier = FacialAnimationModifier::Create(&modifierDesc);
        if (_gameMenuFacialModifier == nullptr)
            return;

        headModel->Add_AnimModifier(_gameMenuFacialModifier);
        currentModifier = _gameMenuFacialModifier.get();
    }

    if (_gameMenuFacialModifier != nullptr && _gameMenuFacialModifier.get() != currentModifier)
        _gameMenuFacialModifier->Set_Active(false);

    _gameMenuFacialEnabled = true;
    currentModifier->Set_Active(true);
}

const wchar_t* Player_WorldBase::Get_GameMenuFacialClipPath() const
{
    return L"Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu_Lune.facialclipbin";
}

void Player_WorldBase::Handle_DebugWarpShortcuts()
{
    if (!GAME->KeyPress(KEY_TYPE::SHIFT))
        return;

    const array<KEY_TYPE, 5> keys =
    {
        KEY_TYPE::KEY_1,
        KEY_TYPE::KEY_2,
        KEY_TYPE::KEY_3,
        KEY_TYPE::KEY_4,
        static_cast<KEY_TYPE>('5'),
    };

    for (size_t i = 0; i < keys.size(); ++i)
    {
        if (!GAME->KeyDown(keys[i]))
            continue;

        _transformCom->Set_WorldPosition(PlayerWorldTuning::kDebugWarpPoints[i]);

        if (_movementCom)
            _movementCom->Sync_RuntimePositionToTransform();

        _fallStateEnterAcc = 0.f;

        if (_playerStateMachineCom)
            _playerStateMachineCom->Change_State(PlayerState::Idle);

        return;
    }
}

void Player_WorldBase::Enter_GameMenuAnimation(uint32 introAnim, uint32 idleAnim)
{
    if (nullptr == _playerStateMachineCom) return;

    _playerStateMachineCom->Enter_GameMenuAnimation(introAnim, idleAnim);
}

Vec3 Player_WorldBase::Get_MovementVelocity() const
{
    if (_movementCom == nullptr)
        return Vec3::Zero;

    return _movementCom->Get_Velocity();
}

void Player_WorldBase::Start_SwitchDissolve(float durationSec, const string& noiseName)
{
    if (_effectCom)
        _effectCom->Start_SwitchDissolve(durationSec, noiseName);


}

void Player_WorldBase::Set_WorldInteractionCandidate(const Shared<GameObject>& spot)
{
    if (_worldInteractionContext.isInteracting || spot == nullptr)
        return;

    const Shared<GameObject> current = _worldInteractionContext.candidateSpot.lock();
    if (current != nullptr && current.get() != spot.get())
    {
        const auto* currentProvider = dynamic_cast<IWorldInteractionProvider*>(current.get());
        const auto* nextProvider = dynamic_cast<IWorldInteractionProvider*>(spot.get());
        const int32 currentPriority = currentProvider != nullptr ? currentProvider->Get_InteractionPriority() : 0;
        const int32 nextPriority = nextProvider != nullptr ? nextProvider->Get_InteractionPriority() : 0;

        if (nextPriority < currentPriority)
            return;
    }

    _worldInteractionContext.candidateSpot = spot;
}

void Player_WorldBase::Clear_WorldInteractionCandidate(const GameObject* spot)
{
    const Shared<GameObject> current = _worldInteractionContext.candidateSpot.lock();
    if (current == nullptr)
        return;

    if (spot != nullptr && current.get() != spot)
        return;

    _worldInteractionContext.candidateSpot.reset();
}

Shared<GameObject> Player_WorldBase::Get_WorldInteractionCandidate() const
{
    return _worldInteractionContext.candidateSpot.lock();
}

bool Player_WorldBase::Begin_WorldInteraction(const Shared<GameObject>& spot, const WorldInteractionSpot::WorldInteractionRequest& request)
{
    if (spot == nullptr)
        return false;

    _worldInteractionContext.activeSpot = spot;
    _worldInteractionContext.candidateSpot = spot;
    _worldInteractionContext.activeRequest = request;
    _worldInteractionContext.isInteracting = true;

    return true;
}

void Player_WorldBase::Clear_WorldInteractionRequest(const GameObject* spot)
{
    const Shared<GameObject> active = _worldInteractionContext.activeSpot.lock();
    if (spot != nullptr && active != nullptr && active.get() != spot)
        return;

    if (spot != nullptr && active == nullptr && !_worldInteractionContext.isInteracting)
        return;

    _worldInteractionContext.activeSpot.reset();
    _worldInteractionContext.activeRequest = {};
    _worldInteractionContext.isInteracting = false;
}

void Player_WorldBase::End_WorldInteraction()
{
    _worldInteractionContext.activeSpot.reset();
    _worldInteractionContext.activeRequest = {};
    _worldInteractionContext.isInteracting = false;
}

bool Player_WorldBase::Is_WorldInteracting() const
{
    return _worldInteractionContext.isInteracting;
}

const WorldInteractionSpot::WorldInteractionRequest& Player_WorldBase::Get_WorldInteractionRequest() const
{
    return _worldInteractionContext.activeRequest;
}

HRESULT Player_WorldBase::Apply_CostumePreset(const CostumePreset& preset)
{
    if (preset.parts.bodyModelTag.empty() ||
        preset.parts.headModelTag.empty())
        //|| preset.parts.hairModelTag.empty()) 대머리 때문에 일단 이거 제외해보기
    {
        return E_FAIL;
    }

    Capture_DefaultCostumePartTransformsIfNeeded();

    CHECK_FAILED(Ensure_CostumeVariantBuilt(preset), E_FAIL);
    CHECK_FAILED(Activate_CostumeVariant(preset), E_FAIL);

    return S_OK;
}

HRESULT Player_WorldBase::Prewarm_CostumeVariants()
{
    // Ready_PartObjects 이후에도 옷장/preview 준비 경로에서 명시적으로 다시 부를 수 있게 둔다.
    return Warmup_CostumeVariants();
}

HRESULT Player_WorldBase::Warmup_CostumeVariants()
{
    CostumeManager* costumeManager = CLIENT->Get_CostumeManager();
    if (costumeManager == nullptr)
        return S_OK;

    const vector<CostumePreset>* presets = costumeManager->Get_Costumes(Get_HeroId());
    if (presets == nullptr)
        return S_OK;

    for (const CostumePreset& preset : *presets)
    {
        if (FAILED(Ensure_CostumeVariantBuilt(preset)))
        {
            LOG_WARN(
                "Player_WorldBase: skip costume prewarm. hero={}, preset={}",
                static_cast<int32>(Get_HeroId()),
                preset.presetId);
            continue; // 선택 전 선로딩 실패는 world hero 초기화까지 막지 않는다.
        }

        const auto iter = _costumeVariantsByPreset.find(preset.presetId);
        if (iter != _costumeVariantsByPreset.end())
        {
            if (FAILED(Warmup_CostumeVariantParts(iter->second)))
            {
                LOG_WARN(
                    "Player_WorldBase: skip costume part warmup. hero={}, preset={}",
                    static_cast<int32>(Get_HeroId()),
                    preset.presetId);
            }
        }
    }

    return S_OK;
}

HRESULT Player_WorldBase::Ensure_CostumeVariantBuilt(const CostumePreset& preset)
{
    if (_costumeVariantsByPreset.contains(preset.presetId))
        return S_OK;

    CostumeVariantParts variant{};
    variant.body = Create_CostumeBodyVariant(preset.parts.bodyModelTag);
    CHECK_NULL(variant.body, E_FAIL);
    CHECK_FAILED(Ready_BodyModifiers(variant.body.get(), preset.presetId), E_FAIL);

    variant.head = Create_CostumeHeadVariant(preset.parts.headModelTag, variant.body.get());
    CHECK_NULL(variant.head, E_FAIL);

    variant.beard = Create_CostumeFaceAccessoryVariant(preset.parts.beardModelTag, variant.head.get());
    variant.mustache = Create_CostumeFaceAccessoryVariant(preset.parts.mustacheModelTag, variant.head.get());

    variant.hair = Create_CostumeHairVariant(preset.parts.hairModelTag, variant.head.get());
    if (!preset.parts.hairModelTag.empty())
        CHECK_NULL(variant.hair, E_FAIL);

    variant.glasses = Create_CostumeFaceAccessoryVariant(preset.parts.glassesModelTag, variant.head.get());
    variant.baseTransforms = Capture_CostumeVariantPartTransforms(variant); // 각 모델이 clone 직후 가진 자기 로컬 기준을 저장
    variant.presetTransforms = Build_CostumePresetTransforms(preset, variant.baseTransforms); // preset override는 그 모델 고유 기준 위에만 덮는다.

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

    _costumeVariantsByPreset.emplace(preset.presetId, std::move(variant));
    return S_OK;
}

HRESULT Player_WorldBase::Activate_CostumeVariant(const CostumePreset& preset)
{
    const auto iter = _costumeVariantsByPreset.find(preset.presetId);
    if (iter == _costumeVariantsByPreset.end())
        return E_FAIL;

    const PlayerWorldTuning::BodyAnimationSnapshot animationSnapshot =
        PlayerWorldTuning::Capture_BodyAnimationSnapshot(dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body"))));

    for (auto& [cachedPresetId, parts] : _costumeVariantsByPreset)
    {
        const bool isTarget = (cachedPresetId == preset.presetId);

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

    Apply_CostumePartTransforms(iter->second.presetTransforms); // preset별 최종 오프셋을 매번 다시 얹어야 preview 재오픈 후에도 안 깨진다.
    Reapply_CostumeBaseMaterials();

    if (auto* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body"))))
    {
        if (Shared<ModelCom> model = body->Get_Model())
            model->Refresh_AnimStateMappingFromOwnerContext();

        PlayerWorldTuning::Restore_BodyAnimationSnapshot(body, animationSnapshot);
    }

    if (_playerStateMachineCom)
    {
        _playerStateMachineCom->Refresh_BodyPlayerForStates();
        if (!GAME->Is_GameplayPaused())
            _playerStateMachineCom->Reenter_CurrentState();
    }

    return S_OK;
}

void Player_WorldBase::Rebind_ActiveCostumeVariant(const string& presetId)
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
}

HRESULT Player_WorldBase::Warmup_CostumeVariantParts(CostumeVariantParts& parts)
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

void Player_WorldBase::Capture_DefaultCostumePartTransformsIfNeeded()
{
    if (_defaultCostumePartTransforms.empty())
        _defaultCostumePartTransforms = Capture_CostumePartTransforms();
}

void Player_WorldBase::Clear_AllCostumeVariants()
{
    for (auto& [presetId, parts] : _costumeVariantsByPreset)
    {
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

Player_WorldBase::CostumePartTransformMap Player_WorldBase::Capture_CostumeVariantPartTransforms(const CostumeVariantParts& parts) const
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

Player_WorldBase::CostumePartTransformMap Player_WorldBase::Build_CostumePresetTransforms(
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

void Player_WorldBase::Apply_CostumePartTransforms(const CostumePartTransformMap& partTransforms)
{
    Apply_CostumePartTransform(TEXT("Part_Body"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Head"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Beard"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Mustache"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Hair"), partTransforms);
    Apply_CostumePartTransform(TEXT("Part_Glasses"), partTransforms);
}

HRESULT Player_WorldBase::Rebuild_CostumeParts(const CostumePreset& preset)
{
    if (_defaultCostumePartTransforms.empty())
        _defaultCostumePartTransforms = Capture_CostumePartTransforms();

    const CostumePartTransformMap currentPartTransforms = Capture_CostumePartTransforms();
    const CostumePartTransformMap& restorePartTransforms =
        preset.presetId == "Default" ? Get_DefaultCostumePartTransforms() : currentPartTransforms;

    Clear_CostumeParts();

    Body_Player* body = Add_CostumeBody(preset.parts.bodyModelTag);
    CHECK_NULL(body, E_FAIL);
    Apply_CostumePartTransform(TEXT("Part_Body"), restorePartTransforms, preset.parts.bodyTransform);

    if (_playerStateMachineCom)
    {
        _playerStateMachineCom->Refresh_BodyPlayerForStates();
    }

    CHECK_FAILED(Ready_BodyModifiers(body, preset.presetId), E_FAIL);

    Head_Player* head = Add_CostumeHead(preset.parts.headModelTag, body);
    CHECK_NULL(head, E_FAIL);
    Apply_CostumePartTransform(TEXT("Part_Head"), restorePartTransforms, preset.parts.headTransform);

    CHECK_FAILED(Add_CostumeFaceAccessory(TEXT("Part_Beard"), preset.parts.beardModelTag, head), E_FAIL);
    Apply_CostumePartTransform(TEXT("Part_Beard"), restorePartTransforms, preset.parts.beardTransform);

    CHECK_FAILED(Add_CostumeFaceAccessory(TEXT("Part_Mustache"), preset.parts.mustacheModelTag, head), E_FAIL);
    Apply_CostumePartTransform(TEXT("Part_Mustache"), restorePartTransforms, preset.parts.mustacheTransform);

    CHECK_FAILED(Add_CostumeHair(preset.parts.hairModelTag, head), E_FAIL);
    Apply_CostumePartTransform(TEXT("Part_Hair"), restorePartTransforms, preset.parts.hairTransform);

    CHECK_FAILED(Add_CostumeFaceAccessory(TEXT("Part_Glasses"), preset.parts.glassesModelTag, head), E_FAIL);
    Apply_CostumePartTransform(TEXT("Part_Glasses"), restorePartTransforms, preset.parts.glassesTransform);

    Reapply_CostumeBaseMaterials();

    if (_playerStateMachineCom)
        _playerStateMachineCom->Reenter_CurrentState();

    return S_OK;
}

void Player_WorldBase::Clear_CostumeParts()
{
    Remove_PartObject(TEXT("Part_Glasses"));
    Remove_PartObject(TEXT("Part_Hair"));
    Remove_PartObject(TEXT("Part_Mustache"));
    Remove_PartObject(TEXT("Part_Beard"));
    Remove_PartObject(TEXT("Part_Head"));
    Remove_PartObject(TEXT("Part_Body"));
}

Player_WorldBase::CostumePartTransformMap Player_WorldBase::Capture_CostumePartTransforms() const
{
    CostumePartTransformMap partTransforms{};

    for (const auto& [partTag, partObject] : _partObjects)
    {
        if (partObject == nullptr || partObject->Get_Transform() == nullptr)
            continue;

        CostumePartTransform transform{};
        transform.position = partObject->Get_Transform()->Get_Position();
        transform.rotation = partObject->Get_Transform()->Get_RotationEuler();
        transform.scale = partObject->Get_Transform()->Get_Scale();

        partTransforms.emplace(partTag, transform);
    }

    return partTransforms;
}

const Player_WorldBase::CostumePartTransformMap& Player_WorldBase::Get_DefaultCostumePartTransforms()
{
    if (_defaultCostumePartTransforms.empty())
        _defaultCostumePartTransforms = Capture_CostumePartTransforms();

    return _defaultCostumePartTransforms;
}

void Player_WorldBase::Apply_CostumePartTransform(const wstring& partTag,
    const CostumePartTransformMap& partTransforms)
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

void Player_WorldBase::Apply_CostumePartTransform(const wstring& partTag,
    const CostumePartTransformMap& partTransforms, const CostumePartLocalTransform& fallbackTransform)
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

    const auto iter = partTransforms.find(partTag);
    if (iter != partTransforms.end())
    {
        Apply_CostumePartTransform(partTag, partTransforms);
        return;
    }
}

void Player_WorldBase::Reapply_CostumePartBaseMaterial(const wstring& partTag)
{
    PartObject* partObject = Get_PartObject(partTag);
    if (partObject == nullptr)
        return;

    Shared<ModelCom> model{};

    if (auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        model = playablePart->Get_Model();
    }
    else if (auto* faceAccessory = dynamic_cast<FaceAccessory_Player*>(partObject))
    {
        model = faceAccessory->Get_Model();
    }

    if (model == nullptr)
        return;

    // 코스튬 전환은 새 모델만 갈아끼우는 경로라 base bundle 재적용을 명시적으로 다시 돌려준다.
    model->Apply_BaseMaterialFromBundle_AllSlots();
    (void)model->Normalize_OwnerComponentTagAndBindings();
}

void Player_WorldBase::Reapply_CostumeBaseMaterials()
{
    Reapply_CostumePartBaseMaterial(TEXT("Part_Body"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Head"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Beard"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Mustache"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Hair"));
    Reapply_CostumePartBaseMaterial(TEXT("Part_Glasses"));
}

Shared<Body_Player> Player_WorldBase::Create_CostumeBodyVariant(const wstring& modelTag)
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
        model->Refresh_AnimStateMappingFromOwnerContext();
        model->Apply_BaseMaterialFromBundle_AllSlots();
        (void)model->Normalize_OwnerComponentTagAndBindings();
    }

    return body;
}

Shared<Head_Player> Player_WorldBase::Create_CostumeHeadVariant(const wstring& modelTag, const Body_Player* body)
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

Shared<Hair_Player> Player_WorldBase::Create_CostumeHairVariant(const wstring& modelTag, const Head_Player* head)
{
    if (modelTag.empty())
        return nullptr;

    CHECK_NULL(head, nullptr);

    Hair_Player::HAIR_PLAYER_DESC hairDesc{};
    hairDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    hairDesc.modelType = ModelType::Anim;
    hairDesc.modelTag = modelTag;
    hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairDesc.useHairModifier = Get_HeroId() == HeroId::Maelle;
    hairDesc.useLuneHairPhysicsModifier = Get_HeroId() == HeroId::Lune;
    hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    Shared<PartObject> clonedPart = static_pointer_cast<PartObject>(
        GAME->Clone_Prototype(Prototype::GameObject, ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), &hairDesc));
    Shared<Hair_Player> hair = dynamic_pointer_cast<Hair_Player>(clonedPart);
    if (hair == nullptr)
        return nullptr;

    hair->Set_Owner(GetSharedPtr<GameObject>());
    return hair;
}

Shared<FaceAccessory_Player> Player_WorldBase::Create_CostumeFaceAccessoryVariant(const wstring& modelTag,
    const Head_Player* head)
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

Body_Player* Player_WorldBase::Add_CostumeBody(const wstring& modelTag)
{
    if (modelTag.empty())
        return nullptr;

    Body_Player::BODY_PLAYER_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = modelTag;

    CHECK_FAILED(__super::Add_PartObject(
        ETOI(LevelType::Static), TEXT("GameObject_Body_Player"), TEXT("Part_Body"), &bodyDesc), nullptr);

    Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body")));
    if (body == nullptr)
        return nullptr;

    if (Shared<ModelCom> model = body->Get_Model())
        model->Refresh_AnimStateMappingFromOwnerContext();

    return body;
}

Head_Player* Player_WorldBase::Add_CostumeHead(const wstring& modelTag, const Body_Player* body)
{
    if (modelTag.empty() || body == nullptr)
        return nullptr;

    Head_Player::HEAD_PLAYER_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = modelTag;
    headDesc.socketMatrixPointer = nullptr;

    CHECK_FAILED(__super::Add_PartObject(
        ETOI(LevelType::Static), TEXT("GameObject_Head_Player"), TEXT("Part_Head"), &headDesc), nullptr);

    Head_Player* head = dynamic_cast<Head_Player*>(Get_PartObject(TEXT("Part_Head")));
    if (head == nullptr)
        return nullptr;

    head->Set_LeaderPoseComponent(body);
    return head;
}

HRESULT Player_WorldBase::Add_CostumeHair(const wstring& modelTag, const Head_Player* head)
{
    if (modelTag.empty())
        return S_OK;

    CHECK_NULL(head, E_FAIL);

    Hair_Player::HAIR_PLAYER_DESC hairDesc{};
    hairDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    hairDesc.modelType = ModelType::Anim;
    hairDesc.modelTag = modelTag;
    hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairDesc.useHairModifier = Get_HeroId() == HeroId::Maelle;
    hairDesc.useLuneHairPhysicsModifier = Get_HeroId() == HeroId::Lune;
    hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(__super::Add_PartObject(
        ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), TEXT("Part_Hair"), &hairDesc), E_FAIL);

    return S_OK;
}

HRESULT Player_WorldBase::Add_CostumeFaceAccessory(const wstring& partTag, const wstring& modelTag,
    const Head_Player* head)
{
    if (modelTag.empty())
        return S_OK;

    CHECK_NULL(head, E_FAIL);

    FaceAccessory_Player::FACE_ACCESSORY_DESC accessoryDesc{};
    accessoryDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    accessoryDesc.modelType = ModelType::NonAnim;
    accessoryDesc.modelTag = modelTag.c_str();
    accessoryDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    if (FAILED(__super::Add_PartObject(
        ETOI(LevelType::Static), TEXT("GameObject_FaceAccessory_Player"), partTag, &accessoryDesc)))
    {
        LOG_WARN("[Player_WorldBase] failed to add costume face accessory: part={}, model={}",
            String::ToString(partTag), String::ToString(modelTag));
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Player_WorldBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Input"), _inputCom), E_FAIL);
       
    MovementCom::MoveDesc moveDesc{};
    moveDesc.walkSpeed = 3.f;
    moveDesc.runSpeed = 6.5f;
    moveDesc.sprintSpeed = 10.f;
    moveDesc.turnSpeedDegree = 720.f;
    moveDesc.turnResponse = 10.f;
    moveDesc.jumpVelocity = 20.f;
    moveDesc.gravity = -40.f;

    moveDesc.backend = MovementCom::MoveBackend::PhysXCCT; // 플레이어는 CCT로 지형타기

    moveDesc.sweep.skinWidth = 0.03f;
    moveDesc.sweep.groundSnapUp = 0.35f;
    moveDesc.sweep.groundSnapDown = 0.90f;
    moveDesc.sweep.walkableNormalY = 0.65f;
    moveDesc.sweep.groundY = 0.f;

    moveDesc.cct.radius = 0.32f;
    moveDesc.cct.height = 1.50f;
    moveDesc.cct.stepOffset = 0.45f;
    moveDesc.cct.contactOffset = 0.05f;
    moveDesc.cct.slopeLimitDeg = 55.f;
    moveDesc.cct.groundLerpSpeed = 15.f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Movement"), _movementCom, &moveDesc), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerController"), _playerControllerCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStateMachine"), _playerStateMachineCom), E_FAIL);
    
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.75f, 0.f);
    obbDesc.extents = Vec3(0.35f, 0.75f, 0.35f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::Player_WorldBody), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_0_PlayerWorldBodyCollider")), E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_SkillTree"), _skillTreeCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStat"), _combatStatCom), E_FAIL);

	CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    return S_OK;
}

HRESULT Player_WorldBase::Ready_PartObjects()
{
    Body_Player::BODY_PLAYER_DESC playableDesc{};
    playableDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    playableDesc.modelType = ModelType::Anim;
    playableDesc.modelTag = Get_BodyModelTag();;

    //> MainBody Setting
    if (FAILED(
        __super::Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Body_Player"),
            TEXT("Part_Body"), &playableDesc)
    ))
        return E_FAIL;

    const Body_Player* mainBody = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
    if (nullptr == mainBody)
        return E_FAIL;

    //> MainBody에 붙을 파츠 세팅
    //>> Head Part
    Head_Player::HEAD_PLAYER_DESC headPartDesc{};
    headPartDesc.parentMatrix = playableDesc.parentMatrix;
    headPartDesc.modelType = ModelType::Anim;
    headPartDesc.modelTag = Get_HeadModelTag();
    headPartDesc.socketMatrixPointer = nullptr;

    if (FAILED(
        __super::Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Head_Player"),
            TEXT("Part_Head"), &headPartDesc)
    ))
        return E_FAIL;

    Head_Player* head = dynamic_cast<Head_Player*>(_partObjects[TEXT("Part_Head")].get());
    if (nullptr == head)
        return E_FAIL;

    // 머리붙이기
    head->Set_LeaderPoseComponent(mainBody);

    // 콧수염
    if (const wchar_t* beardModelTag = Get_BeardModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC beardPartDesc{};
        beardPartDesc.parentMatrix = playableDesc.parentMatrix;
        beardPartDesc.modelType = ModelType::NonAnim;
        beardPartDesc.modelTag = beardModelTag;
        beardPartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

        if (FAILED(
            __super::Add_PartObject(
                ETOI(LevelType::Static),
                TEXT("GameObject_FaceAccessory_Player"),
                TEXT("Part_Beard"),
                &beardPartDesc)))
            return E_FAIL;
    }

    // 턱수염
    if (const wchar_t* mustacheModelTag = Get_MustacheModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC mustachePartDesc{};
        mustachePartDesc.parentMatrix = playableDesc.parentMatrix;
        mustachePartDesc.modelType = ModelType::NonAnim;
        mustachePartDesc.modelTag = mustacheModelTag;
        mustachePartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

        if (FAILED(
            __super::Add_PartObject(
                ETOI(LevelType::Static),
                TEXT("GameObject_FaceAccessory_Player"),
                TEXT("Part_Mustache"),
                &mustachePartDesc)))
            return E_FAIL;
    }


    //>> Hair Part
    Hair_Player::HAIR_PLAYER_DESC hairPartDesc{};
    hairPartDesc.parentMatrix = playableDesc.parentMatrix;
    hairPartDesc.modelType = ModelType::Anim;
    hairPartDesc.modelTag = Get_HairModelTag();
    hairPartDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairPartDesc.useHairModifier = Get_HeroId() == HeroId::Maelle;
    hairPartDesc.useLuneHairPhysicsModifier = Get_HeroId() == HeroId::Lune;
    hairPartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    if (FAILED(
        __super::Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"),
            TEXT("Part_Hair"), &hairPartDesc)
    ))
        return E_FAIL;

    Capture_DefaultCostumePartTransformsIfNeeded();

    CostumeVariantParts defaultParts{};
    defaultParts.body = dynamic_pointer_cast<Body_Player>(_partObjects[TEXT("Part_Body")]);
    defaultParts.head = dynamic_pointer_cast<Head_Player>(_partObjects[TEXT("Part_Head")]);
    if (const auto beardIter = _partObjects.find(TEXT("Part_Beard")); beardIter != _partObjects.end())
        defaultParts.beard = dynamic_pointer_cast<FaceAccessory_Player>(beardIter->second);
    if (const auto mustacheIter = _partObjects.find(TEXT("Part_Mustache")); mustacheIter != _partObjects.end())
        defaultParts.mustache = dynamic_pointer_cast<FaceAccessory_Player>(mustacheIter->second);
    defaultParts.hair = dynamic_pointer_cast<Hair_Player>(_partObjects[TEXT("Part_Hair")]);
    if (const auto glassesIter = _partObjects.find(TEXT("Part_Glasses")); glassesIter != _partObjects.end())
        defaultParts.glasses = dynamic_pointer_cast<FaceAccessory_Player>(glassesIter->second);
    defaultParts.baseTransforms = _defaultCostumePartTransforms;
    defaultParts.presetTransforms = _defaultCostumePartTransforms;

    CHECK_FAILED(Ready_BodyModifiers(defaultParts.body.get(), "Default"), E_FAIL);

    if (CostumeManager* costumeManager = CLIENT->Get_CostumeManager())
    {
        if (const CostumePreset* defaultPreset = costumeManager->Get_Costume(Get_HeroId(), "Default"))
            defaultParts.presetTransforms = Build_CostumePresetTransforms(*defaultPreset, defaultParts.baseTransforms);
    }

    _costumeVariantsByPreset["Default"] = defaultParts;
    _activeCostumePresetId = "Default";

    if (CostumeManager* costumeManager = CLIENT->Get_CostumeManager())
    {
        if (const CostumePreset* currentPreset = costumeManager->Get_CurrentCostume(Get_HeroId()))
            CHECK_FAILED(Apply_CostumePreset(*currentPreset), E_FAIL);
    }

    CHECK_FAILED(Warmup_CostumeVariants(), E_FAIL);

    return S_OK;
}

Shared<Player_WorldBase> Player_WorldBase::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Player_WorldBase>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Player_WorldBase");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> Player_WorldBase::Clone(void* arg)
{
    auto instance = make_shared<Player_WorldBase>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Player_WorldBase");
        return nullptr;
    }
    return instance;
}

void Player_WorldBase::Free()
{
    Clear_AllCostumeVariants();
    __super::Free();
}
