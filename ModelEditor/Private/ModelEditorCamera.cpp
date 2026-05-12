#include "pch.h"
#include "ModelEditorCamera.h"
#include "Input_Manager.h"
#include "ModelImGui_Manager.h"
#include "ModelSceneWindow.h"

NS_BEGIN(ModelEditor)

    ModelEditorCamera::ModelEditorCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context } {}

HRESULT ModelEditorCamera::Initialize_Prototype()
{
    return Camera::Initialize_Prototype();
}

HRESULT ModelEditorCamera::Initialize(void* arg)
{
    if (FAILED(Camera::Initialize(arg)))
        return E_FAIL;

    if (arg)
    {
        ModelEditorCameraDesc* desc = static_cast<ModelEditorCameraDesc*>(arg);
        _mouseSensor = desc->mouseSensor;
    }

    return S_OK;
}

void ModelEditorCamera::Priority_Update(float timeDelta)
{
    if (_firstFrame)
    {
        GAME->Add_Camera(GetSharedPtr<Camera>());
        GAME->Change_ActiveCamera(GetSharedPtr<Camera>());
        _firstFrame = false;
    }
    
    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    if (GAME->KeyDown(KEY_TYPE::TAB))
    {
        _presetIndex = (_presetIndex + 1) % 5;

        Vec3 eye{}, at{};
        switch (_presetIndex)
        {
        case 0: eye = { 0.f, 5.f, -5.f };      at = { 0.f, 0.f, 0.f };    break; 
        case 1: eye = { 0.f, 100.f, -200.f };  at = { 0.f, 0.f, 200.f };  break; 
        case 2: eye = { 0.f, 300.f, -500.f };  at = { 0.f, 0.f, 500.f };  break; 
        case 3: eye = { 0.f, 600.f, -1000.f }; at = { 0.f, 0.f, 800.f };  break; 
        case 4: eye = { 0.f, 1200.f, -2000.f }; at = { 0.f, 0.f, 1000.f }; break; 
        }

        _transformCom->Set_Position(eye);
        _transformCom->LookAt(at);

        Vec3 rotation = _transformCom->Get_RotationEuler();
        _yaw = rotation.y;
        _pitch = rotation.x;

        LOG_INFO("Camera Preset: {} | Eye:({:.0f},{:.0f},{:.0f}) At:({:.0f},{:.0f},{:.0f})", 
            _presetIndex, eye.x, eye.y, eye.z, at.x, at.y, at.z);
    }

    bool isHovered = false;
    auto imguiManager = ModelImGui_Manager::GetInstance();
    if (imguiManager && imguiManager->Get_ModelSceneWindow())
        isHovered = imguiManager->Get_ModelSceneWindow()->IsHovered();

    // 2. Camera Speed Control (Mouse Wheel)
    float wheel = GAME->Get_MouseWheel();
    if (wheel != 0 && isHovered)
    {
        float zoomAmount = wheel * _speed;
        if (GAME->KeyPress(KEY_TYPE::LBUTTON))
            zoomAmount *= 5.f;

        _transformCom->Go_Straight(zoomAmount);
    }

    if (GAME->KeyPress(KEY_TYPE::RBUTTON))
    {
        Vec2 delta = GAME->Get_MouseDelta();
        _yaw += delta.x * _mouseSensor;
        _pitch += delta.y * _mouseSensor;
        _pitch = std::clamp(_pitch, -89.f, 89.f);
        _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));
    }

    float adjustedTimeDelta = timeDelta * _speed;
    if (GAME->KeyPress(KEY_TYPE::SHIFT))
        adjustedTimeDelta *= 3.f;

    if (GAME->KeyPress(KEY_TYPE::W)) _transformCom->Go_Straight(adjustedTimeDelta);
    if (GAME->KeyPress(KEY_TYPE::S)) _transformCom->Go_Backward(adjustedTimeDelta);
    if (GAME->KeyPress(KEY_TYPE::A)) _transformCom->Go_Left(adjustedTimeDelta);
    if (GAME->KeyPress(KEY_TYPE::D)) _transformCom->Go_Right(adjustedTimeDelta);
    if (GAME->KeyPress(KEY_TYPE::E)) _transformCom->Go_Up(adjustedTimeDelta);
    if (GAME->KeyPress(KEY_TYPE::Q)) _transformCom->Go_Down(adjustedTimeDelta);

    Update_TransformMatrices();
}

void ModelEditorCamera::Update(float timeDelta)
{
}

void ModelEditorCamera::Late_Update(float timeDelta)
{
}

void ModelEditorCamera::Free()
{
    Camera::Free();
}

Shared<ModelEditorCamera> ModelEditorCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ModelEditorCamera>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : ModelEditorCamera");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> ModelEditorCamera::Clone(void* arg)
{
    auto instance = make_shared<ModelEditorCamera>(*this);
    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : ModelEditorCamera");
        return nullptr;
    }
    return instance;
}

NS_END
