#include "Camera.h"
#include "GameInstance.h"

Camera::Camera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

Camera::Camera(const Camera& prototype)
    : GameObject{ prototype }
    , _fovY{ prototype._fovY }
    , _near{ prototype._near }
    , _far{ prototype._far }
    , _aspect{ prototype._aspect }
{
}

HRESULT Camera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Camera::Initialize(void* arg)
{
    if (nullptr == arg)
        return E_FAIL;

    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    const CAMERA_DESC* desc = static_cast<CAMERA_DESC*>(arg);

    _transformCom->Set_Position(Vec3(desc->eye.x, desc->eye.y, desc->eye.z));

    _fovY = desc->fovy;
    _near = desc->nearPlane;
    _far = desc->farPlane;
    _yaw = desc->yaw;
    _pitch = desc->pitch;
    _aspect = static_cast<float>(GAME->Get_ViewportWidth()) /
              static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

    _transformCom->Set_RotationEuler(Vec3 {_pitch, _yaw, 0.f}); 

    Update_TransformMatrices();
    return S_OK;
}

void Camera::Priority_Update(float timeDelta)
{
}

void Camera::Update(float timeDelta)
{
}

void Camera::Late_Update(float timeDelta)
{
}

HRESULT Camera::Render()
{
    return S_OK;
}

void Camera::OnActivated()
{
    Apply_View();
}

void Camera::Apply_View()
{
    Update_TransformMatrices();
}

bool Camera::Try_GetFlatMoveBasis(Vec3& outForward, Vec3& outRight) const
{
    outForward = Vec3::Zero;
    outRight = Vec3::Zero;


    if(_transformCom == nullptr)
        return false;

    outForward = _transformCom->Get_WorldForward();
    outRight = _transformCom->Get_WorldRight();

    if(outForward.LengthSquared() > 0.f)
        outForward.Normalize();
    else
        outForward = Vec3::Look;

    if(outRight.LengthSquared() > 0.f)
        outRight.Normalize();
    else
        outRight = Vec3::Right;

    return true;
}

void Camera::Update_TransformMatrices()
{
    _aspect = static_cast<float>(GAME->Get_ViewportWidth()) /
              static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

    GAME->Set_Transform(
        D3DTS::View,
        XMMatrixInverse(nullptr, _transformCom->Get_WorldMatrix())
    );

    if (_near <= 0.f)
        _near = 0.0001f;

    GAME->Set_Transform(
        D3DTS::Proj,
        XMMatrixPerspectiveFovLH(_fovY, _aspect, _near, _far)
    );
}

void Camera::Free()
{
    __super::Free();
}
