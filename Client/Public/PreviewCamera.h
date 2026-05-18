#pragma once

#include "Client_Defines.h"
#include "Camera.h"
#include "PreviewShotTable.h"

NS_BEGIN(Client)

    // UI에서 사용할 카메라 -> 일단 임시임
class PreviewCamera final : public Camera
{
    GENERATED_GAMEOBJECT(PreviewCamera, ObjectType::PreviewCamera)

public:
   typedef struct  Preview_CameraDesc final : public Camera::CAMERA_DESC
   {
      Vec3 lookOffset;   
      float distance;
      float followSpeed;

   }PREVIEW_CAMERADESC;

public:
    PreviewCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PreviewCamera(const PreviewCamera& prototype);
    ~PreviewCamera() override = default;

public:
    void    BeginPlay() override;
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Apply_View();

    void    Set_Preset(PreviewShotTable::PreviewShotPreset preset);
    void    Set_Distance(float distance) {_distance = distance;}
    void    Set_LookOffset(Vec3 lookOffset) {_lookOffset = lookOffset;}
    void    Set_FollowSpeed(float followSpeed) {_followSpeed = followSpeed;}
    void    Set_TargetTransform(Shared<TransformCom> transformCom) {_targetTransformCom = transformCom;}

    void    Reset_ToStartPos();

private:
    Shared<TransformCom>   _targetTransformCom = nullptr;
    Vec3                   _lookOffset;
    float                  _distance;
    float                  _followSpeed;

    Vec4                   _startEye;
    Vec3                   _startAt;

public:
    static Shared<PreviewCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
