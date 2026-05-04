#include "pch.h"
#include "ModelEditorCamera.h"
#include "Input_Manager.h"

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
    
    if (GAME->KeyPress(KEY_TYPE::RBUTTON))
    {
        Vec2 delta = GAME->Get_MouseDelta();
        _yaw += delta.x * _mouseSensor;
        _pitch += delta.y * _mouseSensor;
        _pitch = std::clamp(_pitch, -89.f, 89.f);
        _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));
    }

    float adjustedTimeDelta = timeDelta;
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
