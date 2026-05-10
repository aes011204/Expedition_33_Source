#pragma once
#include "Base.h"

NS_BEGIN(Engine)
class Camera;
class TransformCom;
class ShaderCom;

class Camera_Manager : public Base
{
public:
    Camera_Manager();
    virtual ~Camera_Manager();

public:
    HRESULT Initialize(void* arg);
    void    Update(float timeDelta);

public:
    void                 Change_ActiveCamera(Shared<Camera> camera);
    void                 SwitchTo_NextCamera();
    HRESULT              Add_Camera(Shared<Camera> camera);
    void                 Remove_Camera(Shared<Camera> camera);
    Shared<Camera>       Find_Camera(Shared<Camera> camera);
    bool                 Is_ActiveCamera(Shared<Camera> camera);
    
    int                  Get_CameraCount();
    Shared<TransformCom> Get_CameraTransform(int index);
    Shared<Camera>       Get_Camera(int index);
    Shared<Camera>       Get_ActiveCamera() {return m_activeCamera;}

    // 카메라 식별용
    bool               Activate_CameraByType(ObjectType objectType);
    bool               Is_ActiveCameraType(ObjectType objectType) const;
    bool               Focus_FreeCameraOnPosition(const Vec3& position);

    HRESULT             Bind_CameraFar(Shared<ShaderCom> shader, const char* constantName);

private:
    vector<Shared<Camera>> m_cameras;
    Shared<Camera>         m_activeCamera;
    bool                   _hasPendingFreeCameraFocus = false;
    Vec3                   _freeCameraFocusTarget = Vec3::Zero;
    float                  _freeCameraFocusSpeed = 8.f;

public:
    static Unique<Camera_Manager> Create();
    virtual void Free() override;
};

NS_END
