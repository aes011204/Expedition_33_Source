#pragma once

#include "Camera.h"

NS_BEGIN(Client)

class DialogueCamera final : public Camera
{
    GENERATED_GAMEOBJECT(DialogueCamera, ObjectType::DialogueCamera)

public:
    typedef struct tagDialogueCameraDesc final : public Camera::CAMERA_DESC
    {
        float distance = 2.5f;
        float sideOffset = 0.35f;
        float heightOffset = 1.2f;
        float blendSpeed = 6.f;
    } DIALOGUE_CAMERA_DESC;

public:
    DialogueCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    DialogueCamera(const DialogueCamera& prototype);
    ~DialogueCamera() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void OnActivated() override;
    void Update(float timeDelta) override;

public:
    void Set_CameraPresetId(const string& cameraPresetId) { _cameraPresetId = cameraPresetId; }
    const string& Get_CameraPresetId() const { return _cameraPresetId; }

    void Set_SpeakerTransform(const Shared<TransformCom>& transformCom) { _speakerTransformCom = transformCom; }
    void Set_ListenerTransform(const Shared<TransformCom>& transformCom) { _listenerTransformCom = transformCom; }
    void Clear_Targets();

    void Snap_ToDialogueTargets();
    void Apply_ImmediatePose(const Vec3& eye, const Vec3& lookAt);

private:
    bool Try_ResolveDialoguePose(Vec3& outEye, Vec3& outLookAt) const;

private:
    Weak<TransformCom> _speakerTransformCom;
    Weak<TransformCom> _listenerTransformCom;

    string _cameraPresetId;

    float _distance = 2.5f;
    float _sideOffset = 0.35f;
    float _heightOffset = 1.2f;
    float _blendSpeed = 6.f;

public:
    static Shared<DialogueCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
