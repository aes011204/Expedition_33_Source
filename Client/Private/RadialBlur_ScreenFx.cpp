#include "pch.h"
#include "RadialBlur_ScreenFx.h"

#include <Camera.h>

#include "BattleController.h"
#include "BattleTurnCamera.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "FreeCamera.h"
#include "Player_WorldBase.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(RadialBlur_ScreenFx)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "Radial Blur (Action Cam)";
    info.type = RegisteredType::ScreenFx;

    PROPERTY_VEC2("Center", _originalDesc.center);
    PROPERTY_FLOAT("Intensity", _originalDesc.intensity, 0.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Radius", _originalDesc.radius, 0.f, 2.f, 0.01f);
    PROPERTY_FLOAT("Duration", _originalDesc.duration, 0.01f, 10.f, 0.1f);
    PROPERTY_FLOAT("FadeOut Time", _originalDesc.fadeOutTime, 0.f, 10.f, 0.1f);
    PROPERTY_FLOAT("Elapsed Time", _elapsedTime, 0.f, 10.f, 0.01f);
    return true;
}

HRESULT RadialBlur_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (desc)
    {
        if (FAILED(ScreenFx_Instance::Initialize(desc)))
            return E_FAIL;

        _passType = ETOI(ClientScreenFxManager::ScreenFx_Type::RadialBlur);
        RadialBlurDesc* blurDesc = static_cast<RadialBlurDesc*>(desc);
        _originalDesc = *blurDesc;
        _currentIntensity = _originalDesc.intensity;
    }
    return S_OK;
}

void RadialBlur_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _elapsedTime += timeDelta;

    auto mainCamera = GAME->Get_ActiveCamera();
    if (!mainCamera)
    {
        _targetIntensity = 0.f;
        _currentIntensity = 0.f;
        _prevLookDir = Vec3::Zero;
        return;
    }

    // 자유 카메라, 프리뷰 카메라에서는 ScreenFx 실행 X
    if (mainCamera->Get_ObjectType() == ObjectType::FreeCamera ||
        mainCamera->Get_ObjectType() == ObjectType::PreviewCamera)
    {
        _targetIntensity = 0.f;
        _currentIntensity = 0.f;
        _prevLookDir = Vec3::Zero;
        return;
    }

    auto transform = Get_CurrentTarget();
    Compute_LeaderHeroPosToProjection(transform);


    if (auto mainCamera = GAME->Get_ActiveCamera())
    {
        Vec3 currentLookDir = mainCamera->Get_Transform()->Get_WorldForward();
        currentLookDir.Normalize();

        if (_prevLookDir.Length() > 0.001f)
        {
            float dotProduct = currentLookDir.Dot(_prevLookDir);
            dotProduct = clamp(dotProduct, -1.f, 1.f);
            float angleChange = acos(dotProduct);
            float angularSpeed = angleChange / timeDelta;

            if (angularSpeed > _requireSpeed)
                _targetIntensity = clamp(angularSpeed * 0.02f, 0.05f, 0.15f);
            else
                _targetIntensity = 0.f;
        }
        _prevLookDir = currentLookDir;
    }
    else
    {
        _targetIntensity = 0.f;
    }

    float lerpSpeed = _targetIntensity > _currentIntensity ? 10.f : 3.f;
    _currentIntensity += (_targetIntensity - _currentIntensity) * (timeDelta * lerpSpeed);

    if (_currentIntensity < 0.001f)
        _currentIntensity = 0.0f;
}

HRESULT RadialBlur_ScreenFx::Bind_FxConstantBuffer(ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};

    cbData.radialBlurCenter = _originalDesc.center;
    cbData.radialBlurIntensity = _currentIntensity;
    cbData.radialBlurRadius = _originalDesc.radius;

    cbData.vignetteRadius = 0.8f;
    cbData.vignetteSoftness = 0.5f;
    cbData.vignetteColor = Vec3::Zero;

    return shader->Bind_CBufferData(cbData);
}

HRESULT RadialBlur_ScreenFx::Bind_ShaderResources(const Shared<ShaderCom>& shader)
{
    return S_OK;
}

void RadialBlur_ScreenFx::Evaluate_At(float elapsedTime)
{
    _elapsedTime = elapsedTime;

    if (_originalDesc.fadeOutTime > 0.0f)
    {
        float timeRemaining = _originalDesc.duration - _elapsedTime;
        if (timeRemaining <= _originalDesc.fadeOutTime && timeRemaining > 0.0f)
        {
            float ratio = timeRemaining / _originalDesc.fadeOutTime;
            _currentIntensity = _originalDesc.intensity * ratio;
        }
        else if (timeRemaining <= 0.0f)
        {
            _currentIntensity = 0.0f;
        }
        else
        {
            _currentIntensity = _originalDesc.intensity;
        }
    }
}

Shared<TransformCom> RadialBlur_ScreenFx::Get_CurrentTarget()
{
    auto bt = CLIENT->Get_BattleController();
    auto btCam = bt->Get_BattleTurnCamera();

    if (CLIENT->Is_BattleActive())
    {
        auto req = btCam->Get_CurrentRequest();

        Shared<TransformCom> trans{ nullptr };
        switch (req.mode)
        {
        case BattleCameraMode::PLAYERATTACK: case BattleCameraMode::ENEMYATTACK:
        case BattleCameraMode::WIDE: case BattleCameraMode::PLAYERFOCUS:
            if (req.subject != nullptr)
            {
                return req.subject->Get_Transform();
            }
            break;
        case BattleCameraMode::ENEMYFOCUS:
            if (req.opponent != nullptr)
            {
                return req.opponent->Get_Transform();
            }
            break;
        case BattleCameraMode::BATTLEINTRO:
        default:
            return nullptr;
        }
    }
    if (auto leaderHero = CLIENT->Get_ActiveHero())
    {
        auto trans = leaderHero->Get_Transform();
        return trans;
    }

    return nullptr;
}

void RadialBlur_ScreenFx::Compute_LeaderHeroPosToProjection(const Shared<TransformCom>& targetTransform)
{
    if (targetTransform != nullptr)
    {
        Vec3 worldPos = targetTransform->Get_WorldPosition();
        worldPos.y += 1.575f;

        const Matrix* viewMat = GAME->Get_Transform(D3DTS::View);
        const Matrix* projMat = GAME->Get_Transform(D3DTS::Proj);
        Vec2 curViewport = Vec2{
            static_cast<float>(GAME->Get_ViewportWidth()),
            static_cast<float>(GAME->Get_ViewportHeight())
        };

        Vec3 screenPos = XMVector3Project(
            worldPos, 0.f, 0.f,
            curViewport.x, curViewport.y,
            0.f, 1.f,
            *projMat, *viewMat, Matrix::Identity
        );

        if (screenPos.z < 1.0f)
        {
            Vec2 playerScreenUv{};
            playerScreenUv.x = screenPos.x / curViewport.x;
            playerScreenUv.y = screenPos.y / curViewport.y;
            _originalDesc.center = playerScreenUv;
        }
    }

}

Shared<RadialBlur_ScreenFx> RadialBlur_ScreenFx::Create(RadialBlurDesc* desc)
{
    auto instance = make_shared<RadialBlur_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END

