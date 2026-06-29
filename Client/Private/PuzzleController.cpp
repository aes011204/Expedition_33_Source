#include "pch.h"
#include "PuzzleController.h"
#include "EyeTarget.h"
#include "GameInstance.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(PuzzleController)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.className = "PuzzleController";
    info.displayName = "PuzzleController";
    info.category = "GameObject";
    return true;
}

PuzzleController::PuzzleController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

PuzzleController::PuzzleController(const PuzzleController& prototype)
    : GameObject(prototype)
{
}

HRESULT PuzzleController::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT PuzzleController::Initialize(void* arg)
{


    return __super::Initialize(arg);
}

void PuzzleController::BeginPlay()
{
    __super::BeginPlay();

    Collect_EyeTargets();
    Collect_BridgeObject();

    for(const Shared<EyeTarget>& eyeTarget : _eyeTargets)
    {
        if(!eyeTarget)
            continue;

        eyeTarget->OnDestroyed.Add([this]()
        {
            ++ _destroyedCount;

            if(_destroyedCount >= static_cast<int>(_eyeTargets.size()))
            {
                if(_bridgeObject != nullptr && _bridgeObject->Get_Transform() != nullptr)
                {
                    _bridgeStartPos = _bridgeObject->Get_Transform()->Get_WorldPosition();
                    _bridgeStartRot = _bridgeObject->Get_Transform()->Get_WorldRotationEuler();
                    _bridgeTargetPos = Vec3(47.08f, 8.42f, -22.46f);
                    _bridgeTargetRot = Vec3(-3.5f, -15.3f, 2.1f);
                    _bridgeMoveElapsed = 0.f;
                    _isBridgeMoving = true;
                }

                CLIENT->Play_Sequence(Constants::Cinematic::BridgeCinematic, 0);
            }
        });
    }
}

void PuzzleController::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if(GAME->KeyPress(KEY_TYPE::F6))
        Reset_Puzzle();

    if(!_isBridgeMoving)
        return;

    BridgeMoving(timeDelta);
}

void PuzzleController::From_Json(const json& data)
{
    __super::From_Json(data);
}

void PuzzleController::Collect_EyeTargets()
{
    _eyeTargets.clear();

    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if(layerTag != TEXT("Layer_EyeTarget"))
                return;

            Shared<EyeTarget> eyeTarget = dynamic_pointer_cast<EyeTarget>(gameObject);
            if (eyeTarget == nullptr)
                return;

            _eyeTargets.push_back(eyeTarget);
        });

    if(!_eyeTargets.empty())
        return;

    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring&, const Shared<GameObject>& gameObject)
        {
            Shared<EyeTarget> eyeTarget = dynamic_pointer_cast<EyeTarget>(gameObject);
            if (eyeTarget == nullptr)
                return;

            _eyeTargets.push_back(eyeTarget);
        });
}

void PuzzleController::Collect_BridgeObject()
{
    _bridgeObject.reset();

    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if(layerTag != TEXT("Layer_MapStaticMesh"))
                return;

            if(gameObject == nullptr)
                return;

            if(gameObject->Get_Name() != TEXT("Half_Bridge_B_199"))
                return;

            Shared<MapStaticMesh> bridge =  dynamic_pointer_cast<MapStaticMesh>(gameObject);

            if(bridge == nullptr)
                return;

            _bridgeObject = bridge;
        }
    );
}

void PuzzleController::BridgeMoving(float timeDelta)
{
    if(_bridgeObject == nullptr || _bridgeObject->Get_Transform() == nullptr)
        return;

    _bridgeMoveElapsed += timeDelta;

    float t = _bridgeMoveElapsed / _bridgeMoveDuration;
    t = Math::GetSmoothStepRatio(t);
    Vec3 lerpedPos = Vec3::Lerp(_bridgeStartPos, _bridgeTargetPos, t);
    Vec3 lerpedRot = Vec3::Lerp(_bridgeStartRot, _bridgeTargetRot, t);

    _bridgeObject->Get_Transform()->Set_WorldPosition(lerpedPos);
    _bridgeObject->Get_Transform()->Set_WorldRotationEuler(lerpedRot);

    if(_bridgeMoveElapsed >= _bridgeMoveDuration)
    {
        _bridgeMoveElapsed = _bridgeMoveDuration;
        _isBridgeMoving = false;
        _bridgeObject->Rebuild_StaticCollisionFromModel();
    }
}

void PuzzleController::Reset_Puzzle()
{
    _destroyedCount = 0;
    _bridgeMoveElapsed = 0;

    if(_bridgeObject != nullptr && _bridgeObject->Get_Transform() != nullptr)
    {
        _bridgeObject->Get_Transform()->Set_WorldPosition(_bridgeStartPos);
        _bridgeObject->Get_Transform()->Set_WorldRotationEuler(_bridgeStartRot);
    }

    for(size_t i = 0;  i < _eyeTargets.size() ; i++)
    {
        _eyeTargets[i]->Reset_Target();
    }
}

Shared<PuzzleController> PuzzleController::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PuzzleController>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> PuzzleController::Clone(void* arg)
{
    auto clone = make_shared<PuzzleController>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void PuzzleController::Free()
{
    __super::Free();
}
