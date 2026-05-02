#include "Camera_Manager.h"

Camera_Manager::Camera_Manager()
{
}

Camera_Manager::~Camera_Manager()
{
}

HRESULT Camera_Manager::Initialize(void* arg)
{
}

void Camera_Manager::Update(float timeDelta)
{
}

void Camera_Manager::Change_ActiveCamera(const wstring& cameraTag)
{
    if(m_activeCamera == Find_Camera(cameraTag))
        return;

    auto camera = Find_Camera(cameraTag);

    m_activeCamera = camera;
}

Shared<Camera> Camera_Manager::Find_Camera(const wstring& cameraTag)
{
    auto iter =	m_cameras.find(cameraTag);

    if (iter == m_cameras.end())
        return nullptr;

    return iter->second;
}

HRESULT Camera_Manager::Add_Camera(const wstring& cameraTag, Shared<Camera> camera)
{
    if(nullptr == Find_Camera(cameraTag))
    {
        MSG_BOX("Failed to Add Camera");
        return E_FAIL;
    }

    m_cameras.insert({cameraTag, camera});
}

Unique<Camera_Manager> Camera_Manager::Create()
{
    return make_unique <Camera_Manager>();
}

void Camera_Manager::Free()
{
    __super::Free();
}
