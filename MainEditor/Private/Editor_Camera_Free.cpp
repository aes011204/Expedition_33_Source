#include "Editor_Camera_Free.h"

Editor_Camera_Free::Editor_Camera_Free(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera(device, context)
{
}

Editor_Camera_Free::Editor_Camera_Free(const Editor_Camera_Free& prototype)
    : Camera(prototype)
{
}

HRESULT Editor_Camera_Free::Initialize_Prototype()
{
    return Camera::Initialize_Prototype();
}

HRESULT Editor_Camera_Free::Initialize(void* arg)
{
    return Camera::Initialize(arg);
}

void Editor_Camera_Free::Priority_Update(float timeDelta)
{
    if (_transformCom == nullptr)
        return;

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        const float moveDistance = _transformCom->Get_SpeedPerSec() * timeDelta;
        const Vec3 forward = _transformCom->Get_WorldForward();
        const Vec3 right = _transformCom->Get_WorldRight();
        Vec3 position = _transformCom->Get_WorldPosition();

        if (ImGui::IsKeyDown(ImGuiKey_W))
            position += forward * moveDistance;

        if (ImGui::IsKeyDown(ImGuiKey_S))
            position -= forward * moveDistance;

        if (ImGui::IsKeyDown(ImGuiKey_A))
            position -= right * moveDistance;

        if (ImGui::IsKeyDown(ImGuiKey_D))
            position += right * moveDistance;

        if (ImGui::IsKeyDown(ImGuiKey_E))
            position += Vec3::Up * moveDistance;

        if (ImGui::IsKeyDown(ImGuiKey_Q))
            position -= Vec3::Up * moveDistance;

        _transformCom->Set_WorldPosition(position);

        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.f)
            _transformCom->Add_LocalRotation(Quat::CreateFromAxisAngle(Vec3::Up, delta.x * _mouseSensor * timeDelta));

        if (delta.y != 0.f)
        {
            Vec3 right = _transformCom->Get_WorldRight();
            if (right.LengthSquared() > 0.000001f)
            {
                right.Normalize();
                _transformCom->Add_LocalRotation(Quat::CreateFromAxisAngle(right, delta.y * _mouseSensor * timeDelta));
            }
        }
    }

    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.f)
    {
        const Vec3 position = _transformCom->Get_WorldPosition();
        const Vec3 forward = _transformCom->Get_WorldForward();
        _transformCom->Set_WorldPosition(position + forward * (wheel * 0.8f));
    }
}

Matrix Editor_Camera_Free::Get_ViewMatrix() const
{
    return _transformCom->Get_WorldMatrix().Invert();
}

void Editor_Camera_Free::Apply_EditorDesc(const EDITOR_CAMERA_DESC& desc)
{
    _mouseSensor = desc.mouseSensor;
    _fovY = desc.fovy;
    _near = desc.nearPlane;
    _far = desc.farPlane;
    _yaw = desc.yaw;
    _pitch = desc.pitch;

    if (_transformCom)
    {
        _transformCom->Set_SpeedPerSec(desc.speedPerSec);
        _transformCom->Set_DegreePerSec(desc.degreePerSec);
        _transformCom->Set_WorldPosition(Vec3(desc.eye.x, desc.eye.y, desc.eye.z));

        const Vec3 eye(desc.eye.x, desc.eye.y, desc.eye.z);
        const Vec3 at(desc.at.x, desc.at.y, desc.at.z);
        if ((at - eye).LengthSquared() > 0.000001f)
            _transformCom->LookAt(at);
        else
            _transformCom->Set_RotationEuler(Vec3(desc.pitch, desc.yaw, 0.f));
    }
}

Shared<Editor_Camera_Free> Editor_Camera_Free::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Editor_Camera_Free>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : Editor_Camera_Free");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Editor_Camera_Free::Clone(void* arg)
{
    auto instance = make_shared<Editor_Camera_Free>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Editor_Camera_Free");
        return nullptr;
    }

    return instance;
}

void Editor_Camera_Free::Free()
{
    Camera::Free();
}
