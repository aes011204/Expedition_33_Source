#pragma once

#include "Camera.h"
#include "CameraPresetTypes.h"

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
    } DIALOGUECAMERA_DESC;

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
    void Set_CameraPresetId(const string& cameraPresetId);
    const string& Get_CameraPresetId() const { return _cameraPresetId; }

    void Set_AnchorTransform(const Shared<TransformCom>& transformCom);
    void Set_SpeakerTransform(const Shared<TransformCom>& transformCom);
    void Set_ListenerTransform(const Shared<TransformCom>& transformCom);
    void Clear_Targets();

    void Capture_AnchorPose();
    void Snap_ToDialogueTargets();
    void Apply_ImmediatePose(const Vec3& eye, const Vec3& lookAt);

public:
    void Load_PresetFromId();
    bool Try_ResolveDialoguePose(Vec3& outEye, Vec3& outLookAt) const;
    void Apply_DriftToPose(float timeSec, Vec3& inOutEye, Vec3& inOutLookAt, float& inOutRollOffset) const;

private:
    Weak<TransformCom> _anchorTransformCom;
    Weak<TransformCom> _speakerTransformCom;
    Weak<TransformCom> _listenerTransformCom;
    Vec3 _capturedAnchorPos = Vec3::Zero;
    Vec3 _capturedAnchorForward = Vec3::Look;
    bool _hasCapturedAnchorPose = false;

    string _cameraPresetId;
    float _blendSpeed = 6.f;
    float _driftTimeSec = 0.f;
    DialogueCameraPreset _currentPreset{};

public:
    static Shared<DialogueCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
