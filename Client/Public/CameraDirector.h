#pragma once
#include "Base.h"
enum class TargetCameraMode;

NS_BEGIN(Engine)
class Camera;
NS_END

NS_BEGIN(Client)
class DialogueCamera;
class CinematicCamera;
class CameraDirector final : public Base
{
public:
    CameraDirector();
    virtual ~CameraDirector();

public:
    void Request_GamePlay();
    void Request_SoulLikeCamera();
    void Request_Preview();
    void Request_BattleTurn();
    Shared<DialogueCamera> Request_DialogueCamera();
    Shared<CinematicCamera>  Request_Cinematic();
    void Restore_PreviousCamera();

    void Request_ChangeTargetCameraMode(TargetCameraMode mode);

private:
    Shared<Camera> _previousCamera;

public:
    static Unique<CameraDirector> Create();
};
NS_END
