#include "SoulLikeController.h"
#include "BattleController.h"
#include "BattleTurnCamera.h"
#include "Client_Constants.h"
#include "Event_Bus.h"
#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "GameplayUI.h"
#include "GamePlayUI_SL.h"
#include "GradientSkill.h"
#include "GameInstance.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "ScreenFx_Manager.h"
#include "SL_Base.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SL_DamageSystem.h"
#include "SL_StateMachineCom.h"
#include "SL_TargetCom.h"
#include "Spawn_Prefab.h"
#include "SoulLikeCamera.h"
#include "SkillTreeCom.h"
#include "TransformCom.h"
#include "MonsterStatCom.h"
#include "SL_Skill.h"
#include "SL_EstherManager.h"
#include "Unlock_Manager.h"

#include <chrono>

namespace
{
    constexpr float kDuallisteBGMFadeOutDuration = 1.f;
    constexpr float kDuallisteCinematicMonsterBackOffset = 2.f;
    constexpr wchar_t kHealTintEffect[] = L"Effects\\Assets\\Item\\HealTint_G.effect.json";
    constexpr char kPlayerJumpCameraPresetKey[] = "PlayerJump";

    constexpr char kNpcGolgraDialogueKey[] = "NPC_Golgra";
    constexpr char kNpcGolgraAfterDuallisteDialogueKey[] = "NPC_Golgra_AfterDualliste";

    long long ElapsedMs(chrono::steady_clock::time_point startedAt)
    {
        return chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - startedAt).count();
    }

    Vec3 MakePlanarForwardFromRotationDegree(const Vec3& rotationDegree)
    {
        const Vec3 rotationRad = Vec3(
            XMConvertToRadians(rotationDegree.x),
            XMConvertToRadians(rotationDegree.y),
            XMConvertToRadians(rotationDegree.z));
        const Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(
            rotationRad.y,
            rotationRad.x,
            rotationRad.z);

        Vec3 forward = Vec3::TransformNormal(Vec3::Look, rotationMatrix);
        forward.y = 0.f;

        if (forward.LengthSquared() <= 0.0001f)
            forward = Vec3::Look;
        else
            forward.Normalize();

        return forward;
    }

    Vec3 MakePlanarForwardToMonster(const Vec3& playerPosition, const Vec3& playerRotation, const Vec3& monsterPosition)
    {
        Vec3 forward = monsterPosition - playerPosition;
        forward.y = 0.f;

        if (forward.LengthSquared() <= 0.0001f)
            return MakePlanarForwardFromRotationDegree(playerRotation);

        forward.Normalize();
        return forward;
    }

    Vec3 ApplyDuallisteCinematicMonsterBackOffset(const Vec3& playerPosition, const Vec3& playerRotation, const Vec3& monsterPosition)
    {
        const Vec3 forward = MakePlanarForwardToMonster(playerPosition, playerRotation, monsterPosition);
        return monsterPosition + forward * kDuallisteCinematicMonsterBackOffset;
    }
}

IMPLEMENT_REFLECTION(SoulLikeController)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "SoulLike";
    info.displayName = "SoulLike Controller";

    PROPERTY_STRING("플레이어 프리팹", _playerPrefab);
    PROPERTY_VEC3("플레이어 스폰 위치", _playerSpawnPosition, 0.1f);
    PROPERTY_VEC3("플레이어 스폰 회전", _playerSpawnRotation, 1.f);
    PROPERTY_STRING("몬스터 프리팹", _monsterPrefab);
    PROPERTY_FLOAT("몬스터 스폰 거리", _monsterSpawnDistance, 0.f, 20.f, 0.1f);

    PROPERTY_FLOAT("진입 연출 지연 시간", _enterFxDelay, 0.f, 5.f, 0.05f);
    PROPERTY_BOOL("디버그 3번키 시작 사용", _enableDebugKeyStart);

    // 전투 슬로우
    PROPERTY_FLOAT("승리 슬로우 지속 시간", _victorySlowDuration, 0.f, 5.f, 0.05f);
    PROPERTY_FLOAT("승리 슬로우 시간 배율", _victorySlowTimeScale, 0.f, 1.f, 0.01f);

    // 패링 밀려나는거
    PROPERTY_FLOAT("플레이어 피격 밀림 거리", _playerHitPushDistance, 0.f, 5.f, 0.05f);
    PROPERTY_FLOAT("플레이어 피격 밀림 시간", _playerHitPushDuration, 0.f, 1.f, 0.01f);

    PROPERTY_FLOAT("피격 히트스탑 시간 배율", _playerHitStopTimeScale, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("피격 히트스탑 지속 시간", _playerHitStopDuration, 0.f, 1.f, 0.01f);

    PROPERTY_FLOAT("공격 히트스탑 시간 배율", _playerAttackHitStopTimeScale, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("공격 히트스탑 지속 시간", _playerAttackHitStopDuration, 0.f, 1.f, 0.01f);

    // 전투 승리 후 이동할 위치
    PROPERTY_VEC3("승리 이동 위치", _victoryPosition);
    PROPERTY_VEC3("승리 이동 회전", _victoryRotation);

    return true;
}

SoulLikeController::SoulLikeController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

SoulLikeController::SoulLikeController(const SoulLikeController& prototype)
    : GameObject(prototype)
    , _playerPrefab(prototype._playerPrefab)
    , _playerSpawnPosition(prototype._playerSpawnPosition)
    , _playerSpawnRotation(prototype._playerSpawnRotation)
    , _monsterPrefab(prototype._monsterPrefab)
    , _monsterSpawnDistance(prototype._monsterSpawnDistance)
    , _enterFxDelay(prototype._enterFxDelay)
    , _enableDebugKeyStart(prototype._enableDebugKeyStart)
    , _playerHitPushDistance(prototype._playerHitPushDistance)
    , _playerHitPushDuration(prototype._playerHitPushDuration)
    , _playerHitStopTimeScale(prototype._playerHitStopTimeScale)
    , _playerHitStopDuration(prototype._playerHitStopDuration)
    , _playerAttackHitStopTimeScale(prototype._playerAttackHitStopTimeScale)
    , _playerAttackHitStopDuration(prototype._playerAttackHitStopDuration)
    , _jumpCounterDamageMultiplier(prototype._jumpCounterDamageMultiplier)
{
}

HRESULT SoulLikeController::Initialize_Prototype()
{
    return GameObject::Initialize_Prototype();
}

HRESULT SoulLikeController::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);
    return S_OK;
}

void SoulLikeController::BeginPlay()
{
    GameObject::BeginPlay();

    if (!_editorStopHandle.IsValid())
    {
        _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(
            this,
            &SoulLikeController::Handle_EditorStopRuntime);
    }

    if (_estherManager == nullptr)
        _estherManager = SL_EstherManager::Create(GetSharedPtr<SoulLikeController>());
}

void SoulLikeController::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (_state == BattleState::EnterFx)
    {
        Update_EnterFx(timeDelta);
        return;
    }

    if (_state == BattleState::VictorySlow)
    {
        Update_VictorySlow();
        return;
    }

    if (_state == BattleState::Playing)
    {
        if (_estherManager)
            _estherManager->Update(timeDelta);

        Update_MonsterActionDelay(timeDelta);
        Update_Player(timeDelta);
        Check_BattleEnd();
    }
}

bool SoulLikeController::Begin_Battle()
{
    const auto totalStartedAt = chrono::steady_clock::now();

    if (Is_BattleActive())
        return false;

    const auto cancelStartedAt = chrono::steady_clock::now();
    if (Shared<BattleController> battleController = CLIENT->Get_BattleController())
        battleController->Cancel_BattleEnterPending();
    const long long cancelMs = ElapsedMs(cancelStartedAt);

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);

    const auto removeStartedAt = chrono::steady_clock::now();
    Remove_SpawnedBattleActors();
    const long long removeMs = ElapsedMs(removeStartedAt);

    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
    _jumpCounterDamageApplied = false;
    _playerSpawned = false;
    _enterFxTimer = max(0.f, _enterFxDelay);
    _victorySlowElapsed = 0.f;
    _lastVictoryRewardExp = 0;
    _lastVictoryRewardPictosIds.clear();
    _healingPotionCount = kMaxHealingPotionCount;
    _state = BattleState::EnterFx;

    const auto gateStartedAt = chrono::steady_clock::now();
    Apply_SoulLikeArenaMapRenderGateBasis();
    const long long gateMs = ElapsedMs(gateStartedAt);

    const auto iblStartedAt = chrono::steady_clock::now();
    GAME->Begin_SoulLikeIBLOverride();
    const long long iblMs = ElapsedMs(iblStartedAt);

    const auto bgmStartedAt = chrono::steady_clock::now();
    if (Is_DuallisteBattle())
        Start_DuallistePhase1BGM();
    const long long bgmMs = ElapsedMs(bgmStartedAt);

    if (_estherManager)
        _estherManager->Reset();

    long long screenFxMs = 0;
    if (auto screenFxManager = CLIENT->Get_ScreenFxManager())
    {
        const auto screenFxStartedAt = chrono::steady_clock::now();
        screenFxManager->Play_BattleInCounterFx();
        screenFxManager->Transition_To_BattleField();
        screenFxMs = ElapsedMs(screenFxStartedAt);
    }

    LOG_INFO(
        "[SoulLikeEnterPerf] Begin_Battle dualliste={}, cancel={}ms, remove={}ms, gate={}ms, ibl={}ms, bgm={}ms, screenFx={}ms, enterDelay={}, total={}ms",
        Is_DuallisteBattle(),
        cancelMs,
        removeMs,
        gateMs,
        iblMs,
        bgmMs,
        screenFxMs,
        _enterFxTimer,
        ElapsedMs(totalStartedAt));

    return true;
}

void SoulLikeController::Set_BattleSpawnPoseOverride(
    const Vec3& playerPosition,
    const Vec3& playerRotation,
    const Vec3& monsterPosition,
    const Vec3& monsterRotation)
{
    _hasBattleSpawnPoseOverride = true;
    _overridePlayerSpawnPosition = playerPosition;
    _overridePlayerSpawnRotation = playerRotation;
    _overrideMonsterSpawnPosition = monsterPosition;
    _overrideMonsterSpawnRotation = monsterRotation;
}

void SoulLikeController::Clear_BattleSpawnPoseOverride()
{
    _hasBattleSpawnPoseOverride = false;
}

void SoulLikeController::Set_NextMonsterActionDelay(float delaySec)
{
    _nextMonsterActionDelay = max(0.f, delaySec);
}

void SoulLikeController::End_Battle()
{
    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);
    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Lock_Mouse();
    Clear_BattleSpawnPoseOverride();
    CLIENT->Clear_SoulLikeArenaMapRenderGateBasis();
    Remove_SpawnedBattleActors();
    Restore_PreSoulLikeBGM();
    GAME->End_SoulLikeIBLOverride();

    _state = BattleState::Finished;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;
    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
    _jumpCounterDamageApplied = false;
    _nextMonsterActionDelay = 0.f;
    _monsterActionDelayTimer = 0.f;

    if (_estherManager)
        _estherManager->Reset();

    Release_ControllerRegistration(true);
}

void SoulLikeController::Confirm_Victory()
{
    if (_state != BattleState::Victory)
        return;

    const bool shouldReturnToGolgraFront = Is_DuallisteBattle();

    Sync_PlayerResultToWorld();
    End_Battle();

    if (shouldReturnToGolgraFront)
    {
        CLIENT->Warp_PartyToFormation(_victoryPosition, _victoryRotation);

        CLIENT->Set_DialogueOverride(
            kNpcGolgraDialogueKey,
            kNpcGolgraAfterDuallisteDialogueKey);
    }
        

}

void SoulLikeController::Handle_BattleShotNotify(
    const AnimNotifyContext& context, BattleCameraMode cameraMode, const string& presetId,
    const string& clipId, bool useClip, bool restartClip, bool keepCurrentClip)
{
    if (_state != BattleState::Playing)
        return;

    if (context.owner == nullptr || context.model == nullptr)
        return;

    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr)
        return;

    auto character = dynamic_pointer_cast<Character>(
        context.owner->GetSharedPtr<PlayableParts>()->Get_Owner());

    if (character == nullptr)
        return;

    Shared<SL_Base> subject = dynamic_pointer_cast<SL_Base>(character);
    if (subject == nullptr)
        return;

    if (subject.get() != player.get())
        return;

    Shared<SL_StateMachineCom> stateMachine = player->Get_Component<SL_StateMachineCom>();
    if (stateMachine == nullptr || stateMachine->Get_CurrentSoulLikeState() != SL_PlayerState::Skill)
        return;

    EvtBattlePresentation event{};
    event.cameraMode = cameraMode;
    event.subject = subject;
    event.opponent = Resolve_PlayerCameraOpponent(player);
    event.presetId = presetId;
    event.clipId = clipId;
    event.useClip = useClip;
    event.restartClip = restartClip;
    event.keepCurrentClip = keepCurrentClip;

    if (Shared<SL_Skill> skillState = dynamic_pointer_cast<SL_Skill>(stateMachine->Get_State(SL_PlayerState::Skill)))
        event.skillId = skillState->Get_CurrentSkillKey();

    Request_BattleShotCamera(event);
}
void SoulLikeController::End_BattleShotCamera()
{
    if (!Is_BattleActive())
        return;

    Request_SoulLikeCamera();
}

void SoulLikeController::Apply_SkillCameraPreset(const string& skillKey, float clipStartTimeSec)
{
    if (_state != BattleState::Playing)
        return;

    if (skillKey.empty())
        return;

    if (!Ensure_SoulLikeCamera())
        return;

    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        soulLikeCamera->Apply_SkillPreset(skillKey, clipStartTimeSec);
}

void SoulLikeController::Sync_SkillCameraClipTime(float animationTimeSec)
{
    if (_state != BattleState::Playing)
        return;

    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        soulLikeCamera->Set_ActiveClipExternalTime(animationTimeSec);
}

void SoulLikeController::Restore_SkillCameraPreset()
{
    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        soulLikeCamera->Restore_BasePreset();
}

void SoulLikeController::Apply_PlayerJumpCameraClip(float clipStartTimeSec)
{
    if (_state != BattleState::Playing)
        return;

    if (!Ensure_SoulLikeCamera())
        return;

    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
    {
        if (soulLikeCamera->Apply_ClipOverride(kPlayerJumpCameraPresetKey, clipStartTimeSec, true))
            soulLikeCamera->Begin_TargetYLock();
    }
}

void SoulLikeController::Sync_PlayerJumpCameraClipTime(float animationTimeSec)
{
    if (_state != BattleState::Playing)
        return;

    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        soulLikeCamera->Set_ActiveClipExternalTime(animationTimeSec);
}

void SoulLikeController::Restore_PlayerJumpCameraClip()
{
    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
    {
        soulLikeCamera->Restore_PresetOverride(kPlayerJumpCameraPresetKey);
        soulLikeCamera->End_TargetYLock();
    }
}

void SoulLikeController::Begin_MonsterAttackCameraPreset(const Shared<SL_Base>& attacker, const string& presetKey)
{
    if (_state != BattleState::Playing)
        return;

    const Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr || player->Get_Transform() == nullptr)
        return;

    if (attacker == nullptr || attacker->Get_Transform() == nullptr)
        return;

    if (presetKey.empty())
        return;

    if (!Ensure_SoulLikeCamera())
        return;

    Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock();
    if (soulLikeCamera == nullptr)
        return;

    soulLikeCamera->SetCombatFocusTransforms(player->Get_Transform(), attacker->Get_Transform());
    soulLikeCamera->Apply_PresetOverride(presetKey);
    GAME->Change_ActiveCamera(static_pointer_cast<Camera>(soulLikeCamera));
    soulLikeCamera->Update(0.f);
    _monsterAttackCameraOwner = attacker;
}

void SoulLikeController::End_MonsterAttackCameraPreset(const Shared<SL_Base>& attacker)
{
    const Shared<SL_Base> currentOwner = _monsterAttackCameraOwner.lock();
    if (currentOwner == nullptr)
        return;

    if (attacker != nullptr && attacker.get() != currentOwner.get())
        return;

    _monsterAttackCameraOwner.reset();

    const Shared<SL_Maelle> player = _player.lock();
    Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock();
    if (player == nullptr || player->Get_Transform() == nullptr || soulLikeCamera == nullptr)
        return;

    soulLikeCamera->SetTargetTransform(player->Get_Transform());
    soulLikeCamera->Restore_BasePreset();
    soulLikeCamera->Update(0.f);
}

void SoulLikeController::Play_PlayerHitFeedback(const SL_DamageInfo& damageInfo)
{
    UNREFERENCED_PARAMETER(damageInfo);

    Play_PlayerParryFeedback();
}

void SoulLikeController::Play_PlayerParryFeedback()
{
    if (_state != BattleState::Playing)
        return;

    if (_playerHitStopDuration > 0.f)
        GAME->Slow_WorldTime(_playerHitStopTimeScale, _playerHitStopDuration);
}

void SoulLikeController::Notify_PlayerParrySuccess()
{
    if (_state != BattleState::Playing)
        return;

    if (_estherManager)
        _estherManager->Add_GaugeFromParry();

    Play_PlayerParryFeedback();
}

void SoulLikeController::Play_PlayerAttackHitFeedback(const SL_DamageInfo& damageInfo)
{
    UNREFERENCED_PARAMETER(damageInfo);

    if (_state != BattleState::Playing)
        return;

    GAME->Play_SoundCue(L"SFX_Hits_Sword");

    if (_playerAttackHitStopDuration <= 0.f)
        return;

    GAME->Slow_WorldTime(_playerAttackHitStopTimeScale, _playerAttackHitStopDuration);
}

void SoulLikeController::Start_DuallistePhase1BGM()
{
    if (!Is_DuallisteBattle())
        return;

    Capture_PreSoulLikeBGM();

    const wchar_t* bgm = Resolve_DuallisteBGM(DuallisteBGMPhase::Phase1);
    if (bgm == nullptr || bgm[0] == L'\0')
        return;

    if (GAME->Get_CurrentBGM() == bgm)
        return;

    GAME->Play_BGM(bgm, 0.7f, true, kDuallisteBGMFadeOutDuration);
}

void SoulLikeController::Play_DuallisteBGM(DuallisteBGMPhase phase)
{
    if (!Is_BattleActive())
        return;

    if (!Is_DuallisteBattle())
        return;

    const wchar_t* bgm = Resolve_DuallisteBGM(phase);
    if (bgm == nullptr || bgm[0] == L'\0')
        return;

    if (GAME->Get_CurrentBGM() == bgm)
        return;

    GAME->Play_BGM(bgm, 0.7f, true, kDuallisteBGMFadeOutDuration);
}

void SoulLikeController::Set_JumpCounterThreat(const Shared<SL_Base>& attacker, bool active)
{
    if (_state != BattleState::Playing)
        return;

    if (active)
    {
        if (attacker == nullptr)
            return;

        _jumpCounterAttacker = attacker;
        _jumpCounterThreatOpen = true;
        _jumpCounterJumpSucceeded = false;
        _jumpCounterCounterWindowOpen = false;
        _jumpCounterAttackStarted = false;
        _jumpCounterDamageApplied = false;
        return;
    }

    Shared<SL_Base> currentAttacker = _jumpCounterAttacker.lock();
    if (attacker != nullptr && currentAttacker != nullptr && attacker.get() != currentAttacker.get())
        return;

    _jumpCounterThreatOpen = false;

    if (!_jumpCounterJumpSucceeded && !_jumpCounterAttackStarted)
    {
        _jumpCounterAttacker.reset();
        _jumpCounterCounterWindowOpen = false;
        _jumpCounterDamageApplied = false;
    }
}

void SoulLikeController::Set_JumpCounterWindowOpen(bool open)
{
    if (_state != BattleState::Playing)
        return;

    if (open)
    {
        if (!_jumpCounterJumpSucceeded || _jumpCounterAttackStarted)
            return;

        _jumpCounterCounterWindowOpen = true;
        return;
    }

    _jumpCounterCounterWindowOpen = false;

    if (!_jumpCounterAttackStarted)
    {
        _jumpCounterAttacker.reset();
        _jumpCounterThreatOpen = false;
        _jumpCounterJumpSucceeded = false;
        _jumpCounterDamageApplied = false;
    }
}

bool SoulLikeController::Try_ConsumeJumpCounterDodge(const Shared<SL_Base>& attacker, const Shared<SL_Maelle>& target)
{
    if (_state != BattleState::Playing)
        return false;

    if (!_jumpCounterJumpSucceeded)
        return false;

    if (target == nullptr || target.get() != _player.lock().get())
        return false;

    const Shared<SL_Base> currentAttacker = _jumpCounterAttacker.lock();
    if (attacker == nullptr || currentAttacker == nullptr || attacker.get() != currentAttacker.get())
        return false;

    return true;
}

void SoulLikeController::Apply_JumpCounterDamage()
{
    if (_state != BattleState::Playing)
        return;

    if (!_jumpCounterAttackStarted || _jumpCounterDamageApplied)
        return;

    const Shared<SL_Maelle> player = _player.lock();
    const Shared<SL_Base> target = _jumpCounterAttacker.lock();
    if (player == nullptr || target == nullptr || target->Is_Dead())
        return;

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_Damage(
        player,
        player->Get_Stat(),
        target,
        target->Get_Stat(),
        _jumpCounterDamageMultiplier,
        AttackType::Counter,
        damageInfo))
    {
        return;
    }

    _jumpCounterDamageApplied = true;
    target->Apply_Damage(damageInfo);

    // 이팩트
    {
        PlayEffectDesc effectDesc{};
        effectDesc.effectName = L"Double_Distotion.effect.json";
        effectDesc.worldPosition = damageInfo.target->Get_Transform()->Get_WorldPosition() + Vec3(0.f, 1.5f, 0.f);
        effectDesc.scale = Vec3(1.f, 1.f, 1.f);
        CLIENT->Play_Effect(effectDesc);
    }

    Play_PlayerAttackHitFeedback(damageInfo);

    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
}

bool SoulLikeController::Try_ApplyHealingPotion(const Shared<SL_Maelle>& player)
{
    if (player == nullptr)
        return false;

    if (player != _player.lock())
        return false; // 현재 SL 전투 플레이어가 아닌 notify는 무시한다.

    if (_healingPotionCount == 0)
        return false;

    const Shared<PlayerStatCom> playerStat =
        dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat());

    if (playerStat == nullptr)
        return false;

    CombatStatCom::RuntimeCombatStat& runtimeStat = playerStat->Get_RuntimeStat();
    const float maxHp = max(playerStat->Get_StatInfo().maxHp, 1.f);
    const float curHp = clamp(runtimeStat.CurHp, 0.f, maxHp);

    if (curHp >= maxHp)
        return false;

    PlayEffectDesc effectDesc{};
    effectDesc.effectName = kHealTintEffect;
    effectDesc.spawnBasisTarget = player;
    effectDesc.localOffset = Vec3(0.f, 0.f, 0.f);
    effectDesc.scale = Vec3(1.f, 1.f, 1.f);

    CLIENT->Play_Effect(effectDesc);

    runtimeStat.CurHp = clamp(curHp + (maxHp * kHealingPotionHealRatio), 0.f, maxHp);
    --_healingPotionCount;

    return true;
}

float SoulLikeController::Get_EstherGaugeRatio() const
{
    return _estherManager ? _estherManager->Get_GaugeRatio() : 0.f;
}

bool SoulLikeController::Notify_EstherAttack(GameObject* notifyOwner)
{
    if (!_estherManager)
        return false;

    return _estherManager->Notify_Attack(notifyOwner);
}

bool SoulLikeController::Is_VictoryCameraBlocked() const
{
     return _state == BattleState::VictorySlow ||
        _state == BattleState::Victory;
}

void SoulLikeController::Request_SoulLikeCamera(bool alignBehindPlayer)
{
    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr || player->Get_Transform() == nullptr)
        return;

    if (!Ensure_SoulLikeCamera())
        return;

    Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock();
    if (soulLikeCamera == nullptr)
        return;

    Shared<Character> lockedTarget = nullptr;
    if (const Shared<SL_TargetCom> targetCom = player->Get_Component<SL_TargetCom>())
    {
        if (targetCom->Is_LockOn())
            lockedTarget = targetCom->Get_LockedTarget();
    }

    if (lockedTarget != nullptr && lockedTarget->Get_Transform() != nullptr)
        soulLikeCamera->SetCombatFocusTransforms(player->Get_Transform(), lockedTarget->Get_Transform());
    else
        soulLikeCamera->SetTargetTransform(player->Get_Transform());

    if (alignBehindPlayer)
        soulLikeCamera->Align_BehindTarget(); // 전투 진입 첫 프레임만 플레이어 등 뒤 기준으로 카메라를 세운다.

    GAME->Change_ActiveCamera(static_pointer_cast<Camera>(soulLikeCamera));
    soulLikeCamera->Update(0.f);
}

void SoulLikeController::Request_BattleShotCamera(const EvtBattlePresentation& event)
{
    if (_state != BattleState::Playing || event.subject == nullptr)
        return;

    if (!Ensure_BattleTurnCamera())
        return;

    CLIENT->Request_BattleTurn();
    GAME->Get_EventBus().Publish(event);
}

bool SoulLikeController::Ensure_BattleTurnCamera()
{
    if (GAME->Get_CameraByType(ObjectType::BattleTurnCamera) != nullptr)
        return true;

    BattleTurnCamera::BATTLE_TURN_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(0.f, 8.f, -15.f, 1.f);
    desc.at = XMFLOAT4(0.f, 1.2f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(45.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.15f;
    desc.mouseSensitivity = 0.f;

    Shared<GameObject> cameraObject = nullptr;
    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_BattleTurnCamera"),
            GAME->Current_LevelIndex(),
            TEXT("Layer_Camera"),
            cameraObject,
            &desc),
        false);

    Shared<BattleTurnCamera> battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(cameraObject);
    CHECK_NULL(battleTurnCamera, false);

    battleTurnCamera->Try_BeginPlay();
    battleTurnCamera->Reset_RuntimeState();
    _soulLikeBattleTurnCameraLevelIndex = GAME->Current_LevelIndex();
    _createdSoulLikeBattleTurnCamera = true;

    return true;
}

void SoulLikeController::Restore_GameplayCamera()
{
    CLIENT->Request_GamePlay();
}

bool SoulLikeController::Ensure_SoulLikeCamera()
{
    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        return true;

    if (Shared<SoulLikeCamera> soulLikeCamera =
        dynamic_pointer_cast<SoulLikeCamera>(GAME->Get_CameraByType(ObjectType::SoulLikeCamera)))
    {
        _soulLikeCamera = soulLikeCamera;
        _soulLikeCameraLevelIndex = GAME->Current_LevelIndex();
        return true;
    }

    SoulLikeCamera::SOULLIKE_CAMERA_DESC desc{};
    desc.speedPerSec = 10.f;
    desc.degreePerSec = 180.f;
    desc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    desc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(60.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.06f;
    desc.mouseSensitivity = 0.5f;

    Shared<GameObject> cameraObject = nullptr;
    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_SoulLikeCamera"),
            GAME->Current_LevelIndex(),
            TEXT("Layer_Camera"),
            cameraObject,
            &desc),
        false);

    Shared<SoulLikeCamera> soulLikeCamera = dynamic_pointer_cast<SoulLikeCamera>(cameraObject);
    CHECK_NULL(soulLikeCamera, false);

    soulLikeCamera->Try_BeginPlay();

    _soulLikeCamera = soulLikeCamera;
    _soulLikeCameraLevelIndex = GAME->Current_LevelIndex();

    return true;
}

void SoulLikeController::Remove_SoulLikeRuntimeCameras()
{
    _monsterAttackCameraOwner.reset();
    Restore_GameplayCamera();

    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        GAME->Remove_GameObject(_soulLikeCameraLevelIndex, soulLikeCamera);

    _soulLikeCamera.reset();
    _soulLikeCameraLevelIndex = 0;

    if (_createdSoulLikeBattleTurnCamera)
    {
        if (Shared<Camera> battleTurnCamera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera))
            GAME->Remove_GameObject(_soulLikeBattleTurnCameraLevelIndex, battleTurnCamera);
    }

    _createdSoulLikeBattleTurnCamera = false;
    _soulLikeBattleTurnCameraLevelIndex = 0;
}

Shared<Character> SoulLikeController::Resolve_PlayerCameraOpponent(const Shared<SL_Maelle>& player) const
{
    if (player == nullptr)
        return nullptr;

    Shared<SL_TargetCom> targetCom = player->Get_Component<SL_TargetCom>();
    if (targetCom != nullptr)
    {
        Shared<Character> lockedTarget = targetCom->Get_LockedTarget();
        if (lockedTarget != nullptr)
            return lockedTarget;
    }

    return _monster.lock();
}

bool SoulLikeController::Spawn_Player()
{
    const auto totalStartedAt = chrono::steady_clock::now();

    if (_playerPrefab.empty())
        return false;

    const auto weaponStartedAt = chrono::steady_clock::now();
    CLIENT->Ensure_DefaultEquippedWeapon(HeroId::Maelle);
    const long long weaponMs = ElapsedMs(weaponStartedAt);

    const Shared<Player_WorldBase> worldMaelle = CLIENT->Find_Hero(HeroId::Maelle);
    const Shared<PlayerStatCom> worldStat = worldMaelle
        ? worldMaelle->Get_Component<PlayerStatCom>()
        : nullptr;

    const Shared<SkillTreeCom> worldSkillTreeCom = worldMaelle
        ? worldMaelle->Get_Component<SkillTreeCom>()
        : nullptr;

    const uint32 levelIndex = GAME->Current_LevelIndex();
    const Vec3 playerSpawnPosition = _hasBattleSpawnPoseOverride
        ? _overridePlayerSpawnPosition
        : _playerSpawnPosition;
    const Vec3 playerSpawnRotation = _hasBattleSpawnPoseOverride
        ? _overridePlayerSpawnRotation
        : _playerSpawnRotation;

    const auto spawnStartedAt = chrono::steady_clock::now();
    Shared<GameObject> playerObject = Spawn_Prefab::Prefab(_playerPrefab)
        .AtLevel(levelIndex)
        .InLayer(TEXT("Layer_Player"))
        .Position(playerSpawnPosition)
        .Rotation(playerSpawnRotation)
        .Spawn();
    const long long spawnMs = ElapsedMs(spawnStartedAt);

    Shared<SL_Maelle> player = dynamic_pointer_cast<SL_Maelle>(playerObject);
    if (player == nullptr)
        return false;

    CHECK_NULL(worldStat, false);

    const auto statStartedAt = chrono::steady_clock::now();
    // 스탯 세팅
    if (FAILED(player->Apply_WorldStatSnapshot(worldStat->Create_BattleSyncSnapshot())))
        return false;
    const long long statMs = ElapsedMs(statStartedAt);

    const auto skillStartedAt = chrono::steady_clock::now();
    vector<string> soulLikeSkillKeys = worldStat->Get_SkillKeys();
    if (soulLikeSkillKeys.empty() && worldSkillTreeCom != nullptr)
        soulLikeSkillKeys = worldSkillTreeCom->Get_UnlockedSkillIds();

    soulLikeSkillKeys.erase(
        remove_if(soulLikeSkillKeys.begin(), soulLikeSkillKeys.end(),
            [](const string& skillKey)
            {
                return GradientSkill::Is_GradientSkill(HeroId::Maelle, skillKey);
            }),
        soulLikeSkillKeys.end());

    soulLikeSkillKeys.resize(4);

    player->Apply_WorldSkillKeys(soulLikeSkillKeys);
    const long long skillMs = ElapsedMs(skillStartedAt);

    if (const Shared<PlayerStatCom> playerStat = dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat()))
        player->Reset_CombatHp(playerStat->Get_PlayerRuntimeStat().CurMaxHp);

    _player = player;

    const auto monsterStartedAt = chrono::steady_clock::now();
    if (!Spawn_Monster(player))
    {
        Remove_SpawnedBattleActors();
        return false;
    }
    const long long monsterMs = ElapsedMs(monsterStartedAt);

    _playerSpawned = true;

    const auto cameraStartedAt = chrono::steady_clock::now();
    Request_SoulLikeCamera(true);
    const long long cameraMs = ElapsedMs(cameraStartedAt);

    LOG_INFO(
        "[SoulLikeEnterPerf] Spawn_Player prefab={}, weapon={}ms, spawn={}ms, stat={}ms, skill={}ms, monster={}ms, camera={}ms, total={}ms",
        _playerPrefab,
        weaponMs,
        spawnMs,
        statMs,
        skillMs,
        monsterMs,
        cameraMs,
        ElapsedMs(totalStartedAt));

    return true;
}

bool SoulLikeController::Spawn_Monster(const Shared<SL_Maelle>& player)
{
    const auto totalStartedAt = chrono::steady_clock::now();

    if (_monsterPrefab.empty())
    {
        Clear_BattleSpawnPoseOverride();
        return true;
    }

    if (player == nullptr || player->Get_Transform() == nullptr)
        return false;

    Vec3 opponentPosition = _overrideMonsterSpawnPosition;
    Vec3 opponentRotation = _overrideMonsterSpawnRotation;

    if (!_hasBattleSpawnPoseOverride)
    {
        Vec3 forward = player->Get_Transform()->Get_WorldForward();
        forward.y = 0.f;

        if (forward.LengthSquared() <= 0.0001f)
            forward = Vec3::Look;
        else
            forward.Normalize();

        const Vec3 playerPosition = player->Get_Transform()->Get_WorldPosition();
        opponentPosition = playerPosition + forward * max(0.f, _monsterSpawnDistance);
        opponentPosition.y = playerPosition.y;

        opponentRotation = player->Get_Transform()->Get_WorldRotationEuler();
        opponentRotation.y += 180.f;
    }
    else if (Is_DuallisteBattle())
    {
        opponentPosition = ApplyDuallisteCinematicMonsterBackOffset(
            player->Get_Transform()->Get_WorldPosition(),
            player->Get_Transform()->Get_WorldRotationEuler(),
            opponentPosition);
    }

    const auto spawnStartedAt = chrono::steady_clock::now();
    Shared<GameObject> monsterObject = Spawn_Prefab::Prefab(_monsterPrefab)
        .AtLevel(GAME->Current_LevelIndex())
        .InLayer(TEXT("Layer_Monster"))
        .Position(opponentPosition)
        .Rotation(opponentRotation)
        .Spawn();
    const long long spawnMs = ElapsedMs(spawnStartedAt);

    Shared<SL_Base> opponent = dynamic_pointer_cast<SL_Base>(monsterObject);
    if (opponent == nullptr)
        return false;

    _monster = opponent;

    if (_nextMonsterActionDelay > 0.f)
    {
        if (const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(opponent))
        {
            dualliste->Set_ActionLocked(true);
            _monsterActionDelayTimer = _nextMonsterActionDelay;
        }

        _nextMonsterActionDelay = 0.f;
    }

    Clear_BattleSpawnPoseOverride();

    LOG_INFO(
        "[SoulLikeEnterPerf] Spawn_Monster prefab={}, spawn={}ms, total={}ms",
        _monsterPrefab,
        spawnMs,
        ElapsedMs(totalStartedAt));

    return true;
}

void SoulLikeController::Remove_SpawnedBattleActors()
{
    const uint32 levelIndex = GAME->Current_LevelIndex();

    if (Shared<SL_Base> opponent = _monster.lock())
        GAME->Remove_GameObject(levelIndex, opponent);

    if (Shared<SL_Maelle> player = _player.lock())
        GAME->Remove_GameObject(levelIndex, player);

    _monster.reset();
    _player.reset();
    _nextMonsterActionDelay = 0.f;
    _monsterActionDelayTimer = 0.f;

    Remove_SoulLikeRuntimeCameras();
}

void SoulLikeController::Apply_SoulLikeArenaMapRenderGateBasis()
{
    if (!Is_DuallisteBattle())
    {
        CLIENT->Clear_SoulLikeArenaMapRenderGateBasis();
        return;
    }

    const Vec3 playerPosition = _hasBattleSpawnPoseOverride
        ? _overridePlayerSpawnPosition
        : _playerSpawnPosition;
    const Vec3 playerRotation = _hasBattleSpawnPoseOverride
        ? _overridePlayerSpawnRotation
        : _playerSpawnRotation;

    Vec3 opponentPosition = _overrideMonsterSpawnPosition;

    if (!_hasBattleSpawnPoseOverride)
    {
        const Vec3 forward = MakePlanarForwardFromRotationDegree(playerRotation);

        opponentPosition = playerPosition + forward * max(0.f, _monsterSpawnDistance);
        opponentPosition.y = playerPosition.y;
    }
    else
    {
        opponentPosition = ApplyDuallisteCinematicMonsterBackOffset(
            playerPosition,
            playerRotation,
            opponentPosition);
    }

    Vec3 forward = opponentPosition - playerPosition;
    forward.y = 0.f;

    if (forward.LengthSquared() <= 0.0001f)
        forward = Vec3::Look;
    else
        forward.Normalize();

    const Vec3 right = Vec3(-forward.z, 0.f, forward.x);
    const Vec3 origin = (playerPosition + opponentPosition) * 0.5f;

    ClientInstance::Set_SoulLikeArenaMapRenderGateBasis(origin, right, forward);
}

void SoulLikeController::Update_EnterFx(float timeDelta)
{
    _enterFxTimer -= timeDelta;

    if (_enterFxTimer > 0.f)
        return;

    const auto spawnStartedAt = chrono::steady_clock::now();
    if (!_playerSpawned && !Spawn_Player())
    {
        CLIENT->Clear_SoulLikeArenaMapRenderGateBasis();
        Restore_PreSoulLikeBGM();
        _state = BattleState::Finished;
        LOG_INFO(
            "[SoulLikeEnterPerf] Update_EnterFx spawn failed elapsed={}ms",
            ElapsedMs(spawnStartedAt));
        return;
    }
    const long long spawnMs = ElapsedMs(spawnStartedAt);

    _state = BattleState::Playing;
    LOG_INFO(
        "[SoulLikeEnterPerf] Update_EnterFx enter playing spawn={}ms",
        spawnMs);
}

void SoulLikeController::Update_MonsterActionDelay(float timeDelta)
{
    if (_monsterActionDelayTimer <= 0.f)
        return;

    _monsterActionDelayTimer = max(0.f, _monsterActionDelayTimer - timeDelta);
    if (_monsterActionDelayTimer > 0.f)
        return;

    if (const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(_monster.lock()))
        dualliste->Set_ActionLocked(false);
}

void SoulLikeController::Update_Player(float timeDelta)
{
    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr)
        return;

    Shared<SL_StateMachineCom> stateMachine = player->Get_Component<SL_StateMachineCom>();
    CHECK_NULL(stateMachine);

    Shared<SL_TargetCom> targetCom = player->Get_Component<SL_TargetCom>();
    if (targetCom && GAME->KeyDown(KEY_TYPE::T))
        targetCom->Toggle_LockOn();

    SL_MoveRequest moveRequest{};

    if (GAME->KeyPress(KEY_TYPE::W)) moveRequest.moveZ += 1.f;
    if (GAME->KeyPress(KEY_TYPE::S)) moveRequest.moveZ -= 1.f;
    if (GAME->KeyPress(KEY_TYPE::A)) moveRequest.moveX -= 1.f;
    if (GAME->KeyPress(KEY_TYPE::D)) moveRequest.moveX += 1.f;

    moveRequest.worldDir = Vec3(moveRequest.moveX, 0.f, moveRequest.moveZ);

    if (moveRequest.worldDir.LengthSquared() > 0.f)
        moveRequest.worldDir.Normalize();

    moveRequest.sprint = GAME->KeyPress(KEY_TYPE::SHIFT);
    moveRequest.lockOn = targetCom != nullptr && targetCom->Is_LockOn();

    if (Update_Jump(stateMachine))
        return;

    Update_Dodge(stateMachine, moveRequest);

    // TODO : 포션 먹을 때 간단한 애니메이션 + 이팩트 추가 (이팩트는 그대로)
    if (Update_HealingPotion(player, stateMachine))
        return;

    if (Update_Esther())
        return;

    if (Update_Parry(stateMachine))
        return;

    if (Update_Skill(player, stateMachine))
        return;

    stateMachine->Request_Move(moveRequest, timeDelta);

    Update_BaseAttack(stateMachine);
}

void SoulLikeController::Check_BattleEnd()
{
    Shared<SL_Maelle> player = _player.lock();
    Shared<SL_Base> opponent = _monster.lock();

    if (player == nullptr || player->Is_Dead())
    {
        End_Battle();
        return;
    }

    if (!_monsterPrefab.empty() && (opponent == nullptr || opponent->Is_Dead()))
        Begin_VictorySlow();
}

void SoulLikeController::Update_VictorySlow()
{
    _victorySlowElapsed += GAME->Get_UnscaledTimeDelta();

    if (_victorySlowElapsed < max(0.f, _victorySlowDuration))
        return;

    GAME->Set_WorldTimeScale(0.0001f);
    _victorySlowElapsed = 0.f;
    Begin_Victory();
}

void SoulLikeController::Begin_VictorySlow()
{
    if (_state == BattleState::VictorySlow || _state == BattleState::Victory)
        return;

    _state = BattleState::VictorySlow;
    _victorySlowElapsed = 0.f;

    if (_victorySlowDuration <= 0.f)
    {
        Begin_Victory();
        return;
    }

    GAME->Set_WorldTimeScale(_victorySlowTimeScale);
}

void SoulLikeController::Begin_Victory()
{
    if (_state == BattleState::Victory)
        return;

    GAME->Set_WorldTimeScale(0.0001f);
    Apply_VictoryRewardExp();
    Apply_VictoryUnlockRewards();

    _state = BattleState::Victory;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;

    if (const Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI())
    {
        if (GamePlayUI_SL* soulLikeUI = gameplayUI->Get_SoulLikeUI())
            soulLikeUI->Show_Victory();
    }
}

void SoulLikeController::Apply_VictoryRewardExp()
{
    if (_lastVictoryRewardExp > 0)
        return;

    const Shared<SL_Base> monster = _monster.lock();
    if (monster == nullptr)
        return;

    const Shared<CombatStatCom> monsterStat = monster->Get_Stat();
    if (monsterStat == nullptr)
        return;

    _lastVictoryRewardExp = monsterStat->Get_StatInfo().exp;

    if (_lastVictoryRewardExp > 0)
    {
        for (const HeroPartyEntry& entry : CLIENT->Get_HeroEntries())
        {
            const Shared<PlayerStatCom> worldStat = entry.hero
                ? entry.hero->Get_Component<PlayerStatCom>()
                : nullptr;

            if (worldStat != nullptr)
                worldStat->Add_Exp(_lastVictoryRewardExp);
        }

        const Shared<SL_Maelle> player = _player.lock();
        const Shared<PlayerStatCom> playerStat = player
            ? dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat())
            : nullptr;

        if (playerStat != nullptr)
            playerStat->Add_Exp(_lastVictoryRewardExp);
    }
}

void SoulLikeController::Apply_VictoryUnlockRewards()
{
    if (!_lastVictoryRewardPictosIds.empty() || _monsterPrefab.empty())
        return;

    vector<string> defeatedEnemyKeys{};
    defeatedEnemyKeys.reserve(2);

    const Shared<SL_Base> monster = _monster.lock();
    if (monster != nullptr && !monster->Get_SourcePrefabName().empty())
        defeatedEnemyKeys.emplace_back(monster->Get_SourcePrefabName());

    if (find(defeatedEnemyKeys.begin(), defeatedEnemyKeys.end(), _monsterPrefab) == defeatedEnemyKeys.end())
        defeatedEnemyKeys.emplace_back(_monsterPrefab);

    Vec3 rewardOrigin = Vec3::Zero;
    const Vec3* rewardOriginPtr = nullptr;
    if (monster != nullptr)
    {
        if (const Shared<TransformCom> monsterTransform = monster->Get_Transform())
        {
            rewardOrigin = monsterTransform->Get_WorldPosition();
            rewardOriginPtr = &rewardOrigin;
        }
    }

    if (Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
    {
        _lastVictoryRewardPictosIds = unlockManager->Handle_BattleVictory(
            defeatedEnemyKeys,
            0,
            GAME->Current_LevelIndex(),
            rewardOriginPtr);
    }
}

void SoulLikeController::Sync_PlayerResultToWorld()
{
    const Shared<SL_Maelle> player = _player.lock();
    const Shared<PlayerStatCom> playerStat = player
        ? dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat())
        : nullptr;

    const Shared<Player_WorldBase> worldMaelle = CLIENT->Find_Hero(HeroId::Maelle);
    const Shared<PlayerStatCom> worldStat = worldMaelle
        ? worldMaelle->Get_Component<PlayerStatCom>()
        : nullptr;

    if (playerStat == nullptr || worldStat == nullptr)
        return;

    const PlayerStatCom::PRuntimeCombatStat& runtimeStat = playerStat->Get_PlayerRuntimeStat();
    worldStat->Set_RuntimeProgress(
        runtimeStat.CurLevel,
        runtimeStat.CurExp,
        runtimeStat.CurStatPoint,
        runtimeStat.CurSkillPoint,
        runtimeStat.CurRespecCount);
}

void SoulLikeController::Handle_EditorStopRuntime()
{
    Remove_SpawnedBattleActors();
    CLIENT->Clear_SoulLikeArenaMapRenderGateBasis();
    Restore_PreSoulLikeBGM();
    GAME->End_SoulLikeIBLOverride();

    _state = BattleState::None;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;
    _lastVictoryRewardExp = 0;
    _playerSpawned = false;
    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
    _jumpCounterDamageApplied = false;

    if (_estherManager)
        _estherManager->Reset();

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);
    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Lock_Mouse();

    if (auto screenFxManager = GAME->Get_ScreenFxManager())
        screenFxManager->Clear_ScreenEffect();

    Release_ControllerRegistration(true);
}

void SoulLikeController::Release_ControllerRegistration(bool removeFromWorld)
{
    if (CLIENT->Get_SoulLikeController().get() == this)
        CLIENT->Set_SoulLikeController(nullptr);

    if (removeFromWorld)
        GAME->Remove_GameObject(GAME->Current_LevelIndex(), GetSharedPtr<GameObject>());
}

void SoulLikeController::Capture_PreSoulLikeBGM()
{
    if (_hasPreSoulLikeBGM)
        return;

    _preSoulLikeBGM = GAME->Get_CurrentBGM();
    _hasPreSoulLikeBGM = true;
}

void SoulLikeController::Restore_PreSoulLikeBGM()
{
    if (!_hasPreSoulLikeBGM)
        return;

    if (_preSoulLikeBGM.empty())
        GAME->Stop_SoundChannel(SoundChannel::BGM, kDuallisteBGMFadeOutDuration);
    else
        GAME->Play_BGM(_preSoulLikeBGM, 0.7f, true, kDuallisteBGMFadeOutDuration);

    _preSoulLikeBGM.clear();
    _hasPreSoulLikeBGM = false;
}

bool SoulLikeController::Is_DuallisteBattle() const
{
    return _monsterPrefab == Constants::Prefab::SL_Dualliste;
}

const wchar_t* SoulLikeController::Resolve_DuallisteBGM(DuallisteBGMPhase phase) const
{
    switch (phase)
    {
    case DuallisteBGMPhase::Phase1:
        return Constants::SoundBGM::DuallisteBattlePhase1Main;

    case DuallisteBGMPhase::PhaseTransition:
        return Constants::SoundBGM::DuallisteBattlePhaseTransition;

    case DuallisteBGMPhase::Phase2:
        return Constants::SoundBGM::DuallisteBattlePhase2Main;

    default:
        return nullptr;
    }
}

void SoulLikeController::Update_BaseAttack(const Shared<SL_StateMachineCom>& stateMachine)
{
    if (stateMachine == nullptr)
        return;

    if (!GAME->KeyDown(KEY_TYPE::LBUTTON))
        return;

    if (_jumpCounterCounterWindowOpen &&
        _jumpCounterJumpSucceeded &&
        !_jumpCounterAttackStarted)
    {
        if (stateMachine->Request_JumpCounter())
        {
            _jumpCounterAttackStarted = true;
            _jumpCounterCounterWindowOpen = false;
            return;
        }
    }

    stateMachine->Request_BasicAttack();
}

bool SoulLikeController::Update_Jump(const Shared<SL_StateMachineCom>& stateMachine)
{
    if (stateMachine == nullptr)
        return false;

    if (GAME->KeyDown(KEY_TYPE::SPACE) &&
        _jumpCounterThreatOpen &&
        !_jumpCounterJumpSucceeded &&
        !_jumpCounterAttackStarted)
    {
        if (!stateMachine->Request_Jump())
            return false;

        _jumpCounterJumpSucceeded = true;
        _jumpCounterCounterWindowOpen = false;
        return true;
    }

    return false;
}

bool SoulLikeController::Update_Parry(const Shared<SL_StateMachineCom>& stateMachine)
{
    if (stateMachine == nullptr)
        return false;

    if (!GAME->KeyDown(KEY_TYPE::E))
        return false;

    return stateMachine->Request_Parry();
}

bool SoulLikeController::Update_Skill(const Shared<SL_Maelle>& player, const Shared<SL_StateMachineCom>& stateMachine)
{
    if (player == nullptr || stateMachine == nullptr)
        return false;

    Skill_Data skillData{};
    uint32 skillSlotIndex = UINT32_MAX;

    if (GAME->KeyDown(KEY_TYPE::Z))      skillSlotIndex = 0;
    else if (GAME->KeyDown(KEY_TYPE::X)) skillSlotIndex = 1;
    else if (GAME->KeyDown(KEY_TYPE::C)) skillSlotIndex = 2;
    else if (GAME->KeyDown(KEY_TYPE::V)) skillSlotIndex = 3;

    if (skillSlotIndex == UINT32_MAX)
        return false;

    if (!player->Can_UseSkillSlot(skillSlotIndex, skillData))
        return false;

    if (!stateMachine->Request_Skill(skillData, skillSlotIndex))
        return false;

    player->Start_SkillCooldown(skillSlotIndex, skillData);

    return true;
}

void SoulLikeController::Update_Dodge(const Shared<SL_StateMachineCom>& stateMachine, const SL_MoveRequest& moveRequest)
{
    if (stateMachine == nullptr)
        return;

    if (GAME->KeyDown(KEY_TYPE::SPACE))
    {
        SL_MoveRequest dodgeRequest = moveRequest;

        if (GAME->KeyDown(KEY_TYPE::A))
            dodgeRequest.moveX = -1.f; // 반대 방향 홀드 중 새로 누른 대쉬 방향을 우선한다.
        else if (GAME->KeyDown(KEY_TYPE::D))
            dodgeRequest.moveX = 1.f;

        if (GAME->KeyDown(KEY_TYPE::W))
            dodgeRequest.moveZ = 1.f;
        else if (GAME->KeyDown(KEY_TYPE::S))
            dodgeRequest.moveZ = -1.f;

        dodgeRequest.worldDir = Vec3(dodgeRequest.moveX, 0.f, dodgeRequest.moveZ);
        if (dodgeRequest.worldDir.LengthSquared() > 0.f)
            dodgeRequest.worldDir.Normalize();

        stateMachine->Request_Dodge(dodgeRequest);
        return;
    }
}

bool SoulLikeController::Update_HealingPotion(const Shared<SL_Maelle>& player,
    const Shared<SL_StateMachineCom>& stateMachine)
{
    if (player == nullptr || stateMachine == nullptr)
        return false;

    if (!GAME->KeyDown(KEY_TYPE::KEY_1))
        return false;

    return stateMachine->Change_State(SL_PlayerState::UseItem);
}

bool SoulLikeController::Update_Esther()
{
    if (_estherManager == nullptr)
        return false;

    if (GAME->Is_UIInputEnabled() && GAME->Get_GameplayInputMode() == GameplayInputMode::UIOnly)
        return false;

    if (GAME->KeyDown(KEY_TYPE::KEY_8))
    {
        _estherManager->Fill_GaugeForDebug();
        return true;
    }


    if (GAME->KeyDown(KEY_TYPE::KEY_3))
        return _estherManager->Request_Esther(SL_EstherType::GustaveOvercharge);

    if (GAME->KeyDown(KEY_TYPE::KEY_4))
        return _estherManager->Request_Esther(SL_EstherType::LuneImmolation);

    return false;
}

Shared<SoulLikeController> SoulLikeController::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SoulLikeController>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : SoulLikeController");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SoulLikeController::Clone(void* arg)
{
    auto clone = make_shared<SoulLikeController>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SoulLikeController");
        return nullptr;
    }

    return clone;
}

void SoulLikeController::Free()
{
    GAME->End_SoulLikeIBLOverride();

    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    if (_estherManager)
    {
        _estherManager->Free();
        _estherManager.reset();
    }

    Release_ControllerRegistration(false);

    GameObject::Free();
}
