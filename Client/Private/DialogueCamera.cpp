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
    preset.distance = distance;
    preset.sideOffset = sideOffset;
    preset.heightOffset = heightOffset;
    preset.aimOffset = 0.f;
    preset.pitchLookScale = 0.f;
    preset.pitchMin = -45.f;
    preset.pitchMax = 45.f;
    preset.yawOffset = 180.f;
    preset.pitchOffset = 6.f;
    preset.rollOffset = 0.f;
    preset.lookHeight = 0.85f;
    preset.pivotHeight = 0.75f;
    return preset;
}
}

IMPLEMENT_REFLECTION(DialogueCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("cameraPresetId", _cameraPresetId);
    PROPERTY_FLOAT("distance", _currentPreset.distance);
    PROPERTY_FLOAT("sideOffset", _currentPreset.sideOffset);
    PROPERTY_FLOAT("heightOffset", _currentPreset.heightOffset);
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
    GAME->Add_Camera(GetSharedPtr<Camera>());
}

void DialogueCamera::OnActivated()
{
    Set_FovY(_currentPreset.camera.fovy);
    Set_Near(_currentPreset.camera.nearPlane);
    Set_Far(_currentPreset.camera.farPlane);
    Snap_ToDialogueTargets();
    Apply_View();
}

void DialogueCamera::Update(float timeDelta)
{
    Vec3 desiredEye = Vec3::Zero;
    Vec3 desiredLookAt = Vec3::Zero;
    if (!Try_ResolveDialoguePose(desiredEye, desiredLookAt))
        return;

    const Vec3 currentEye = _transformCom->Get_WorldPosition();
    const float t = min(1.f, timeDelta * _blendSpeed);
    const Vec3 nextEye = Vec3::Lerp(currentEye, desiredEye, t);

    Set_FovY(_currentPreset.camera.fovy);
    Set_Near(_currentPreset.camera.nearPlane);
    Set_Far(_currentPreset.camera.farPlane);
    _transformCom->Set_WorldPosition(nextEye);
    _transformCom->LookAt(desiredLookAt);
    if (_currentPreset.rollOffset != 0.f)
        _transformCom->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, _currentPreset.rollOffset));
    Apply_View();
}

void DialogueCamera::Clear_Targets()
{
    _speakerTransformCom.reset();
    _listenerTransformCom.reset();
    _cameraPresetId.clear();
}

void DialogueCamera::Set_CameraPresetId(const string& cameraPresetId)
{
    _cameraPresetId = cameraPresetId;
    Load_PresetFromId();
}

void DialogueCamera::Snap_ToDialogueTargets()
{
    Vec3 desiredEye = Vec3::Zero;
    Vec3 desiredLookAt = Vec3::Zero;
    if (!Try_ResolveDialoguePose(desiredEye, desiredLookAt))
        return;

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
    const Shared<TransformCom> speakerTransform = _speakerTransformCom.lock();
    const Shared<TransformCom> listenerTransform = _listenerTransformCom.lock();
    if (speakerTransform == nullptr || listenerTransform == nullptr)
        return false;

    const Vec3 speakerPos = speakerTransform->Get_WorldPosition();
    const Vec3 listenerPos = listenerTransform->Get_WorldPosition();

    Vec3 dialogueDir = listenerPos - speakerPos;
    dialogueDir.y = 0.f;
    if (dialogueDir.LengthSquared() <= 0.0001f)
        return false;
    dialogueDir.Normalize();

    Vec3 rightDir = Vec3::Up.Cross(dialogueDir);
    if (rightDir.LengthSquared() <= 0.0001f)
        rightDir = Vec3::Right;
    else
        rightDir.Normalize();

    Vec3 backDir = -dialogueDir;
    const float yawRad = XMConvertToRadians(_currentPreset.yawOffset);
    Vec3 yawAdjustedBack = backDir * cosf(yawRad) + rightDir * sinf(yawRad);
    if (yawAdjustedBack.LengthSquared() <= 0.0001f)
        yawAdjustedBack = backDir;
    else
        yawAdjustedBack.Normalize();

    const Vec3 anchorPos = Vec3::Lerp(speakerPos, listenerPos, 0.5f);
    const Vec3 pivotPos = anchorPos + Vec3::Up * _currentPreset.pivotHeight;
    const Vec3 framingPos = pivotPos + rightDir * _currentPreset.aimOffset;

    const Vec3 baseOffset = yawAdjustedBack * _currentPreset.distance;
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, XMConvertToRadians(_currentPreset.pitchOffset));
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    outLookAt = framingPos + Vec3::Up * _currentPreset.lookHeight;
    outEye = framingPos + orbitOffset + Vec3::Up * _currentPreset.heightOffset + rightDir * _currentPreset.sideOffset;
    return true;
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
