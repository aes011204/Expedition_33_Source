#include "BattleTurnCamera.h"
#include <CameraPresetIO.h>

#include "BattleCameraClipIO.h"
#include "GameInstance.h"
#include "Character.h"
#include "Event_Bus.h"
#include "Battle_Event.h"

IMPLEMENT_REFLECTION(BattleTurnCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "BattleTurnCamera";
    info.category = "Camera";

    return true;
}

BattleTurnCamera::BattleTurnCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera(device, context)
{
}

BattleTurnCamera::BattleTurnCamera(const BattleTurnCamera& prototype)
    : Camera(prototype)
    , _isFreeAimActive(prototype._isFreeAimActive)
{
}

HRESULT BattleTurnCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT BattleTurnCamera::Initialize(void* arg)
{
    CHECK_FAILED(Camera::Initialize(arg), E_FAIL);

    const auto desc = static_cast<BATTLE_TURN_CAMERA_DESC*>(arg);
    if (desc)
    {
        _mouseSensor = desc->mouseSensor;
    }

    Set_Name(L"Battle Turn Camera");

    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::WIDE, _wideModePreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERFOCUS, _playerFocusPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYFOCUS, _enemyFocusPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackPreset);

    _enemyAttackClip.duration = 5.f;

    //_enemyAttackClip.distanceOffset.keyframes.push_back({0.5f, 0.0f});
    //_enemyAttackClip.distanceOffset.keyframes.push_back({1.f, -1.0f});
    //_enemyAttackClip.distanceOffset.keyframes.push_back({2.f, -0.4f});
    //_enemyAttackClip.distanceOffset.keyframes.push_back({3.f, 0.0f});

   // _enemyAttackClip.fovOffset.keyframes.push_back({0.5f, XMConvertToRadians(0.f)});
   // _enemyAttackClip.fovOffset.keyframes.push_back({1.f, XMConvertToRadians(-2.f)});
   // _enemyAttackClip.fovOffset.keyframes.push_back({2.f, XMConvertToRadians(-1.f)});
   // _enemyAttackClip.fovOffset.keyframes.push_back({3.f,  XMConvertToRadians(0.f)});
   //
   //
   // _playerAttackClip.duration = 5.f;
   // _playerAttackClip.fovOffset.keyframes.push_back({0.5f, XMConvertToRadians(0.f)});
   // _playerAttackClip.fovOffset.keyframes.push_back({1.f, XMConvertToRadians(-3.f)});
   // _playerAttackClip.fovOffset.keyframes.push_back({2.f, XMConvertToRadians(-1.f)});
   // _playerAttackClip.fovOffset.keyframes.push_back({3.f,  XMConvertToRadians(0.f)});

   // CameraPresetIO::SaveBattleCameraClipToFile(BattleCameraMode::PLAYERATTACK, _playerAttackClip);
   // CameraPresetIO::SaveBattleCameraClipToFile(BattleCameraMode::ENEMYATTACK, _enemyAttackClip);
    //_enemyAttackClip.shakeAmplitude.keyframes.push_back({0.5f, 0.1f});
   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackClip);
   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackClip);

   _battlePresentationHandle = GAME->Get_EventBus().Subscribe<EvtBattlePresentation>
    ([this](const EvtBattlePresentation& event)
        {
            BattleShotRequest request;

            switch(event.cameraMode)
            {
                case BattleCameraMode::ENEMYFOCUS:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    break;

                case BattleCameraMode::PLAYERFOCUS:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    break;

                case BattleCameraMode::WIDE:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    break;

                case BattleCameraMode::ENEMYATTACK:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.opponent = event.opponent;
                    break;

                case BattleCameraMode::PLAYERATTACK:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    break;
            }

            Apply_ShotRequest(request);
        }
    );

    _currentRequest.mode = BattleCameraMode::WIDE;
    _currentPreset = _wideModePreset;
    _targetPreset = _wideModePreset;

    return S_OK;
}

void BattleTurnCamera::BeginPlay()
{
    Camera::BeginPlay();

    GAME->Add_Camera(GetSharedPtr<Camera>());
}

void BattleTurnCamera::Update(float timeDelta)
{
    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    LOG_DEBUG("Camera Mode : ", magic_enum::enum_name(_currentRequest.mode));

    if (_isLiveEditEnabled)
        Update_LiveEditInput(timeDelta);

    if(_isClipPreview)
        return;

    switch (_currentRequest.mode)
    {
    case BattleCameraMode::WIDE:
        {
         if(!_isWideMode)
         {
            Focus_Battle(_heroes, _enemies);
             _isWideMode = true;
         }
        }
        break;
    case BattleCameraMode::PLAYERFOCUS:
        Update_PlayerFocusMode(timeDelta);
        break;
    case BattleCameraMode::ENEMYFOCUS:
        Update_EnemyFocusMode(timeDelta);
        break;
    case BattleCameraMode::ENEMYATTACK:
        Update_EnemyAttackMode(timeDelta);
        break;
    case BattleCameraMode::PLAYERATTACK:
        Update_PlayerAttackMode(timeDelta);
        break;
    }

    Update_TransformMatrices();
}

void BattleTurnCamera::Focus_Battle(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies)
{
    // 위치는 일단 임시
    Vec3 heroCenter = Vec3::Zero;
    uint32 heroCount = 0;

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        if (heroes[i].actor && heroes[i].actor->Get_Transform())
        {
            // 플레이어 3명 위치 누적
            heroCenter += heroes[i].actor->Get_Transform()->Get_WorldPosition();
            ++heroCount;
        }
    }

    if (heroCount == 0)
        return;

    heroCenter /= static_cast<float>(heroCount);

    // 시작 위치 기준점으로 잡을 플레이어
    Shared<GameObject> yawBaseActor = nullptr;

    if (heroes.size() > 1 && heroes[1].actor && heroes[1].actor->Get_Transform())
        yawBaseActor = heroes[1].actor; 
    else if (!heroes.empty() && heroes[0].actor && heroes[0].actor->Get_Transform())
        yawBaseActor = heroes[0].actor; // hero[1]이 없으면 hero[0] fallback

    CHECK_NULL(yawBaseActor);

    const Vec3 baseRot = yawBaseActor->Get_Transform()->Get_RotationEuler();
    const float cameraYawDeg = baseRot.y;
    const float cameraYawRad = XMConvertToRadians(cameraYawDeg);

    Vec3 forwardDir{};
    forwardDir.x = sinf(cameraYawRad);
    forwardDir.y = 0.f;
    forwardDir.z = cosf(cameraYawRad);
    forwardDir.Normalize();

    // 카메라 위치는 플레이어 뒤쪽
    Vec3 backDir = -forwardDir;
    Vec3 rightDir{};
    rightDir.x = cosf(cameraYawRad); // 카메라를 정면 중앙보다 약간 옆으로 빼기 위한 right 방향
    rightDir.y = 0.f;
    rightDir.z = -sinf(cameraYawRad);
    rightDir.Normalize();

    const Vec3 lookAt = heroCenter + Vec3(0.f, 2.2f, 0.f);

    const Vec3 cameraPos =
        heroCenter +
        backDir * 8.5f +          
        rightDir * 2.0f +         
        Vec3(2.f, 3.f, -2.f);     // 테스트용 임시 위치 보정

    Apply_LookAt(cameraPos, lookAt);

    _smoothedTargetPos = cameraPos;
}

void BattleTurnCamera::Update_FreeAimRotation(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    CHECK_NULL(_transformCom);

    const Vec2 mouseDelta = GAME->Get_MouseDelta();

    _yaw += mouseDelta.x * _mouseSensor;
    _pitch += mouseDelta.y * _mouseSensor;
    _pitch = std::clamp(_pitch, -25.f, 20.f); // 상하 각도 제한

    _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));
    Update_TransformMatrices();
}

Vec3 BattleTurnCamera::Get_AimOrigin() const
{
    CHECK_NULL(_transformCom, Vec3::Zero);

    return _transformCom->Get_WorldPosition();
}

Vec3 BattleTurnCamera::Get_AimDirection() const
{
    CHECK_NULL(_transformCom, Vec3::Zero);

    Vec3 lookDir = _transformCom->Get_WorldForward();
    if (lookDir.LengthSquared() <= 0.0001f)
        return Vec3::Look;

    lookDir.Normalize();
    return lookDir;
}

void BattleTurnCamera::Apply_FreeAimCamera()
{
}

void BattleTurnCamera::Set_Participants(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies)
{
    _heroes = heroes;
    _enemies = enemies;
}

void BattleTurnCamera::Apply_LookAt(const Vec3& cameraPos, const Vec3& lookAt)
{
    _transformCom->Set_WorldPosition(cameraPos);
    _transformCom->LookAt(lookAt);

    Update_TransformMatrices();
}

void BattleTurnCamera::Apply_ShotRequest(const BattleShotRequest& request)
{
    if (_currentRequest.mode == request.mode &&
        _currentRequest.subject == request.subject)
        return;

    bool wasWide = _currentRequest.mode == BattleCameraMode::WIDE;
    bool willWide = request.mode == BattleCameraMode::WIDE;

    if(wasWide && !willWide)
        _isWideMode = false;

    _activeClip = nullptr; 
    _clipTime = 0.f;
    _isClipPreview = false;
    _isFirstUpdate = true;
    _currentRequest = request;
    _targetTransform = nullptr;

    if (_currentRequest.subject)
        _targetTransform = _currentRequest.subject->Get_Transform();

    switch(_currentRequest.mode)
    {
    case BattleCameraMode::WIDE:
        _targetPreset = _wideModePreset;
        break;

    case BattleCameraMode::PLAYERFOCUS: 
        _targetPreset = _playerFocusPreset;
        break;

    case BattleCameraMode::ENEMYFOCUS:
        _targetPreset = _enemyFocusPreset;
        break;

    case BattleCameraMode::ENEMYATTACK:
        _targetPreset = _enemyAttackPreset;
        _activeClip = &_enemyAttackClip;
        break;

    case BattleCameraMode::PLAYERATTACK:
        _targetPreset = _playerAttackPreset;
        _activeClip = &_playerAttackClip;
        break;
    }

    _currentPreset = _targetPreset;

    //playerfocus, playerAttack 이었을 때 snapOnEnter가 false여서 그 마지막 애니메이션의 target Pos가 되어있을텐데 지금은 enemyfocus가 마지막으로 저장된 것이므로 거기서 확대되는식으로 이루어짐
    //각 애니메이션이 종료하고 나서의 _smoothedTargetPos는 무조건 그 애니메이션 종료 시점에 한번씩은 저장할 수 있도록 해야할텐데
    if (_currentPreset.snapOnEnter && _targetTransform)
    {
        const Vec3 targetPos = _targetTransform->Get_WorldPosition();
        _smoothedTargetPos = targetPos;
        _prevTargetPos = targetPos;
    }
}

void BattleTurnCamera::Begin_LiveEdit(BattleCameraMode mode, const Shared<Character>& subject, const Shared<Character>& opponent)
{
    _isLiveEditEnabled = true;
    Update_LiveEditContext(mode, subject, opponent);
}

void BattleTurnCamera::Update_LiveEditContext(BattleCameraMode mode, const Shared<Character>& target, const Shared<Character>& opponent)
{
    BattleShotRequest request{};
    request.mode = mode;
    request.subject = target;
    request.opponent = opponent;
    Apply_ShotRequest(request);

    if (BattleCameraPreset* preset = Get_MutablePreset(mode))
    {
        _currentPreset = *preset;
        _targetPreset = *preset;
    }
}

void BattleTurnCamera::End_LiveEdit()
{
    _isLiveEditEnabled = false;
}

void BattleTurnCamera::Update_WideMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_PlayerFocusMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_EnemyFocusMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_EnemyAttackMode(float timeDelta)
{
    // clip이 있을 시 적용
    const BattleCameraClipSample clip  = Advance_ActiveClip(timeDelta);

    float finalFovY = _currentPreset.camera.fovy + clip.fovOffset;
    float finalDistance = _currentPreset.distance + clip.distanceOffset;

    if(_isFirstUpdate)
    {
        _enemyPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
        _playerPos = _currentRequest.opponent->Get_Transform()->Get_WorldPosition();

        _isFirstUpdate = false;
    }

    Vec3 forward = _playerPos - _enemyPos;

    forward.y = 0;
    forward.Normalize();

    Vec3 right;

    right.x = forward.z;
    right.y = 0;
    right.z  = -forward.x;

    Vec3 backDir = - forward;

    const float yawRad = XMConvertToRadians(_currentPreset.yawOffset);
    Vec3 yawAdjustedBack = backDir * cosf(yawRad) + right * sinf(yawRad);

    Vec3 camPos = _playerPos + yawAdjustedBack * finalDistance + right * _currentPreset.sideOffset + Vec3::Up * _currentPreset.heightOffset;

    Vec3 lookTarget = _enemyPos + forward * _currentPreset.aimOffset + Vec3::Up * _currentPreset.lookHeight;

    finalFovY = clamp(finalFovY, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    Set_FovY(finalFovY);
    Apply_LookAt(camPos,lookTarget);
    Update_TransformMatrices();
}

void BattleTurnCamera::Update_PlayerAttackMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_FromCurrentPreset(float timeDelta)
{
    if(_currentRequest.subject == nullptr)
        return;

    // Clip이 있을 시 적용
    const BattleCameraClipSample clip  = Advance_ActiveClip(timeDelta);

    float finalFovY = _currentPreset.camera.fovy + clip.fovOffset;
    float finalDistance = _currentPreset.distance + clip.distanceOffset;

    const Vec3 targetPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();

    const float finalYaw = _yaw + _currentPreset.yawOffset;
    const float finalPitch = _pitch + _currentPreset.pitchOffset;

    const float yawRad = XMConvertToRadians(finalYaw);
    const float pitchRad = XMConvertToRadians(finalPitch);

    Vec3 backDir;
    backDir.x = -sinf(yawRad);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRad);
    backDir.Normalize();

    Vec3 rightDir;
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * finalDistance;

    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    _smoothedTargetPos = Vec3::Lerp(_smoothedTargetPos, targetPos, timeDelta * _currentPreset.followSpeed);

    const Vec3 pivotPos = _smoothedTargetPos + Vec3::Up * _currentPreset.pivotHeight;
    Vec3 framingPos = pivotPos  + rightDir * _currentPreset.aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * _currentPreset.heightOffset + rightDir * _currentPreset.sideOffset;
    Vec3 lookTarget = framingPos + Vec3::Up * _currentPreset.lookHeight;

    Vec3 newPos = desiredPos;

    finalFovY = clamp(finalFovY, XMConvertToRadians(10.f), XMConvertToRadians(120.f));

    Set_FovY(finalFovY);

    Apply_LookAt(newPos, lookTarget);
    _transformCom->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, _currentPreset.rollOffset));
    
    Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _prevTargetPos = targetPos;

}

void BattleTurnCamera::Update_LiveEditInput(float timeDelta)
{
    if (!GAME->KeyPress(KEY_TYPE::RBUTTON))
        return;

    BattleCameraPreset* preset = Get_MutablePreset(_currentRequest.mode);
    if (preset == nullptr)
        return;

    const float wheelDelta = GAME->Get_MouseWheel();
    if (wheelDelta != 0.f)
    {
        const float wheelStep = 0.4f;
        preset->distance += (wheelDelta > 0.f) ? -wheelStep : wheelStep;
    }

    const float moveStep = 3.f * timeDelta;

    if (GAME->KeyPress(KEY_TYPE::W))
        preset->distance -= moveStep;

    if (GAME->KeyPress(KEY_TYPE::S))
        preset->distance += moveStep;

    if (GAME->KeyPress(KEY_TYPE::A))
        preset->aimOffset -= moveStep;

    if (GAME->KeyPress(KEY_TYPE::D))
        preset->aimOffset += moveStep;

    if (GAME->KeyPress(KEY_TYPE::E))
        preset->pivotHeight += moveStep;

    if (GAME->KeyPress(KEY_TYPE::Q))
        preset->pivotHeight -= moveStep;

    preset->distance = std::max(0.1f, preset->distance);

    const Vec2 mouseDelta = GAME->Get_MouseDelta();
    preset->yawOffset += mouseDelta.x * preset->camera.mouseSensor;
    preset->pitchOffset += mouseDelta.y * preset->camera.mouseSensor * preset->camera.mouseSensitivity;
    preset->pitchOffset = std::clamp(preset->pitchOffset, preset->pitchMin, preset->pitchMax);

    _currentPreset = *preset;
    _targetPreset = *preset;
}

const BattleCameraPreset& BattleTurnCamera::Get_Preset(BattleCameraMode mode) const
{
    switch (mode)
    {
    case BattleCameraMode::WIDE:
        return _wideModePreset;

    case BattleCameraMode::PLAYERFOCUS:
        return _playerFocusPreset;

    case BattleCameraMode::ENEMYFOCUS:
        return _enemyFocusPreset;

    case BattleCameraMode::ENEMYATTACK:
        return _enemyAttackPreset;

    case BattleCameraMode::PLAYERATTACK:
        return _playerAttackPreset;

    default:
        return _wideModePreset;
    }
}

void BattleTurnCamera::Set_Preset(BattleCameraMode mode, const BattleCameraPreset& preset)
{
    switch (mode)
    {
    case BattleCameraMode::WIDE:
        _wideModePreset = preset;
        break;

    case BattleCameraMode::PLAYERFOCUS:
        _playerFocusPreset = preset;
        break;

    case BattleCameraMode::ENEMYFOCUS:
        _enemyFocusPreset = preset;
        break;

    case BattleCameraMode::ENEMYATTACK:
        _enemyAttackPreset = preset;
        break;

    case BattleCameraMode::PLAYERATTACK:
        _playerAttackPreset = preset;
        break;

    default:
        break;
    }
}

void BattleTurnCamera::Set_Clip(BattleCameraMode mode, const BattleCameraClip& clip)
{
    if (BattleCameraClip* targetClip = Get_MutableClip(mode))
        *targetClip = clip;
}

void BattleTurnCamera::Apply_PreviewPreset(const BattleCameraPreset& preset)
{
    _currentPreset = preset;
    _targetPreset = preset;
}

void BattleTurnCamera::Preview_ClipAtTime(const BattleShotRequest& request, float time)
{
    if (_currentRequest.mode != request.mode || _currentRequest.subject != request.subject || _currentRequest.opponent != request.opponent)
        Apply_ShotRequest(request);
    else
    {
        _currentRequest = request;
        _targetTransform = _currentRequest.subject ? _currentRequest.subject->Get_Transform() : nullptr;
    }

    switch (_currentRequest.mode)
    {
    case BattleCameraMode::ENEMYATTACK:
        Preview_EnemyAttackAtTime(time);
        break;

    default:
        Preview_CurrentPresetAtTime(time);
        break;
    }
}

BattleCameraPreset* BattleTurnCamera::Get_MutablePreset(BattleCameraMode cameraMode)
{
    switch (cameraMode)
    {
    case BattleCameraMode::WIDE:
        return &_wideModePreset;

    case BattleCameraMode::PLAYERFOCUS:
        return &_playerFocusPreset;

    case BattleCameraMode::ENEMYFOCUS:
        return &_enemyFocusPreset;

    case BattleCameraMode::ENEMYATTACK:
        return &_enemyAttackPreset;

    case BattleCameraMode::PLAYERATTACK:
        return &_playerAttackPreset;

    default:
        return nullptr;
    }
}

BattleCameraClip* BattleTurnCamera::Get_MutableClip(BattleCameraMode cameraMode)
{
    switch (cameraMode)
    {
    case BattleCameraMode::WIDE:
        return &_wideModeClip;

    case BattleCameraMode::PLAYERFOCUS:
        return &_playerFocusClip;

    case BattleCameraMode::ENEMYFOCUS:
        return &_enemyFocusClip;

    case BattleCameraMode::ENEMYATTACK:
        return &_enemyAttackClip;

    case BattleCameraMode::PLAYERATTACK:
        return &_playerAttackClip;

    default:
        return nullptr;
    }
}

void BattleTurnCamera::Preview_CurrentPresetAtTime(float time)
{
    if (_currentRequest.subject == nullptr)
        return;

    BattleCameraClip* clip = Get_MutableClip(_currentRequest.mode);
    const BattleCameraClipSample clipSample = clip ? Sample_Clip(*clip, clamp(time, 0.f, clip->duration)) : BattleCameraClipSample {};

    const float finalFovY = clamp(_currentPreset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = _currentPreset.distance + clipSample.distanceOffset;
    const Vec3 targetPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();

    const float finalYaw = _yaw + _currentPreset.yawOffset;
    const float finalPitch = _pitch + _currentPreset.pitchOffset;

    const float yawRad = XMConvertToRadians(finalYaw);
    const float pitchRad = XMConvertToRadians(finalPitch);

    Vec3 backDir;
    backDir.x = -sinf(yawRad);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRad);
    backDir.Normalize();

    Vec3 rightDir;
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * finalDistance;
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);
    const Vec3 pivotPos = targetPos + Vec3::Up * _currentPreset.pivotHeight;
    const Vec3 framingPos = pivotPos + rightDir * _currentPreset.aimOffset;
    const Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * _currentPreset.heightOffset + rightDir * _currentPreset.sideOffset;
    const Vec3 lookTarget = framingPos + Vec3::Up * _currentPreset.lookHeight;

    Set_FovY(finalFovY);
    Apply_LookAt(desiredPos, lookTarget);
    _transformCom->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, _currentPreset.rollOffset));
    Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
}

void BattleTurnCamera::Preview_EnemyAttackAtTime(float time)
{
    if (_currentRequest.subject == nullptr || _currentRequest.opponent == nullptr)
        return;

    BattleCameraClip* clip = Get_MutableClip(_currentRequest.mode);
    const BattleCameraClipSample clipSample = clip ? Sample_Clip(*clip, clamp(time, 0.f, clip->duration)) : BattleCameraClipSample {};

    const float finalFovY = clamp(_currentPreset.camera.fovy + clipSample.fovOffset, XMConvertToRadians(10.f), XMConvertToRadians(120.f));
    const float finalDistance = _currentPreset.distance + clipSample.distanceOffset;

    const Vec3 enemyPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
    const Vec3 playerPos = _currentRequest.opponent->Get_Transform()->Get_WorldPosition();

    Vec3 forward = playerPos - enemyPos;
    forward.y = 0.f;
    forward.Normalize();

    Vec3 right;
    right.x = forward.z;
    right.y = 0.f;
    right.z = -forward.x;

    const Vec3 backDir = -forward;
    const float yawRad = XMConvertToRadians(_currentPreset.yawOffset);
    const Vec3 yawAdjustedBack = backDir * cosf(yawRad) + right * sinf(yawRad);

    const Vec3 camPos = playerPos + yawAdjustedBack * finalDistance + right * _currentPreset.sideOffset + Vec3::Up * _currentPreset.heightOffset;
    const Vec3 lookTarget = enemyPos + forward * _currentPreset.aimOffset + Vec3::Up * _currentPreset.lookHeight;

    Set_FovY(finalFovY);
    Apply_LookAt(camPos, lookTarget);
    Update_TransformMatrices();
}

float BattleTurnCamera::Sample_Track(const BattleCameraTrack& battleCameraTrack, float time)
{
   if(battleCameraTrack.keyframes.empty())
       return 0.f;

   if(time <= battleCameraTrack.keyframes.front().time)
   {
       return battleCameraTrack.keyframes.front().value;
   }

   if(time >= battleCameraTrack.keyframes.back().time)
   {
       return battleCameraTrack.keyframes.back().value;
   }

    for(auto i = 0; i + 1 < battleCameraTrack.keyframes.size() ; i++)
    {
        const auto& a = battleCameraTrack.keyframes[i];
        const auto& b = battleCameraTrack.keyframes[i + 1];

        if(time >= a.time && time <= b.time)
        {
            float t = (time - a.time) / (b.time - a.time);
            return lerp(a.value, b.value, t);
        }
    }
    return battleCameraTrack.keyframes.back().value;
}

BattleCameraClipSample BattleTurnCamera::Sample_Clip(const BattleCameraClip& battleCameraClip, float time)
{
    BattleCameraClipSample sample;

    sample.distanceOffset = Sample_Track(battleCameraClip.distanceOffset, time);
    sample.fovOffset = Sample_Track(battleCameraClip.fovOffset, time);
    sample.shakeAmplitude = Sample_Track(battleCameraClip.shakeAmplitude, time);

    return sample;
}

BattleCameraClipSample BattleTurnCamera::Advance_ActiveClip(float timeDelta)
{
    BattleCameraClipSample sample{};

    if(_activeClip == nullptr)
        return sample;

    _clipTime += timeDelta;

    float sampleTime = clamp(_clipTime, 0.f, _activeClip->duration);

    sample = Sample_Clip(*_activeClip, sampleTime);

    if(_clipTime >= _activeClip->duration)
    {
        _clipTime = _activeClip->duration;
        _activeClip = nullptr;
    }

    return sample;
}

Shared<BattleTurnCamera> BattleTurnCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleTurnCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> BattleTurnCamera::Clone(void* arg)
{
    auto instance = make_shared<BattleTurnCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void BattleTurnCamera::Free()
{
    if (_battlePresentationHandle.IsValid())
    {
        GAME->Get_EventBus().Unsubscribe(_battlePresentationHandle);
        _battlePresentationHandle = {};
    }

    Camera::Free();
}
