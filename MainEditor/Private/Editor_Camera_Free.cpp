#include "Editor_Camera_Free.h"

Editor_Camera_Free::Editor_Camera_Free(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Engine::Camera(device, context)
{
}

Editor_Camera_Free::Editor_Camera_Free(const Editor_Camera_Free& prototype)
    : Engine::Camera(prototype)
{
}

HRESULT Editor_Camera_Free::Initialize_Prototype()
{
    return Engine::Camera::Initialize_Prototype();
}

HRESULT Editor_Camera_Free::Initialize(void* arg)
{
    const HRESULT hr = Engine::Camera::Initialize(arg);
    if (FAILED(hr))
        return hr;

    Sync_RotationFromAngles();
    return S_OK;
}

void Editor_Camera_Free::Priority_Update(float timeDelta)
{
    if (_transformCom == nullptr)
        return;

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;

        _yaw += delta.x * _mouseSensor;
        _pitch += delta.y * _mouseSensor;
        _pitch = clamp(_pitch, -89.f, 89.f);

        if (delta.x != 0.f || delta.y != 0.f)
            Sync_RotationFromAngles();

        if (ImGui::IsKeyDown(ImGuiKey_W))
            _transformCom->Go_Straight(timeDelta);

        if (ImGui::IsKeyDown(ImGuiKey_S))
            _transformCom->Go_Backward(timeDelta);

        if (ImGui::IsKeyDown(ImGuiKey_A))
            _transformCom->Go_Left(timeDelta);

        if (ImGui::IsKeyDown(ImGuiKey_D))
            _transformCom->Go_Right(timeDelta);

        if (ImGui::IsKeyDown(ImGuiKey_E))
            _transformCom->Go_Up(timeDelta);

        if (ImGui::IsKeyDown(ImGuiKey_Q))
            _transformCom->Go_Down(timeDelta);
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
        const Vec3 lookVector = at - eye;

        if (lookVector.LengthSquared() > 0.000001f)
        {
            _transformCom->LookAt(at);

            const Vec3 euler = _transformCom->Get_RotationEuler();
            _pitch = clamp(euler.x, -89.f, 89.f);
            _yaw = euler.y;
            Sync_RotationFromAngles();
        }
        else
        {
            _pitch = clamp(_pitch, -89.f, 89.f);
            Sync_RotationFromAngles();
        }
    }
}

void Editor_Camera_Free::Sync_RotationFromAngles()
{
    if (_transformCom == nullptr)
        return;

    _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));
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
    Engine::Camera::Free();
}
