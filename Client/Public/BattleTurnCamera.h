#pragma once
#include <Event_Struct.h>

#include "Camera.h"
#include "Battle_Enum.h"
#include "CameraPresetTypes.h"
#include "BattleCameraClipTypes.h"

NS_BEGIN(Client)
class BattleTurnCamera final : public Camera
{
    GENERATED_GAMEOBJECT(BattleTurnCamera, ObjectType::BattleTurnCamera)

public:
    using BATTLE_TURN_CAMERA_DESC = Camera::CAMERA_DESC;

    struct BattleShotRequest
    {
        BattleCameraMode mode = BattleCameraMode::WIDE;
        Shared<Engine::Character> subject = nullptr;
        Shared<Engine::Character> opponent = nullptr;
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
    void Apply_PreviewPreset(const BattleCameraPreset& preset);
    void Apply_ShotRequest(const BattleShotRequest& request);
    void Begin_LiveEdit(BattleCameraMode mode, const Shared<Character>& target, const Shared<Character>& opponent);
    void Update_LiveEditContext(BattleCameraMode mode, const Shared<Character>& target, const Shared<Character>& opponent);
    void End_LiveEdit();
    bool Is_LiveEditEnabled() const { return _isLiveEditEnabled; }
    void Set_Participants(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies);

    void Update_FreeAimRotation(float timeDelta);

    Vec3 Get_AimOrigin() const;                // 히트스캔 시작 위치
    Vec3 Get_AimDirection() const;             // 히트스캔 방향

    bool Is_FreeAimActive() const { return _isFreeAimActive; }

private:
    void Apply_LookAt(const Vec3& cameraPos, const Vec3& lookAt);
    void Apply_FreeAimCamera();                


private:
    void Update_WideMode(float timeDelta);
    void Update_PlayerFocusMode(float timeDelta);
    void Update_EnemyFocusMode(float timeDelta);
    void Update_EnemyAttackMode(float timeDelta);
    void Update_FromCurrentPreset(float timeDelta);
    void Update_LiveEditInput(float timeDelta);
    void Update_PlayerAttackMode(float timeDelta);

    BattleCameraPreset* Get_MutablePreset(BattleCameraMode mode);

private:
    float   Sample_Track(const BattleCameraTrack& battleCameraTrack, float time);
    BattleCameraClipSample   Sample_Clip(const BattleCameraClip& battleCameraClip,float time);

private:
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
    BattleCameraClip*     _activeClip {};
    float                _clipTime {};

public:
    static Shared<BattleTurnCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
