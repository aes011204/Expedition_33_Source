#include "BattleTurnCamera.h"
#include <CameraPresetIO.h>
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

    GAME->Get_EventBus().Subscribe<EvtBattlePresentation>
    ([this](const EvtBattlePresentation& event)
        {
            BattleShotRequest request;

            switch(event.cameraMode)
            {
                case BattleCameraMode::ENEMYFOCUS:
                    request.mode = event.cameraMode;
                    request.target = event.target;
                    break;

                case BattleCameraMode::PLAYERFOCUS:
                    request.mode = event.cameraMode;
                    request.target = event.target;
                    break;

                case BattleCameraMode::WIDE:
                    request.mode = event.cameraMode;
                    request.target = event.target;
                    break;

            case BattleCameraMode::ENEMYATTACK:
                    request.mode = event.cameraMode;
                    request.target = event.target;
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

    if (_isLiveEditEnabled)
        Update_LiveEditInput(timeDelta);

    switch (_currentRequest.mode)
    {
    case BattleCameraMode::WIDE:
        Update_WideMode(timeDelta);
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

void BattleTurnCamera::Apply_LookAt(const Vec3& cameraPos, const Vec3& lookAt)
{
    _transformCom->Set_WorldPosition(cameraPos);
    _transformCom->LookAt(lookAt);

    Update_TransformMatrices();
}

void BattleTurnCamera::Apply_ShotRequest(const BattleShotRequest& request)
{
    if (_currentRequest.mode == request.mode &&
        _currentRequest.target == request.target)
        return;

    _currentRequest = request;

    _targetTransform = nullptr;
    if (_currentRequest.target)
        _targetTransform = _currentRequest.target->Get_Transform();

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
        break;
    }

    _currentPreset = _targetPreset;
}

void BattleTurnCamera::Begin_LiveEdit(BattleCameraMode mode, const Shared<Character>& target)
{
    _isLiveEditEnabled = true;
    Update_LiveEditContext(mode, target);
}

void BattleTurnCamera::Update_LiveEditContext(BattleCameraMode mode, const Shared<Character>& target)
{
    BattleShotRequest request{};
    request.mode = mode;
    request.target = target;
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
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_FromCurrentPreset(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if(_currentRequest.target == nullptr)
        return;

    const Vec3 targetPos = _currentRequest.target->Get_Transform()->Get_WorldPosition();

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

    const Vec3 baseOffset = backDir * _currentPreset.distance;

    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    _smoothedTargetPos = Vec3::Lerp(_smoothedTargetPos, targetPos, timeDelta * _currentPreset.followSpeed);

    const Vec3 pivotPos = _smoothedTargetPos + Vec3::Up * _currentPreset.pivotHeight;
    Vec3 framingPos = pivotPos  + rightDir * _currentPreset.aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * _currentPreset.heightOffset + rightDir * _currentPreset.sideOffset;
    Vec3 lookTarget = framingPos + Vec3::Up * _currentPreset.lookHeight;

    Vec3 newPos = desiredPos;

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

    default:
        break;
    }
}

void BattleTurnCamera::Apply_PreviewPreset(const BattleCameraPreset& preset)
{
    _currentPreset = preset;
    _targetPreset = preset;
}

BattleCameraPreset* BattleTurnCamera::Get_MutablePreset(BattleCameraMode mode)
{
    switch (mode)
    {
    case BattleCameraMode::WIDE:
        return &_wideModePreset;

    case BattleCameraMode::PLAYERFOCUS:
        return &_playerFocusPreset;

    case BattleCameraMode::ENEMYFOCUS:
        return &_enemyFocusPreset;

    case BattleCameraMode::ENEMYATTACK:
        return &_enemyAttackPreset;

    default:
        return nullptr;
    }
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
    Camera::Free();
}
