#include "pch.h"
#include "EffectEditorCamera.h"

#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Helper_Math.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    static bool Is_PreviewCameraDragInputHeld()
    {
        const bool altHeld =
            GAME->KeyPress(KEY_TYPE::ALT) || GAME->KeyDown(KEY_TYPE::ALT);

        return GAME->KeyPress(KEY_TYPE::RBUTTON) ||
               (altHeld &&
                (ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                 ImGui::IsMouseDown(ImGuiMouseButton_Middle)));
    }
}

EffectEditorCamera::EffectEditorCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

EffectEditorCamera::EffectEditorCamera(const EffectEditorCamera& prototype)
    : Camera{ prototype }
    , _pivot{ prototype._pivot }
    , _targetPivot{ prototype._targetPivot }
    , _focusSpeed{ prototype._focusSpeed }
    , _distance{ prototype._distance }
    , _homePivot{ prototype._homePivot }
    , _homeDistance{ prototype._homeDistance }
    , _homeYaw{ prototype._homeYaw }
    , _homePitch{ prototype._homePitch }
{
}

EffectEditorCamera::~EffectEditorCamera()
{
    Free();
}

HRESULT EffectEditorCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT EffectEditorCamera::Initialize(void* arg)
{
    if (arg == nullptr)
        return E_FAIL;

    const auto desc = static_cast<EffectEditorCameraDesc*>(arg);

    _mouseSensor = desc->mouseSensor;
    _pivot = desc->pivot;
    _targetPivot = _pivot;
    _focusSpeed = max(0.1f, desc->focusSpeed);
    _distance = desc->distance;

    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    const Vec3 eye = Vec3(desc->eye.x, desc->eye.y, desc->eye.z);
    _homePivot = _pivot;
    _homeDistance = max((eye - _homePivot).Length(), 0.1f);

    _transformCom->Set_WorldPosition(eye);
    _transformCom->LookAt(_homePivot);

    const Vec3 rotationEuler = _transformCom->Get_WorldRotationEuler();
    _homePitch = rotationEuler.x;
    _homeYaw = rotationEuler.y;

    if (_distance <= 0.01f)
    {
        _distance = _homeDistance;
        _pitch = _homePitch;
        _yaw = _homeYaw;
    }

    Update_CameraTransform();
    Update_TransformMatrices();

    Set_Name(L"EffectEditor Camera");

    return S_OK;
}

void EffectEditorCamera::BeginPlay()
{
    Camera::BeginPlay();

    CHECK_FAILED(GAME->Add_Camera(GetSharedPtr<Camera>()));
}

void EffectEditorCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    const float cameraDelta = max(timeDelta, GAME->Get_FreeCameraTimeDelta());

    if (_hasPendingFocus)
    {
        _pivot = Math::Lerp_Damp(_pivot, _targetPivot, _focusSpeed, cameraDelta);
        if ((_targetPivot - _pivot).LengthSquared() <= 0.001f)
        {
            _pivot = _targetPivot;
            _hasPendingFocus = false;
        }
    }

    if (_hasPendingHome)
    {
        _pivot = Math::Lerp_Damp(_pivot, _homePivot, _focusSpeed, cameraDelta);
        _distance = Math::Lerp_Damp(_distance, _homeDistance, _focusSpeed, cameraDelta);
        _yaw = Math::Lerp_Damp(_yaw, _homeYaw, _focusSpeed, cameraDelta);
        _pitch = Math::Lerp_Damp(_pitch, _homePitch, _focusSpeed, cameraDelta);

        if ((_homePivot - _pivot).LengthSquared() <= 0.001f &&
            fabsf(_homeDistance - _distance) <= 0.01f &&
            fabsf(_homeYaw - _yaw) <= 0.01f &&
            fabsf(_homePitch - _pitch) <= 0.01f)
        {
            _pivot = _homePivot;
            _distance = _homeDistance;
            _yaw = _homeYaw;
            _pitch = _homePitch;
            _hasPendingHome = false;
        }
    }

    if (!EDITOR->Is_PreviewCameraControlEnabled())
    {
        EDITOR->End_PreviewCameraDrag();
        Update_CameraTransform();
        Update_TransformMatrices();
        return;
    }

    if (EDITOR->Is_PreviewCameraDragging() && !Internal::Is_PreviewCameraDragInputHeld())
        EDITOR->End_PreviewCameraDrag();

    if (ImGui::IsKeyPressed(ImGuiKey_F, false))
        Start_HomeTransition();

    const bool altHeld =
        GAME->KeyPress(KEY_TYPE::ALT) || GAME->KeyDown(KEY_TYPE::ALT);
    const bool panDragging = altHeld && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool orbitDragging = altHeld && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool rightMouseHeld = EDITOR->Is_PreviewCameraDragging() && GAME->KeyPress(KEY_TYPE::RBUTTON);
    const bool dollyDragging = altHeld && rightMouseHeld;
    const bool freeFlyDragging = !altHeld && rightMouseHeld;
    const bool isDragging = panDragging || orbitDragging || dollyDragging || freeFlyDragging;
    const float wheelDelta = GAME->Get_MouseWheel();

    if (rightMouseHeld && wheelDelta != 0.f)
    {
        _hasPendingFocus = false;
        _hasPendingHome = false;

        const float wheelStep = wheelDelta / static_cast<float>(WHEEL_DELTA);
        const float wheelDollyScale = max(0.1f, _distance * 0.1f);
        _distance = clamp(_distance - wheelStep * wheelDollyScale, 0.5f, _far);

        Update_CameraTransform();
        Update_TransformMatrices();
        return;
    }

    if (panDragging || orbitDragging || dollyDragging)
    {
        _hasPendingFocus = false;
        _hasPendingHome = false;
        _targetPivot = _pivot;

        const Vec2 mouseDelta = GAME->Get_MouseDelta();

        if (panDragging)
        {
            const float panScale = max(0.01f, _distance * 0.0025f);
            const Vec3 panOffset =
            (-_transformCom->Get_WorldRight() * mouseDelta.x +
             _transformCom->Get_WorldUp() * mouseDelta.y) * panScale;

            _pivot += panOffset;
            _targetPivot = _pivot;
        }
        else if (orbitDragging)
        {
            _yaw += mouseDelta.x * _mouseSensor;
            _pitch += mouseDelta.y * _mouseSensor;
            _pitch = clamp(_pitch, -89.f, 89.f);
        }
        else if (dollyDragging)
        {
            const float dollyScale = max(0.02f, _distance * 0.0035f);
            _distance = clamp(_distance - mouseDelta.x * dollyScale, 0.5f, _far);
        }

        Update_CameraTransform();
        Update_TransformMatrices();
        return;
    }

    if (freeFlyDragging)
    {
        _hasPendingFocus = false;
        _hasPendingHome = false;

        if (!rightMouseHeld && GAME->KeyPress(KEY_TYPE::CTRL) && wheelDelta != 0.f)
        {
            float moveSpeed = _transformCom->Get_SpeedPerSec();

            constexpr float step = 2.f;
            if (wheelDelta > 0.f)
                moveSpeed += step;
            else
                moveSpeed -= step;

            moveSpeed = clamp(moveSpeed, 0.f, 100.f);
            _transformCom->Set_SpeedPerSec(moveSpeed);
        }

        if (GAME->KeyPress(KEY_TYPE::W))
            _transformCom->Go_Straight(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::S))
            _transformCom->Go_Backward(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::A))
            _transformCom->Go_Left(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::D))
            _transformCom->Go_Right(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::E))
            _transformCom->Go_Up(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::Q))
            _transformCom->Go_Down(cameraDelta);

        const Vec2 mouseDelta = GAME->Get_MouseDelta();
        _yaw += mouseDelta.x * _mouseSensor;
        _pitch += mouseDelta.y * _mouseSensor;

        if (0.f != mouseDelta.x || 0.f != mouseDelta.y)
            _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));

        Sync_PivotFromCurrentView();
        Update_TransformMatrices();
        return;
    }

    Update_CameraTransform();
    Update_TransformMatrices();
}

void EffectEditorCamera::Update(float)
{
}

void EffectEditorCamera::Late_Update(float)
{
}

HRESULT EffectEditorCamera::Render()
{
    return S_OK;
}

void EffectEditorCamera::Request_Focus(const Vec3& pivot)
{
    _targetPivot = pivot;
    _hasPendingFocus = true;
    _hasPendingHome = false;
}

void EffectEditorCamera::Sync_PivotFromCurrentView()
{
    _distance = max(_distance, 0.1f);

    const Vec3 worldForward = _transformCom->Get_WorldForward();
    if (worldForward.LengthSquared() <= 0.0001f)
        return;

    _pivot = _transformCom->Get_WorldPosition() + worldForward * _distance;
    _targetPivot = _pivot;
}

void EffectEditorCamera::Start_HomeTransition()
{
    _hasPendingFocus = false;
    _hasPendingHome = true;
    _targetPivot = _homePivot;
}

void EffectEditorCamera::Update_CameraTransform()
{
    _distance = max(_distance, 0.1f);

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

    const Vec3 baseOffset = backDir * _distance;
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    _transformCom->Set_WorldPosition(_pivot + orbitOffset);
    _transformCom->LookAt(_pivot);
}

Shared<EffectEditorCamera> EffectEditorCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectEditorCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : EffectEditorCamera");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> EffectEditorCamera::Clone(void* arg)
{
    auto instance = make_shared<EffectEditorCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : EffectEditorCamera");
        return nullptr;
    }

    return instance;
}

void EffectEditorCamera::Free()
{
    __super::Free();
}

NS_END
