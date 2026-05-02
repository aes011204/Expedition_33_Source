#include "Camera_Manager.h"
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

    if(GAME->KeyPress(KEY_TYPE::LCTRL))
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

//void Camera_Manager::Change_ActiveCamera(const wstring& cameraTag)
//{
//    if(m_activeCamera == Find_Camera(cameraTag))
//        return;
//
//    auto camera = Find_Camera(cameraTag);
//
//    m_activeCamera = camera;
//}
//
//Shared<Camera> Camera_Manager::Find_Camera(const wstring& cameraTag)
//{
//    auto iter =	m_cameras.find(cameraTag);
//
//    if (iter == m_cameras.end())
//        return nullptr;
//
//    return iter->second;
//}
//
bool Camera_Manager::Is_ActiveCamera(Shared<Camera> camera)
{
  if(m_activeCamera != camera)
      return false;

  return true;
}

//HRESULT Camera_Manager::Add_Camera(const wstring& cameraTag, Shared<Camera> camera)
//{
//    if(nullptr == Find_Camera(cameraTag))
//    {
//        MSG_BOX("Failed to Add Camera");
//        return E_FAIL;
//    }
//
//    m_cameras.insert({cameraTag, camera});
//}

Unique<Camera_Manager> Camera_Manager::Create()
{
    return make_unique <Camera_Manager>();
}

void Camera_Manager::Free()
{
    __super::Free();
}
