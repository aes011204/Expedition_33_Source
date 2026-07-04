#include "DialogueCamera.h"

#include "CameraPresetIO.h"
#include "GameInstance.h"

namespace
{
DialogueCameraPreset Make_DefaultDialogueCameraPreset(float distance, float sideOffset, float heightOffset)
{
    DialogueCameraPreset preset{};
    preset.camera.fovy = XMConvertToRadians(45.f);
    preset.camera.nearPlane = 0.1f;
    preset.camera.farPlane = 500.f;
    preset.camera.mouseSensor = 0.29f;
    preset.camera.mouseSensitivity = 0.23f;
    preset.localCameraOffset = Vec3(sideOffset, heightOffset, distance * 0.5f);
    preset.localLookTargetOffset = Vec3(0.f, 0.85f, 0.f);
    preset.rollOffset = 0.f;
    return preset;
}
}

IMPLEMENT_REFLECTION(DialogueCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("cameraPresetId", _cameraPresetId);
    PROPERTY_FLOAT("localCameraOffsetX", _currentPreset.localCameraOffset.x);
    PROPERTY_FLOAT("localCameraOffsetY", _currentPreset.localCameraOffset.y);
    PROPERTY_FLOAT("localCameraOffsetZ", _currentPreset.localCameraOffset.z);
    PROPERTY_FLOAT("localLookTargetOffsetX", _currentPreset.localLookTargetOffset.x);
    PROPERTY_FLOAT("localLookTargetOffsetY", _currentPreset.localLookTargetOffset.y);
    PROPERTY_FLOAT("localLookTargetOffsetZ", _currentPreset.localLookTargetOffset.z);
    PROPERTY_FLOAT("blendSpeed", _blendSpeed);

    return true;
}

DialogueCamera::DialogueCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

DialogueCamera::DialogueCamera(const DialogueCamera& prototype)
    : Camera{ prototype }
{
}

HRESULT DialogueCamera::Initialize_Prototype()
{
    if (FAILED(__super::Initialize_Prototype()))
        return E_FAIL;

    return S_OK;
}

HRESULT DialogueCamera::Initialize(void* arg)
{
    if (arg == nullptr)
        return E_FAIL;

    const auto* desc = static_cast<DIALOGUECAMERA_DESC*>(arg);

    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    _currentPreset = Make_DefaultDialogueCameraPreset(desc->distance, desc->sideOffset, desc->heightOffset);
    _blendSpeed = desc->blendSpeed;
    Set_FovY(_currentPreset.camera.fovy);
    Set_Near(_currentPreset.camera.nearPlane);
    Set_Far(_currentPreset.camera.farPlane);

    Set_Name(L"Dialogue Camera");
    Update_TransformMatrices();

    return S_OK;
}

void DialogueCamera::BeginPlay()
{
    Camera::BeginPlay();
}

void DialogueCamera::OnActivated()
{
    _driftTimeSec = 0.f;
    Set_FovY(_currentPreset.camera.fovy);
    Set_Near(_currentPreset.camera.nearPlane);
    Set_Far(_currentPreset.camera.farPlane);
    Snap_ToDialogueTargets();
    Apply_View();
}

void DialogueCamera::Update(float timeDelta)
{
    if (!GAME->Is_ActiveCameraType(ObjectType::DialogueCamera))
        return;

    Vec3 desiredEye = Vec3::Zero;
    Vec3 desiredLookAt = Vec3::Zero;
    if (!Try_ResolveDialoguePose(desiredEye, desiredLookAt))
        return;

    _driftTimeSec += std::max(0.f, timeDelta);

    float finalRollOffset = _currentPreset.rollOffset;
    Apply_DriftToPose(_driftTimeSec, desiredEye, desiredLookAt, finalRollOffset);

    const Vec3 currentEye = _transformCom->Get_WorldPosition();
    const float t = min(1.f, timeDelta * _blendSpeed);
    const Vec3 nextEye = Vec3::Lerp(currentEye, desiredEye, t);

    Set_FovY(_currentPreset.camera.fovy);
    Set_Near(_currentPreset.camera.nearPlane);
    Set_Far(_currentPreset.camera.farPlane);
    _transformCom->Set_WorldPosition(nextEye);
    _transformCom->LookAt(desiredLookAt);
    if (finalRollOffset != 0.f)
        _transformCom->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, finalRollOffset));
    Apply_View();
}

void DialogueCamera::Clear_Targets()
{
    _anchorTransformCom.reset();
    _speakerTransformCom.reset();
    _listenerTransformCom.reset();
    _cameraPresetId.clear();
}

void DialogueCamera::Set_CameraPresetId(const string& cameraPresetId)
{
    if (_cameraPresetId == cameraPresetId)
        return;

    _cameraPresetId = cameraPresetId;
    Load_PresetFromId();
}

void DialogueCamera::Snap_ToDialogueTargets()
{
    Vec3 desiredEye = Vec3::Zero;
    Vec3 desiredLookAt = Vec3::Zero;
    if (!Try_ResolveDialoguePose(desiredEye, desiredLookAt))
        return;

    float finalRollOffset = _currentPreset.rollOffset;
    Apply_DriftToPose(_driftTimeSec, desiredEye, desiredLookAt, finalRollOffset);
    Apply_ImmediatePose(desiredEye, desiredLookAt);
}

void DialogueCamera::Apply_ImmediatePose(const Vec3& eye, const Vec3& lookAt)
{
    Set_FovY(_currentPreset.camera.fovy);
    Set_Near(_currentPreset.camera.nearPlane);
    Set_Far(_currentPreset.camera.farPlane);
    _transformCom->Set_WorldPosition(eye);
    _transformCom->LookAt(lookAt);
    if (_currentPreset.rollOffset != 0.f)
        _transformCom->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, _currentPreset.rollOffset));
    Apply_View();
}

void DialogueCamera::Load_PresetFromId()
{
    if (_cameraPresetId.empty())
        return;

    DialogueCameraPreset preset = _currentPreset;
    Client::CameraPresetIO::LoadDialoguePresetFromFile(_cameraPresetId, preset);
    _currentPreset = preset;
}

bool DialogueCamera::Try_ResolveDialoguePose(Vec3& outEye, Vec3& outLookAt) const
{
    Shared<TransformCom> anchorTransform = _anchorTransformCom.lock();
    if (anchorTransform == nullptr)
        anchorTransform = _speakerTransformCom.lock();

    if (anchorTransform == nullptr)
        return false;

    const Vec3 anchorPos = anchorTransform->Get_WorldPosition();
    Vec3 anchorForward = anchorTransform->Get_WorldForward();
    return Evaluate_DialoguePresetPoseNpcAnchor(
        anchorPos,
        anchorForward,
        _currentPreset,
        outEye,
        outLookAt);
}

void DialogueCamera::Apply_DriftToPose(float timeSec, Vec3& inOutEye, Vec3& inOutLookAt, float& inOutRollOffset) const
{
    if (!_currentPreset.drift.enabled)
        return;

    Vec3 forward = inOutLookAt - inOutEye;
    if (forward.LengthSquared() <= 0.0001f)
        return;

    forward.Normalize();

    Vec3 right = Vec3::Up.Cross(forward);
    if (right.LengthSquared() <= 0.0001f)
        return;

    right.Normalize();

    Vec3 up = forward.Cross(right);
    up.Normalize();

    const float x = sinf(timeSec * _currentPreset.drift.lateralFrequency) * _currentPreset.drift.lateralAmplitude;
    const float y = sinf(timeSec * _currentPreset.drift.verticalFrequency + 1.37f) * _currentPreset.drift.verticalAmplitude;
    const float z = sinf(timeSec * _currentPreset.drift.depthFrequency + 2.11f) * _currentPreset.drift.depthAmplitude;
    const float roll = sinf(timeSec * _currentPreset.drift.rollFrequency + 0.73f) * _currentPreset.drift.rollAmplitude;

    const Vec3 driftOffset = right * x + up * y + forward * z;
    inOutEye += driftOffset;
    inOutLookAt += driftOffset * 0.6f;
    inOutRollOffset += roll;
}

Shared<DialogueCamera> DialogueCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<DialogueCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> DialogueCamera::Clone(void* arg)
{
    auto instance = make_shared<DialogueCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void DialogueCamera::Free()
{
    Camera::Free();
}
