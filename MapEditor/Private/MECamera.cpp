#include "pch.h"
#include "MECamera.h"
#include "GameInstance.h"

void MECamera::Initialize(float fov, float aspect, float nearZ, float farZ)
{
    _fov = fov;
    _aspect = aspect;
    _nearZ = nearZ;
    _farZ = farZ;

    ComputeMatrices();
}

void MECamera::UpdateInput(float timeDelta)
{
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        constexpr float sensitivity = 0.003f;
        _yaw += io.MouseDelta.x * sensitivity;
        _pitch += io.MouseDelta.y * sensitivity;
        _pitch = clamp(_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

        const Vec3 forward = GetForward();
        const Vec3 right = GetRight();
        const float speed = _moveSpeed * timeDelta;

        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            _position += forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            _position -= forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            _position += right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            _position -= right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            _position += Vec3(0.f, 1.f, 0.f) * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            _position -= Vec3(0.f, 1.f, 0.f) * speed;
        }
    }

    if (io.MouseWheel != 0.f)
    {
        _moveSpeed = clamp(_moveSpeed + io.MouseWheel, 1.f, 100.f);
    }

    ComputeMatrices();
}

void MECamera::Apply()
{
    GAME->Set_Transform(D3DTS::View, _view);
    GAME->Set_Transform(D3DTS::Proj, _proj);
}

void MECamera::ComputeMatrices()
{
    const Vec3 target = _position + GetForward();
    _view = XMMatrixLookAtLH(_position, target, Vec3(0.f, 1.f, 0.f));
    _proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(_fov), _aspect, _nearZ, _farZ);
}

Vec3 MECamera::GetForward() const
{
    return Vec3(cosf(_pitch) * sinf(_yaw), sinf(_pitch), cosf(_pitch) * cosf(_yaw));
}

Vec3 MECamera::GetRight() const
{
    Vec3 right = Vec3(0.f, 1.f, 0.f).Cross(GetForward());
    right.Normalize();
    return right;
}

void MECamera::FocusOn(const Vec3& target)
{
    _position = target - GetForward() * 8.f;
    ComputeMatrices();
}

Shared<MECamera> MECamera::Create(float fov, float aspect, float nearZ, float farZ)
{
    auto instance = make_shared<MECamera>();
    instance->Initialize(fov, aspect, nearZ, farZ);
    return instance;
}
