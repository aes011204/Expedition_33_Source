#include "DialogueCamera.h"

#include "GameInstance.h"

IMPLEMENT_REFLECTION(DialogueCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("cameraPresetId", _cameraPresetId);
    PROPERTY_FLOAT("distance", _distance);
    PROPERTY_FLOAT("sideOffset", _sideOffset);
    PROPERTY_FLOAT("heightOffset", _heightOffset);
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

    _distance = desc->distance;
    _sideOffset = desc->sideOffset;
    _heightOffset = desc->heightOffset;
    _blendSpeed = desc->blendSpeed;

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

    _transformCom->Set_WorldPosition(nextEye);
    _transformCom->LookAt(desiredLookAt);
    Apply_View();
}

void DialogueCamera::Clear_Targets()
{
    _speakerTransformCom.reset();
    _listenerTransformCom.reset();
    _cameraPresetId.clear();
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
    _transformCom->Set_WorldPosition(eye);
    _transformCom->LookAt(lookAt);
    Apply_View();
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
    if (dialogueDir.LengthSquared() <= 0.0001f)
        dialogueDir = Vec3::UnitZ;
    dialogueDir.Normalize();

    Vec3 rightDir = Vec3::Up.Cross(dialogueDir);
    if (rightDir.LengthSquared() <= 0.0001f)
        rightDir = Vec3::UnitX;
    rightDir.Normalize();

    outLookAt = Vec3::Lerp(speakerPos, listenerPos, 0.5f) + Vec3::Up * _heightOffset;
    outEye = outLookAt - dialogueDir * _distance + rightDir * _sideOffset;
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
