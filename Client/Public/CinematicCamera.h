#pragma once
#include "Cinematic_Types.h"
#include "Camera.h"

NS_BEGIN(Client)
class CinematicCamera : public Camera
{
    GENERATED_GAMEOBJECT(CinematicCamera, ObjectType::CinematicCamera)

public:
    typedef struct tagCinematicCamera final : public Camera::CAMERA_DESC
    {
        
    }CINEMATICCAMERA_DESC;

public:
    CinematicCamera(const ComPtr<Device>& device,  const ComPtr<Context>& context);
    CinematicCamera(const CinematicCamera& prototype);
    virtual ~CinematicCamera() { Free();}

public:
    void            BeginPlay() override;
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    void            OnActivated() override;
    virtual void    Priority_Update(float timeDelta) override;
    virtual void    Update(float timeDelta) override;
    virtual void    Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    void            Apply_CinematicPose(const CAMERAPOSE& camPose);

public:
    static Shared<CinematicCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};
NS_END

