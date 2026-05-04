#include "Camera_Manager.h"

#include "Camera.h"
#include "GameInstance.h"

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
    if(nullptr == m_activeCamera)
        return;

    if(GAME->KeyDown(KEY_TYPE::CTRL))
    {
        SwitchTo_NextCamera();
    }
}

void Camera_Manager::Change_ActiveCamera(Shared<Camera> camera)
{
    if(nullptr == camera)
        return;

    m_activeCamera = camera;
}

void Camera_Manager::SwitchTo_NextCamera()
{
    if(m_cameras.empty())
        return;

    auto iter = find(m_cameras.begin(), m_cameras.end(), m_activeCamera);

    int nextIndex = 0;

    if(iter != m_cameras.end())
    {
        int currentIndex = distance(m_cameras.begin(), iter);
        nextIndex = (currentIndex + 1) % m_cameras.size();
    }
    else
    {
        nextIndex = 0;
    }

    m_activeCamera = m_cameras[nextIndex];
}

HRESULT Camera_Manager::Add_Camera(Shared<Camera> camera)
{
    if(nullptr != Find_Camera(camera))
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

    if(iter == m_cameras.end())
        return nullptr;

    return (*iter);
}

bool Camera_Manager::Is_ActiveCamera(Shared<Camera> camera)
{
  if(m_activeCamera != camera)
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

Unique<Camera_Manager> Camera_Manager::Create()
{
    return make_unique <Camera_Manager>();
}

void Camera_Manager::Free()
{
    __super::Free();
}
