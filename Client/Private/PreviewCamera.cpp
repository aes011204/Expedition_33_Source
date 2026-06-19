#include "PreviewCamera.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(PreviewCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_VEC3("lookOffset", _lookOffset);
    PROPERTY_FLOAT("distance", _distance);
    PROPERTY_FLOAT("followSpeed", _followSpeed);

    return true;
}

PreviewCamera::PreviewCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

PreviewCamera::PreviewCamera(const PreviewCamera& prototype)
    : Camera{ prototype }
{
}

void PreviewCamera::BeginPlay()
{
    Camera::BeginPlay();

    // 이거 추가하면, 카메라쪽 로직 꼬이려나 잘 모르겠네;
    GAME->Add_Camera(GetSharedPtr<Camera>());
}

HRESULT PreviewCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT PreviewCamera::Initialize(void* arg)
{
    CHECK_FAILED(Camera::Initialize(arg), E_FAIL);

    const auto* desc = static_cast<Preview_CameraDesc*>(arg);
    if (desc != nullptr && _transformCom != nullptr)
    {
        const Vec3 at(desc->at.x, desc->at.y, desc->at.z);
        _startAt = at;
        _startEye = desc->eye;
        _lookOffset = desc->lookOffset;
        _distance = desc->distance;
        _followSpeed = desc->followSpeed;
    }

    Set_Name(L"Preview Camera");
    return S_OK;
}

void PreviewCamera::Update(float timeDelta)
{
    if(_targetTransformCom == nullptr)
        return;

    const Vec3 targetPos = _targetTransformCom->Get_WorldPosition() + _lookOffset;
    const Vec3 desiredEye = targetPos + Vec3(0.f, 0.f,_distance);

    const Vec3 currentEye = _transformCom->Get_WorldPosition();

    float t = min(1.f, timeDelta * _followSpeed);

    Vec3 nextPos =  Vec3::Lerp(currentEye, desiredEye, t);
    
    _transformCom->Set_WorldPosition(nextPos);
    _transformCom->LookAt(targetPos);

    Apply_View(); 
}

void PreviewCamera::Apply_View()
{
    Update_TransformMatrices();
}

void PreviewCamera::Set_Preset(PreviewShotPreset preset)
{
    _lookOffset = preset.lookOffset;
    _distance = preset.distance;
    _followSpeed = preset.followSpeed;
}

void PreviewCamera::Reset_ToStartPos()
{
    if(_transformCom == nullptr)
        return;

    _transformCom->Set_WorldPosition(Vec3(_startEye.x, _startEye.y, _startEye.z));
    _transformCom->LookAt(_startAt);

    Apply_View();
}

Shared<PreviewCamera> PreviewCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PreviewCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> PreviewCamera::Clone(void* arg)
{
    auto instance = make_shared<PreviewCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void PreviewCamera::Free()
{
    Camera::Free();
}
