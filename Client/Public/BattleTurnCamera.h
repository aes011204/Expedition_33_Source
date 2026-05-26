#pragma once
#include <Event_Struct.h>
#include "Camera.h"
#include "Battle_Enum.h"
#include "BattleTurnCameraPoseUtils.h"
#include "CameraPresetTypes.h"
#include "BattleCameraClipTypes.h"
#include "BattleCameraProfileTypes.h"

NS_BEGIN(Client)
class BattleTurnCamera final : public Camera
{
    GENERATED_GAMEOBJECT(BattleTurnCamera, ObjectType::BattleTurnCamera)

public:
    using BATTLE_TURN_CAMERA_DESC = Camera::CAMERA_DESC;
    using EvaluatedPose = BattleTurnCameraEvaluatedPose;

    struct BattleShotRequest
    {
        BattleCameraMode mode = BattleCameraMode::WIDE;
        Shared<Engine::Character> subject = nullptr;
        Shared<Engine::Character> opponent = nullptr;
        string          skillId;
    };

public:
    BattleTurnCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    BattleTurnCamera(const BattleTurnCamera& prototype);
    ~BattleTurnCamera() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;

public:
    void Focus_Battle(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies);
    const BattleCameraPreset& Get_Preset(BattleCameraMode mode) const;
    void Set_Preset(BattleCameraMode mode, const BattleCameraPreset& preset);
    void Set_Clip(BattleCameraMode mode, const BattleCameraClip& clip);
    void Set_ClipScrubPreviewEnabled(bool enabled) { _isClipPreview = enabled; }
    void Apply_PreviewPreset(const BattleCameraPreset& preset);
    void Apply_ShotRequest(const BattleShotRequest& request);
    void Preview_ClipAtTime(const BattleShotRequest& request, float time);
    void Begin_LiveEdit(BattleCameraMode mode, const Shared<Character>& target, const Shared<Character>& opponent);
    void Update_LiveEditContext(BattleCameraMode mode, const Shared<Character>& target, const Shared<Character>& opponent);
    void End_LiveEdit();
    bool Is_LiveEditEnabled() const { return _isLiveEditEnabled; }
    void Set_Participants(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies);

    const BattleCameraProfile* Resolve_BattleCameraProfile(const BattleShotRequest& request) const;

    void Update_FreeAimRotation(float timeDelta);
    Vec3 Get_AimOrigin() const;                // 히트스캔 시작 위치
    Vec3 Get_AimDirection() const;             // 히트스캔 방향

    bool Is_FreeAimActive() const { return _isFreeAimActive; }

    void Set_IsClipPreview(bool boolean) { _isClipPreview = boolean; }

private:
    struct PoseTransitionState
    {
        bool active = false;
        float elapsed = 0.f;
        float duration = 0.35f;
        EvaluatedPose fromPose{};
        EvaluatedPose toPose{};
        BattleShotRequest targetRequest{};
        BattleCameraPreset targetPreset{};
        BattleCameraClip* targetActiveClip = nullptr;
        Vec3 targetSmoothedTargetPos = Vec3::Zero;
        Vec3 targetPrevTargetPos = Vec3::Zero;
    };

private:
    void Apply_LookAt(const Vec3& cameraPos, const Vec3& lookAt);
    void Apply_FreeAimCamera();                
    void Apply_EvaluatedPose(const EvaluatedPose& pose);
    EvaluatedPose Capture_CurrentPose();
    bool Evaluate_ShotRequestPose(
        const BattleShotRequest& request,
        const BattleCameraPreset& preset,
        const Vec3* smoothedTargetPos,
        float clipTime,
        EvaluatedPose& outPose) const;
    bool Should_StartPoseTransition(const BattleShotRequest& request) const;
    bool Start_PoseTransition(
        const BattleShotRequest& request,
        const BattleCameraPreset& preset,
        BattleCameraClip* activeClip,
        const Vec3& targetSmoothedTargetPos,
        const Vec3& targetPrevTargetPos);
    void Update_PoseTransition(float timeDelta);
    bool Evaluate_CurrentPresetPose(
        const Vec3& targetPos,
        const BattleCameraPreset& preset,
        const BattleCameraClipSample& clipSample,
        float yawBase,
        float pitchBase,
        const Vec3* smoothedTargetPos,
        EvaluatedPose& outPose) const;
    bool Evaluate_EnemyAttackPose(
        const Vec3& subjectPos,
        const Vec3& opponentPos,
        const BattleCameraPreset& preset,
        const BattleCameraClipSample& clipSample,
        EvaluatedPose& outPose) const;

private:
    void Update_WideMode(float timeDelta);
    void Update_PlayerFocusMode(float timeDelta);
    void Update_EnemyFocusMode(float timeDelta);
    void Update_EnemyAttackMode(float timeDelta);
    void Update_FromCurrentPreset(float timeDelta);
    void Preview_CurrentPresetAtTime(float time);
    void Preview_EnemyAttackAtTime(float time);
    void Update_LiveEditInput(float timeDelta);
    void Update_PlayerAttackMode(float timeDelta);
public:
    bool Evaluate_PreviewCurrentPresetPose(
        const Vec3& targetPos,
        const BattleCameraPreset& preset,
        const BattleCameraClipSample& clipSample,
        float yawBase,
        float pitchBase,
        EvaluatedPose& outPose) const;
    bool Evaluate_PreviewEnemyAttackPose(
        const Vec3& subjectPos,
        const Vec3& opponentPos,
        const BattleCameraPreset& preset,
        const BattleCameraClipSample& clipSample,
        EvaluatedPose& outPose) const;

    BattleCameraPreset* Get_MutablePreset(BattleCameraMode cameraMode);
    BattleCameraClip* Get_MutableClip(BattleCameraMode cameraMode);


private:
    BattleCameraClipSample   Advance_ActiveClip(float timeDelta);

private:
    bool                  _isClipPreview = false;
    BattleShotRequest    _currentRequest{};
    BattleCameraPreset   _currentPreset{};
    BattleCameraPreset   _targetPreset{};
    BattleCameraPreset   _wideModePreset{};
    BattleCameraPreset   _playerFocusPreset{};
    BattleCameraPreset   _enemyFocusPreset{};
    BattleCameraPreset   _enemyAttackPreset{};
    BattleCameraPreset   _playerAttackPreset{};
    Shared<TransformCom> _targetTransform{};
    Vec3                 _smoothedTargetPos = Vec3::Zero;
    Vec3                 _prevTargetPos = Vec3::Zero;
    bool                 _isLiveEditEnabled = false;
    EvtHandle            _battlePresentationHandle {};
    bool                 _isFreeAimActive = false;
    bool                 _isWideMode = false;
    bool                 _isFirstUpdate = true;
    vector<BattleChar>   _heroes;
    vector<BattleChar>   _enemies;
    Vec3                 _enemyPos;
    Vec3                 _playerPos;

    BattleCameraClip      _enemyAttackClip {};
    BattleCameraClip      _playerAttackClip {};
    BattleCameraClip      _wideModeClip {};
    BattleCameraClip      _playerFocusClip {};
    BattleCameraClip      _enemyFocusClip {};
    BattleCameraClip*     _activeClip {};
    float                _clipTime {};

    BattleCameraProfile   _playerAttackDefaultProfile{};
    umap<string, BattleCameraProfile> _playerAttackProfilesBySkill;
    PoseTransitionState   _poseTransition{};

public:
    static Shared<BattleTurnCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
