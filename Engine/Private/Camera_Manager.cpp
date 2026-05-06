#include "Camera_Manager.h"

#include "Camera.h"
#include "GameInstance.h"
#include "Helper_Math.h"
#include "ShaderCom.h"

Camera_Manager::Camera_Manager()
{
}

Camera_Manager::~Camera_Manager()
{
}

HRESULT Camera_Manager::Initialize(void* arg)
{
    return S_OK;
}

void Camera_Manager::Update(float timeDelta)
{
    if (nullptr == m_activeCamera)
        return;

    if (_hasPendingFreeCameraFocus && m_activeCamera->Get_ObjectType() == ObjectType::FreeCamera)
    {
        Shared<TransformCom> transform = m_activeCamera->Get_Transform();
        if (transform != nullptr)
        {
            const float focusDelta = max(timeDelta, GAME->Get_FreeCameraTimeDelta());
            const Vec3 currentPosition = transform->Get_WorldPosition();
            const Vec3 nextPosition = Math::Lerp_Damp(currentPosition, _freeCameraFocusTarget, _freeCameraFocusSpeed, focusDelta);

            transform->Set_WorldPosition(nextPosition);

            if ((_freeCameraFocusTarget - nextPosition).LengthSquared() <= 0.001f)
            {
                transform->Set_WorldPosition(_freeCameraFocusTarget);
                _hasPendingFreeCameraFocus = false;
            }
        }
        else
        {
            _hasPendingFreeCameraFocus = false;
        }
    }

    // 여기서 전환하면, 실제 게임플레이 때에도 자유카메라로 전환돼서 에디터쪽으로 옮깁니다.
    //if (GAME->KeyDown(KEY_TYPE::F8))
    //{
    //    SwitchTo_NextCamera();
    //}
}

void Camera_Manager::Change_ActiveCamera(Shared<Camera> camera)
{
    if (nullptr == camera)
        return;

    m_activeCamera = camera;
    _hasPendingFreeCameraFocus = false;
}

void Camera_Manager::SwitchTo_NextCamera()
{
    if (m_cameras.empty())
        return;

    _hasPendingFreeCameraFocus = false;

    auto iter = find(m_cameras.begin(), m_cameras.end(), m_activeCamera);

    int nextIndex = 0;

    if (iter != m_cameras.end())
    {
        int currentIndex = distance(m_cameras.begin(), iter);
        nextIndex = (currentIndex + 1) % m_cameras.size();
    }

    Shared<Camera> nextCamera = m_cameras[nextIndex];
    if (nextCamera == nullptr)
        return;

    // FreeCamera로 전환 시, Target 카메라의 현재 위치 넘겨주기
    if (m_activeCamera && nextCamera->Get_ObjectType() == ObjectType::FreeCamera)
    {
        Shared<TransformCom> currentTransform = m_activeCamera->Get_Transform();
        Shared<TransformCom> nextTransform = nextCamera->Get_Transform();

        if (currentTransform && nextTransform)
        {
            nextTransform->Set_WorldPosition(currentTransform->Get_WorldPosition());
            nextTransform->Set_RotationEuler(currentTransform->Get_RotationEuler());
        }

        nextCamera->Set_Yaw(m_activeCamera->Get_Yaw());
        nextCamera->Set_Pitch(m_activeCamera->Get_Pitch());
    }

    m_activeCamera = nextCamera;
}

HRESULT Camera_Manager::Add_Camera(Shared<Camera> camera)
{
    if (nullptr != Find_Camera(camera))
    {
        MSG_BOX("Failed to Add Camera");
        return E_FAIL;
    }

    m_cameras.push_back(camera);

    return S_OK;
}

Shared<Camera> Camera_Manager::Find_Camera(Shared<Camera> camera)
{
    auto iter = find(m_cameras.begin(), m_cameras.end(), camera);

    if (iter == m_cameras.end())
        return nullptr;

    return (*iter);
}

bool Camera_Manager::Is_ActiveCamera(Shared<Camera> camera)
{
    if (m_activeCamera != camera)
        return false;

    return true;
}

int Camera_Manager::Get_CameraCount()
{
    return m_cameras.size();
}

Shared<TransformCom> Camera_Manager::Get_CameraTransform(int index)
{
    return m_cameras[index]->Get_Transform();
}

Shared<Camera> Camera_Manager::Get_Camera(int index)
{
    return m_cameras[index];
}

bool Camera_Manager::Activate_CameraByType(ObjectType objectType)
{
    for (const auto& camera : m_cameras)
    {
        if (nullptr == camera)
            continue;

        if (camera->Get_ObjectType() == objectType)
        {
            m_activeCamera = camera;
            _hasPendingFreeCameraFocus = false;
            return true;
        }
    }

    return false;
}

bool Camera_Manager::Is_ActiveCameraType(ObjectType objectType) const
{
    if (nullptr == m_activeCamera)
        return false;

    return m_activeCamera->Get_ObjectType() == objectType;
}

bool Camera_Manager::Focus_FreeCameraOnPosition(const Vec3& position)
{
    for (const auto& camera : m_cameras)
    {
        if (camera == nullptr || camera->Get_ObjectType() != ObjectType::FreeCamera)
            continue;

        Shared<TransformCom> transform = camera->Get_Transform();
        if (transform == nullptr)
            return false;

        m_activeCamera = camera;
        _freeCameraFocusTarget = position;
        _hasPendingFreeCameraFocus = true;
        return true;
    }

    return false;
}

HRESULT Camera_Manager::Bind_CameraFar(Shared<ShaderCom> shader, const char* constantName)
{
    if (!m_activeCamera)
        return E_FAIL;

    const float farPlane = m_activeCamera->Get_Far();
    return shader->Bind_RawValue(constantName, &farPlane, sizeof(float));
}

Unique<Camera_Manager> Camera_Manager::Create()
{
    return make_unique <Camera_Manager>();
}

void Camera_Manager::Free()
{
    __super::Free();
}
