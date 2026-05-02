#pragma once
#include "Base.h"

NS_BEGIN(Engine)
class Camera;
class Camera_Manager : public Base
{
public:
    Camera_Manager();
    virtual ~Camera_Manager();

public:
    HRESULT Initialize(void* arg);
    void    Update(float timeDelta);

public:
    void              Change_ActiveCamera(Shared<Camera> camera);
    void              SwitchTo_NextCamera();
    HRESULT           Add_Camera(Shared<Camera> camera);
    Shared<Camera>    Find_Camera(Shared<Camera> camera);
    bool              Is_ActiveCamera(Shared<Camera> camera);

private:
    vector<Shared<Camera>> m_cameras;
    Shared<Camera>         m_activeCamera;


public:
    static Unique<Camera_Manager> Create();
    virtual void Free() override;
};

NS_END
