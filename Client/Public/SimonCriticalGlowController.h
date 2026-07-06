#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)

class SimonCriticalGlowController final : public Base
{
public:
    SimonCriticalGlowController() = default;
    virtual ~SimonCriticalGlowController() = default;

public:
    HRESULT Initialize();
    void Set_Critical(bool critical, float duration);
    void Reset_ToBaseColor();
    void Update(float timeDelta);

private:
    void Resolve_TargetMaterials();

private:
    bool _hasResolvedTarget = false;
    vector<pair<Shared<ModelCom>, uint32>> _targetMaterials;

    bool _isTransitioning = false;
    float _elapsed = 0.f;
    float _duration = 1.f;

    Vec4 _currentColor = {};
    Vec4 _fromColor = {};
    Vec4 _toColor = {};
    Vec4 _baseColor = {};

    bool _isCritical = false;

public:
    static Unique<SimonCriticalGlowController> Create();
    void Free() override;
};

NS_END
