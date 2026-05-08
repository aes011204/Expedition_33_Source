#include "TargetCamera.h"
#include "ClientInstance.h"
#include "GameInstance.h"

TargetCamera::TargetCamera(const ComPtr<Device>& device, const ComPtr<Context>& context) :  Camera {device, context}
{
}

TargetCamera::TargetCamera(const TargetCamera& prototype) : Camera {prototype}
{
}

HRESULT TargetCamera::Initialize_Prototype()
{
    if(FAILED(__super::Initialize_Prototype()))
        return E_FAIL;

    return S_OK;
}

HRESULT TargetCamera::Initialize(void* arg)
{
    if (nullptr == arg)
        return E_FAIL;

    const auto desc = static_cast<TARGETCAMERA_DESC*>(arg);

    _followSpeed = desc->followSpeed;
    _offset = desc->offset;
    _mouseSensor = desc->mouseSensor;
    _distance = desc->distance;
    _sideOffset = desc->sideOffset;
    _heightOffset = desc->heightOffset;
    _aimOffset = desc->aimOffset;
    _pitchLookScale = desc->pitchLookScale;
    _pitchMin = desc->pitchMin;
    _pitchMax = desc->pitchMax;
    _lookHeight = desc->lookHeight;
    _pivotHeight = desc->pivotHeight;

    if(FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Update_TransformMatrices();

    Set_Name(L"Target Camera");

    return S_OK;
}

void TargetCamera::BeginPlay()
{
    Camera::BeginPlay();

    GAME->Add_Camera(GetSharedPtr<Camera>());
    GAME->Change_ActiveCamera(GetSharedPtr<Camera>());
    _targetTransformCom =  CLIENT->Find_Player(ETOI(LevelType::Static))->Get_Transform();
    _prevTargetPos = _targetTransformCom.lock()->Get_WorldPosition();
}

void TargetCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if(!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    if(_targetTransformCom.expired())
    {
        MSG_BOX("Target Transform == nullptr");
        return;
    }

   auto targetTransformCom  = _targetTransformCom.lock();

    if(nullptr == targetTransformCom)
        return;

    const Vec2 mouseDelta = GAME->Get_MouseDelta();

    _yaw += mouseDelta.x * _mouseSensor;
    _pitch += mouseDelta.y * _mouseSensor;
    _pitch = std::clamp(_pitch, _pitchMin, _pitchMax);


    const Vec3 targetPos = targetTransformCom->Get_WorldPosition();
    const Vec3 pivotPos = targetPos + Vec3::Up * _pivotHeight;

    const float yawRad = XMConvertToRadians(_yaw);
    const float pitchRad = XMConvertToRadians(_pitch);

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

    const Vec3 baseOffset =
        backDir * _distance;

    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    Vec3 framingPos = pivotPos  + rightDir * _aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * _heightOffset + rightDir * _sideOffset;
    Vec3 lookTarget = framingPos + + Vec3::Up *_lookHeight;

    const Vec3 currentPos = _transformCom->Get_WorldPosition();

    Vec3 newPos;

    const bool isRotating = mouseDelta.LengthSquared() > 0.0001f;
    const bool isTargetMoving = (targetPos - _prevTargetPos).LengthSquared() > 0.0001f;

    if (isRotating && !isTargetMoving)
    {
        newPos = desiredPos;
    }
    else
    {
        newPos = Math::Lerp_Damp(currentPos, desiredPos, _followSpeed, timeDelta);
    }

    _transformCom->Set_WorldPosition(newPos);
    _transformCom->LookAt(lookTarget);

    Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _prevTargetPos = targetPos;
}

void TargetCamera::Update(float timeDelta)
{
    __super::Update(timeDelta);
}

void TargetCamera::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);
}

HRESULT TargetCamera::Render()
{
   if(FAILED(__super::Render()))
       return E_FAIL;

    return S_OK;
}

void TargetCamera::SetTargetTransform(Shared<TransformCom> targetTransform)
{
    //타겟을 설정해줘야함
    _targetTransformCom = targetTransform;
}

Shared<TargetCamera> TargetCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<TargetCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : TargetCamera");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> TargetCamera::Clone(void* arg)
{
    auto instance = make_shared<TargetCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : TargetCamera");
        return nullptr;
    }
    return instance;
}

void TargetCamera::Free()
{
    __super::Free();
}
