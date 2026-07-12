#pragma once
#include "GameObject.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL EffectEmitter abstract : public GameObject
{
public: //## Types::RuntimeDesc
    struct EffectEmitterRuntimeDesc
    {
        Shared<GameObject> effectOwner{};
        uint32 emitterId{};
        string emitterName{};
        Vec3 localPosition{ 0.f, 0.f, 0.f };
        Vec3 localRotationDegrees{ 0.f, 0.f, 0.f };
        bool useLocalSpace{ false };
        bool enabled{ true };
        uint32 effectPlaybackSeed{ 0 };
    };

public:
    EffectEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectEmitter(const EffectEmitter& prototype);
    ~EffectEmitter() override = default;

public: //## Behavior::RootBinding
    HRESULT Attach_ToEffect(const EffectEmitterRuntimeDesc& runtimeDesc);
    void Sync_FromEffectOwner();
    void Set_EffectPlaybackSeed(uint32 effectPlaybackSeed);
    void Set_RuntimeLocalTransform(const Vec3& localPosition, const Vec3& localRotationDegrees);

public: //## Behavior::Lifecycle
    virtual HRESULT Reset_ForEffectReplay();
    virtual bool Can_ReloadDefinition(const EffectEmitterDefinition& emitterDefinition) const;
    virtual HRESULT Reload_Definition(const EffectEmitterDefinition& emitterDefinition);
    virtual bool Is_EffectFinished() const;

public: //## Behavior::ProviderBinding
    virtual void Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider);
    virtual void Bind_SourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider);
    virtual bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const;

public: //## Behavior::MaterialRuntime
    // attached effect 등장 연출에서 material alpha/erosion만 일시적으로 덮어쓴다.
    virtual void Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion);
    virtual void Set_MaterialTintOverride(const Vec4& targetTint, float strength);

public: //## Behavior::Sort
    virtual bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const;
    virtual EffectSortPolicy Get_BlendSortPolicy() const;
    virtual int32 Get_BlendSortLayer() const;
    virtual float Get_BlendSortBias() const;

public: //## Accessors::Runtime
    bool Is_EmitterEnabled() const { return _enabled; }
    uint32 Get_EmitterId() const { return _emitterId; }
    const string& Get_EmitterName() const { return _emitterName; }

protected: //## Data::Runtime
    Weak<GameObject> _effectOwner{};
    uint32 _emitterId{};
    string _emitterName{};
    Vec3 _localPosition{ 0.f, 0.f, 0.f };
    Vec3 _localRotationDegrees{ 0.f, 0.f, 0.f };
    bool _useLocalSpace{ false };
    bool _enabled{ true };
    uint32 _effectPlaybackSeed{ 0 };

protected: //## Hook::TransformSync
    virtual void On_EffectTransformSynced() {}

public:
    Shared<GameObject> Clone(void* arg) override = 0;
};

NS_END
