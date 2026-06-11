#pragma once

#include "GameObject.h"
#include "MapStaticMesh.h"

NS_BEGIN(Client)

    class EyeTarget;

class PuzzleController : public GameObject
{
    GENERATED_GAMEOBJECT(PuzzleController, ObjectType::None)

public:
    PuzzleController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PuzzleController(const PuzzleController& prototype);
    virtual ~PuzzleController() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void    BeginPlay() override;
    virtual void    Update(float timeDelta) override;
    virtual void    From_Json(const json& data) override;

private:
    void Collect_EyeTargets();
    void Collect_BridgeObject();
    void BridgeMoving(float timeDelta);
    void Reset_Puzzle();

private:
    vector<Shared<EyeTarget>> _eyeTargets;
    Shared<MapStaticMesh>     _bridgeObject;
    int                       _destroyedCount = {};

    bool                      _isBridgeMoving = false;
    Vec3                     _bridgeStartPos = {};
    Vec3                     _bridgeStartRot = {};
    Vec3                     _bridgeTargetPos = {};
    Vec3                     _bridgeTargetRot = {};
    float                     _bridgeMoveDuration  = 5.f;
    float                     _bridgeMoveElapsed = {};

public:
    static Shared<PuzzleController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
