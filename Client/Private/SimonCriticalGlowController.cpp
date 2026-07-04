#include "SimonCriticalGlowController.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "MapStaticMesh.h"
#include "ModelCom.h"

namespace
{
    constexpr wchar_t kTargetMeshName[] = L"EditorSphere_11";
    const Vec4 kCriticalColor = Vec4(1.f, 0.05f, 0.02f, 1.f);
}

HRESULT SimonCriticalGlowController::Initialize()
{
    return S_OK;
}

void SimonCriticalGlowController::Set_Critical(bool critical, float duration)
{
    if (!_hasResolvedTarget)
    {
        Resolve_TargetMaterials();
    }

    _isCritical = critical;
    _duration = duration;
    _fromColor = _currentColor;
    _toColor = critical ? kCriticalColor : _baseColor;
    _elapsed = 0.f;
    _isTransitioning = true;
}

void SimonCriticalGlowController::Update(float timeDelta)
{
    if (!_isTransitioning)
    {
        return;
    }

    _elapsed += timeDelta;

    float t = _elapsed / _duration;
    t = Math::GetSmoothStepRatio(t);
    _currentColor = Vec4::Lerp(_fromColor, _toColor, t);

    for (const auto& [model, materialIndex] : _targetMaterials)
    {
        MaterialInstanceOverride overrideData{};
        overrideData.cbMask.emissiveColor = true;
        overrideData.cb.emissiveColor = _currentColor;
        model->Set_InstanceOverride(materialIndex, overrideData);
        model->Apply_EffectiveMaterial(materialIndex);
    }

    if (_elapsed >= _duration)
    {
        _currentColor = _toColor;
        _isTransitioning = false;
    }
}

void SimonCriticalGlowController::Resolve_TargetMaterials()
{
    _targetMaterials.clear();
    _hasResolvedTarget = true;

    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (!_targetMaterials.empty())
            {
                return;
            }

            const Shared<MapStaticMesh> staticMesh = dynamic_pointer_cast<MapStaticMesh>(gameObject);
            if (staticMesh == nullptr || staticMesh->Get_Name() != kTargetMeshName)
            {
                return;
            }

            const Shared<ModelCom> model = staticMesh->Get_Model();
            if (model == nullptr)
            {
                return;
            }

            const uint32 numMeshes = static_cast<uint32>(model->Get_NumMeshes());
            for (uint32 i = 0; i < numMeshes; ++i)
            {
                const uint32 materialIndex = model->Get_MeshMaterialIndex(i);
                _targetMaterials.emplace_back(model, materialIndex);
            }

            if (const Shared<Material> material = model->Get_Material(model->Get_MeshMaterialIndex(0)))
            {
                _baseColor = material->Get_CB().emissiveColor;
                _currentColor = _baseColor;
            }
        });
}

Unique<SimonCriticalGlowController> SimonCriticalGlowController::Create()
{
    auto instance = make_unique<SimonCriticalGlowController>();

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("Failed to Created : SimonCriticalGlowController");
        return nullptr;
    }

    return instance;
}

void SimonCriticalGlowController::Free()
{
    Base::Free();
}
