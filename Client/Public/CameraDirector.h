#pragma once
#include "Base.h"

NS_BEGIN(Engine)
class Camera;
NS_END

NS_BEGIN(Client)

    class CameraDirector final : public Base
{
public:
    CameraDirector();
    virtual ~CameraDirector();

public:
    void Request_GamePlay();
    void Request_Preview();
    void Request_BattleTurn();
    void Request_Cinematic();
    void Restore_PreviousCamera();

private:
    Shared<Camera> _previousCamera;

public:
    static Unique<CameraDirector> Create();
};
NS_END
