#pragma once

#include "Client_Defines.h"
#include "EffectCom.h"
#include "Monster_Part.h"
#include "ModelCom.h"
#include "TrailAnchor_Asset.h"
#include "Weapon.h"

NS_BEGIN(Client)

class Mon_Simon_Weapon final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Simon_Weapon, ObjectType::Weapon)

public:
    struct  WEAPON_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const  Matrix* socketMatrix = nullptr;
    };

    struct WeaponEchoMaterialOverrideDesc
    {
        bool enabled = false;
        string materialBundleKey{};
    };

public:
    Mon_Simon_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Simon_Weapon(const Mon_Simon_Weapon& prototype);
    virtual ~Mon_Simon_Weapon() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

    void From_Json(const json& data) override;

public:
    void Set_UseSocketBinding(bool useSocketBinding) { _useSocketBinding = useSocketBinding; }
    void Detach_PreserveWorld();
    void Attach_ToBindings();

    void Begin_WeaponEchoPlayback(
        float delaySec,
        uint32 echoCount = 1,
        ModelCom* sourceModel = nullptr,
        const string& sourceAnimationName = {},
        float sourceTimeSec = 0.f,
        const WeaponEchoMaterialOverrideDesc* materialOverrides = nullptr,
        uint32 materialOverrideCount = 0);

    void Record_WeaponEchoSample(float elapsedSec);
    void End_WeaponEchoPlayback();
    void Update_WeaponEchoSource(ModelCom* sourceModel, const string& sourceAnimationName, float sourceTimeSec);
    bool Has_WeaponEchoPlayback() const;
    void Queue_WeaponEchoRender();
    const wstring& Get_TrailEffectName() const { return _trailEffectName; }
    vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() { return _sourceHistoryAnchors; }
    const vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() const { return _sourceHistoryAnchors; }
    const WeaponSourceHistoryAnchorDesc* Find_SourceHistoryAnchor(const string& anchorName) const;
    bool Try_GetWeaponEchoTrailSample(uint32 laneIndex, const TrailAnchorAsset& trailAnchorAsset, EffectTrailSample& outSample) const;
    void Begin_WeaponEchoTrail(
        const wstring& effectName,
        const wstring& layerTag,
        const EffectCom::TrailOffsetRuntimeDesc& offsetDesc);
    void Update_WeaponEchoTrailOffset(const EffectCom::TrailOffsetRuntimeDesc& offsetDesc);
    void Set_WeaponEchoTrailEffectOverride(const wstring& effectName);

private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;
    json To_Json() const override;
    HRESULT Ready_DefaultComponents();
    HRESULT Ensure_RuntimeComponentsFromJson(const json& data);

    void Update_WeaponEchoPlayback(float timeDelta);
    HRESULT Render_WeaponEchoPlayback();

    static constexpr uint32 kMaxWeaponEchoLaneCount = 2;

    struct WeaponEchoSample
    {
        float elapsedSec = 0.f;
        Matrix worldMatrix = Matrix::Identity;
    };

    struct WeaponEchoLane
    {
        vector<WeaponEchoSample> samples{};
        bool active = false;
        bool recordingEnded = false;
        float delaySec = 0.2f;
        float playbackSec = 0.f;
        float endElapsedSec = 0.f;

        TrailAnchorAsset trailAnchorAsset{};
        Shared<IEffectTrailSampleProvider> trailSampleProvider{};
        Shared<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
        EffectInstancePool::AttachedHandle trailHandle{};
        EffectCom::TrailOffsetRuntimeDesc trailOffset{};
        bool trailStarted = false;

        WeaponEchoMaterialOverrideDesc materialOverride{};
        unordered_map<uint32, Shared<Material>> materialOverrideCache{};
    };

    void Reset_WeaponEchoLane(WeaponEchoLane& lane);
    void Push_WeaponEchoSample(WeaponEchoLane& lane, float elapsedSec);
    bool Has_WeaponEchoPlayback(const WeaponEchoLane& lane) const;
    bool Try_SampleWeaponEchoMatrix(const WeaponEchoLane& lane, float elapsedSec, Matrix& outWorldMatrix) const;
    Shared<Material> Resolve_WeaponEchoRenderMaterial(
        WeaponEchoLane& lane,
        uint32 materialIndex,
        const Shared<Material>& sourceMaterial);
    void Start_WeaponEchoTrail(
        WeaponEchoLane& lane,
        uint32 laneIndex,
        const wstring& effectName,
        const wstring& layerTag,
        const EffectCom::TrailOffsetRuntimeDesc& offsetDesc);
    const wstring& Resolve_WeaponEchoTrailEffectName(const wstring& effectName) const;
    void Stop_WeaponEchoTrail();
    void Stop_WeaponEchoTrail(WeaponEchoLane& lane);

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};
    Matrix _detachedWorldMatrix = Matrix::Identity;
    bool _useSocketBinding = true;
    bool _isDetached = false;

    array<WeaponEchoLane, kMaxWeaponEchoLaneCount> _weaponEchoLanes{};
    bool _weaponEchoPendingTrailValid = false;
    wstring _weaponEchoPendingTrailEffectName{};
    wstring _weaponEchoPendingTrailLayerTag{};
    EffectCom::TrailOffsetRuntimeDesc _weaponEchoPendingTrailOffset{};
    float _weaponEchoPendingTrailAgeSec = 0.f;
    wstring _weaponEchoTrailEffectOverride{};
    ModelCom* _weaponEchoSourceModel = nullptr;
    string _weaponEchoSourceAnimationName{};
    float _weaponEchoLastKnownSourceTimeSec = 0.f;
    bool _weaponEchoHasSourceTime = false;

    wstring _trailEffectName = L"";
    vector<WeaponSourceHistoryAnchorDesc> _sourceHistoryAnchors{};

public:
    static Shared<Mon_Simon_Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
