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

    struct WeaponEchoTrailProfile
    {
        wstring mainEffectName{};
        wstring tipEffectName{};
    };

    struct WeaponEchoTrailEventDesc
    {
        uint64 notifyToken = 0;
        string animationName{};
        float startSec = 0.f;
        float durationSec = 0.f;
        wstring sourceEffectName{};
        wstring layerTag{};
        EffectCom::TrailOffsetRuntimeDesc offsetDesc{};
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
        uint32 materialOverrideCount = 0,
        const WeaponEchoTrailProfile& trailProfile = {});

    void Record_WeaponEchoSample(float elapsedSec);
    void End_WeaponEchoPlayback();
    void Update_WeaponEchoSource(ModelCom* sourceModel, const string& sourceAnimationName, float sourceTimeSec);
    bool Has_WeaponEchoPlayback() const;
    void Queue_WeaponEchoRender();
    const wstring& Get_TrailEffectName() const { return _trailEffectName; }
    vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() { return _sourceHistoryAnchors; }
    const vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() const { return _sourceHistoryAnchors; }
    const WeaponSourceHistoryAnchorDesc* Find_SourceHistoryAnchor(const string& anchorName) const;
    bool Try_GetWeaponEchoTrailSample(uint32 laneIndex, uint64 notifyToken, EffectTrailSample& outSample) const;
    void Register_WeaponEchoTrailEvent(const WeaponEchoTrailEventDesc& desc);
    void Update_WeaponEchoTrailEvent(uint64 notifyToken, float sourceTimeSec, const EffectCom::TrailOffsetRuntimeDesc& offsetDesc);
    void End_WeaponEchoTrailEvent(uint64 notifyToken, float sourceEndSec);

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

        WeaponEchoMaterialOverrideDesc materialOverride{};
        unordered_map<uint32, Shared<Material>> materialOverrideCache{};
    };

    enum class WeaponEchoTrailRole : uint8
    {
        Main,
        Tip
    };

    struct WeaponEchoTrailLaneState
    {
        Shared<IEffectTrailSampleProvider> trailSampleProvider{};
        Shared<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
        EffectInstancePool::AttachedHandle handle{};
        bool started = false;
    };

    struct WeaponEchoTrailEvent
    {
        uint64 notifyToken = 0;
        string animationName{};
        float startSec = 0.f;
        float plannedEndSec = 0.f;
        float endSec = 0.f;
        float lastSourceTimeSec = 0.f;
        bool sourceEnded = false;
        WeaponEchoTrailRole role = WeaponEchoTrailRole::Main;
        wstring layerTag{};
        EffectCom::TrailOffsetRuntimeDesc offsetDesc{};
        TrailAnchorAsset trailAnchorAsset{};
        bool trailAnchorLoaded = false;
        array<WeaponEchoTrailLaneState, kMaxWeaponEchoLaneCount> laneStates{};
    };

    void Reset_WeaponEchoLane(WeaponEchoLane& lane);
    void Push_WeaponEchoSample(WeaponEchoLane& lane, float elapsedSec);
    bool Has_WeaponEchoPlayback(const WeaponEchoLane& lane) const;
    bool Try_SampleWeaponEchoMatrix(const WeaponEchoLane& lane, float elapsedSec, Matrix& outWorldMatrix) const;
    Shared<Material> Resolve_WeaponEchoRenderMaterial(
        WeaponEchoLane& lane,
        uint32 materialIndex,
        const Shared<Material>& sourceMaterial);
    void Update_WeaponEchoTrailEvents();
    void Start_WeaponEchoTrailEvent(
        WeaponEchoTrailEvent& trailEvent,
        uint32 laneIndex,
        const wstring& effectName);
    void Stop_WeaponEchoTrailEvents();
    void Stop_WeaponEchoTrailEvent(WeaponEchoTrailLaneState& laneState);
    void Clear_WeaponEchoPlayback();
    WeaponEchoTrailEvent* Find_WeaponEchoTrailEvent(uint64 notifyToken);
    const WeaponEchoTrailEvent* Find_WeaponEchoTrailEvent(uint64 notifyToken) const;

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};
    Matrix _detachedWorldMatrix = Matrix::Identity;
    bool _useSocketBinding = true;
    bool _isDetached = false;

    array<WeaponEchoLane, kMaxWeaponEchoLaneCount> _weaponEchoLanes{};
    vector<WeaponEchoTrailEvent> _weaponEchoTrailEvents{};
    WeaponEchoTrailProfile _weaponEchoTrailProfile{};
    ModelCom* _weaponEchoSourceModel = nullptr;
    string _weaponEchoSourceAnimationName{};
    float _weaponEchoLastKnownSourceTimeSec = 0.f;
    bool _weaponEchoHasSourceTime = false;

    wstring _trailEffectName = L"Simon_Trail_Phase1";
    vector<WeaponSourceHistoryAnchorDesc> _sourceHistoryAnchors{};

public:
    static Shared<Mon_Simon_Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
