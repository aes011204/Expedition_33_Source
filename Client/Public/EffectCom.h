#pragma once

#include "ANS_Trail.h"
#include "Component.h"
#include "EffectInstancePool.h"
#include "PartObject.h"
#include "PreviewRimFeature.h"
#include "SourcePointSampleProvider_ByPart.h"
#include "SwitchDissolveFeature.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Engine)
class GameObject;
class EffectInstance;
class IEffectTrailSampleProvider;
class ShaderCom;
NS_END

NS_BEGIN(Client)

class Weapon;

class EffectCom final : public Component
{
    GENERATED_COMPONENT(EffectCom);

public: //## Types::Public
    enum class EffectPlayTarget : uint8
    {
        OwnerRoot = 0,
        PartSocket = 1,
        PartRoot = 2,
    };

    struct EffectTransformInheritance
    {
        bool inheritPosition = true;
        bool inheritRotation = true;
        bool inheritScale = false;
    };

    struct EffectPlayDesc
    {
        uint32 levelIndex = ETOI(LevelType::Static);
        wstring effectName{};
        EffectPlayTarget target = EffectPlayTarget::OwnerRoot;

        wstring partTag{};
        string socketName{};
        wstring layerTag = L"Layer_Effect";

        Vec3 localOffset = Vec3::Zero;
        Vec3 localRotation = Vec3::Zero;
        Vec3 scale = Vec3::One;
        float playbackSpeed = 1.f;
        EffectTransformInheritance transformInheritance{};
        bool localOffsetZTowardCamera = false;

        bool followTarget = false;
        bool requiresManualRelease = false;
        Weak<IEffectTrailSampleProvider> trailSampleProvider{};
    };

    struct InspectorEffectSettings
    {
        bool autoPlay = false;
        wstring effectName = L"";
        EffectPlayTarget target = EffectPlayTarget::OwnerRoot;
        wstring partTag = L"";
        string socketName = "";
        wstring layerTag = L"Layer_Effect";
        Vec3 localOffset = Vec3::Zero;
        Vec3 localRotation = Vec3::Zero;
        Vec3 scale = Vec3::One;
        bool followTarget = false;
        bool requiresManualRelease = false;
    };

    struct AttachedEffectPauseState
    {
        Shared<EffectInstance> effect{};
        bool wasPaused = false;
    };

    struct AttachedEffectDebugSummary
    {
        uint32 totalCount = 0;
        uint32 activeCount = 0;
        uint32 trailCount = 0;
        uint32 trailDrainingCount = 0;
        uint32 sourceHistoryCount = 0;
        uint32 sourceHistoryDrainingCount = 0;
    };

    struct TrailOffsetRuntimeDesc
    {
        float phase = 0.f;
        ANS_Trail::TipExtendCurve tipExtendCurve{};
        ANS_Trail::LocalOffsetCurve baseLocalOffsetCurve{};
        ANS_Trail::LocalOffsetCurve tipLocalOffsetCurve{};
    };

    struct TrailAnchorRuntimeDesc
    {
        ANS_Trail::TrailAnchorSource source = ANS_Trail::TrailAnchorSource::ModelDefault;
        string baseBoneName{};
        Vec3 baseLocalOffset{ Vec3::Zero };
        string tipBoneName{};
        Vec3 tipLocalOffset{ Vec3::Zero };
    };

public:
    EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~EffectCom() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Update(float timeDelta);
    void Tick_AttachedEffectInstances(float timeDelta);

public: //## Behavior::DrawFeatures
    void Start_SwitchDissolve(float durationSec, const string& noiseName);
    void Start_VanishDissolve(float durationSec, const string& noiseName, bool vanishComplete);
    void Set_PreviewRim(const Vec3& color, float intensity, float power, float hairIntensity);
    void Clear_PreviewRim();

    HRESULT Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask);

    static HRESULT Bind_DrawFeatures(const Shared<GameObject>& owner, ShaderCom* shader, uint32 supportFeatureMask);

public: //## Behavior::AttachmentState
    void Stop_AllAttachedEffects();
    static Shared<EffectCom> Find_FromNotifyOwner(GameObject* notifyOwner);
    static Shared<EffectCom> Find_FromNotifyOwner(GameObject* notifyOwner, EffectPlayTarget target);

    void Update_AttachedEffects(float timeDelta);
    void Capture_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& outStates);
    void Restore_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& states);
    AttachedEffectDebugSummary Get_AttachedEffectDebugSummary() const;

    InspectorEffectSettings Get_InspectorEffectSettings() const;
    void Set_InspectorEffectSettings(const InspectorEffectSettings& settings);
    void Set_InspectorEffectLocalOffset(const Vec3& localOffset);
    void Set_AttachedEffectMaterialReveal(float alphaMultiplier, float alphaErosion);
    void Set_AttachedEffectMaterialTintOverride(const vector<uint32>& emitterIds, const Vec4& targetTint, float strength);
    HRESULT Play_InspectorEffect();

public: //## Behavior::Playback
    HRESULT Play_Effect(const EffectPlayDesc& desc);
    HRESULT Play_Effect(const EffectPlayDesc& desc, EffectInstancePool::AttachedHandle* outHandle);

    void Stop_Effect(
        const wstring& effectName,
        EffectPlayTarget target,
        const wstring& partTag = L"",
        const string& socketName = "");
    void Stop_Effect(const EffectInstancePool::AttachedHandle& handle);

public: //## Behavior::PartAttachment
    HRESULT Play_AttachedEffect(
        uint32 levelIndex,
        const wstring& effectName,
        const wstring& partTag,
        const string& socketName,
        const wstring& layerTag,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One,
        const Weak<IEffectTrailSampleProvider>& trailSampleProvider = {});

    HRESULT Play_AttachedPartEffect(
        uint32 levelIndex,
        const wstring& effectName,
        const wstring& partTag,
        const string& socketName,
        const wstring& layerTag,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One);

    HRESULT Play_AttachedPartEffect(
        uint32 levelIndex,
        const wstring& effectName,
        const wstring& partTag,
        const wstring& layerTag,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One);

public: //## Behavior::TrailAndSourceHistory
    HRESULT Play_TrailEffect(
        const wstring& effectName,
        const wstring& partTag,
        const wstring& layerTag,
        uint64 trailNotifyToken = 0,
        const TrailOffsetRuntimeDesc& offsetDesc = TrailOffsetRuntimeDesc{},
        const TrailAnchorRuntimeDesc& anchorDesc = TrailAnchorRuntimeDesc{});

    void Update_TrailEffectOffset(
        const wstring& effectName,
        const wstring& partTag,
        uint64 trailNotifyToken,
        const TrailOffsetRuntimeDesc& offsetDesc);

    void Stop_TrailEffect(
        const wstring& effectName,
        const wstring& partTag,
        uint64 trailNotifyToken = 0);

    bool Try_GetTrailSampleForPart(
        const wstring& partTag,
        const TrailAnchorAsset& trailAnchorAsset,
        uint64 trailNotifyToken,
        EffectTrailSample& outSample) const;

    bool Try_GetActiveTrailEffect(
        const wstring& partTag,
        wstring& outEffectName,
        wstring& outLayerTag,
        TrailOffsetRuntimeDesc& outOffsetDesc) const;

    HRESULT Play_SourceHistoryEffect(
        const wstring& effectName,
        const wstring& partTag,
        const wstring& layerTag,
        SourceHistorySourceBindingMode sourceBindingMode = SourceHistorySourceBindingMode::WeaponAnchor,
        const string& socketName = "",
        const string& sourceHistoryAnchorName = "Source",
        const Vec3& sourceHistoryLocalOffset = Vec3::Zero);

    void Stop_SourceHistoryEffect(
        const wstring& effectName,
        const wstring& partTag,
        SourceHistorySourceBindingMode sourceBindingMode = SourceHistorySourceBindingMode::WeaponAnchor,
        const string& socketName = "",
        const string& sourceHistoryAnchorName = "Source");

    bool Try_GetSourcePointSampleForPart(
        const wstring& partTag,
        SourceHistorySourceBindingMode sourceBindingMode,
        const string& socketName,
        const string& sourceHistoryAnchorName,
        const Vec3& sourceHistoryLocalOffset,
        EffectSourcePointSample& outSample) const;

private: //## Types::AttachedEffect
    enum class AttachedEffectRole : uint8
    {
        Normal,
        Trail,
        SourceHistoryPoint,
    };

    struct AttachedEffect
    {
        EffectInstancePool::AttachedHandle poolHandle{};

        wstring effectName{};
        EffectPlayTarget target = EffectPlayTarget::OwnerRoot;
        wstring partTag{};

        string socketName{};
        wstring layerTag{};

        Vec3 localOffset{ Vec3::Zero };
        Vec3 localRotation{ Vec3::Zero };
        Vec3 scale{ Vec3::One };
        EffectTransformInheritance transformInheritance{};
        bool localOffsetZTowardCamera = false;

        bool followTarget = false;
        AttachedEffectRole role{ AttachedEffectRole::Normal };
        TrailAnchorAsset trailAnchorAsset{};
        Shared<IEffectTrailSampleProvider> trailSampleProvider{};
        uint64 trailNotifyToken = 0;
        TrailOffsetRuntimeDesc trailOffset{};
        bool trailEchoEligible = true;
        Shared<bool> trailSampleEnabled{};
        bool isTrailDraining = false;
        float trailDrainElapsed = 0.f;
        float trailDrainDuration = 0.25f;
        SourceHistorySourceBindingMode sourceHistorySourceBindingMode{
            SourceHistorySourceBindingMode::WeaponAnchor };
        string sourceHistoryAnchorName{ "Source" };
        Vec3 sourceHistoryLocalOffset{ Vec3::Zero };
        Shared<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
        Shared<bool> sourcePointSampleEnabled{};
        bool isSourceHistoryDraining = false;
        float sourceHistoryDrainElapsed = 0.f;
        float sourceHistoryDrainDuration = 0.5f;
    };

    struct RecentTrailEffect
    {
        bool valid = false;
        wstring partTag{};
        wstring effectName{};
        wstring layerTag{};
        TrailOffsetRuntimeDesc offsetDesc{};
        bool echoEligible = true;
        float ageSec = 0.f;
    };

private: //## Data::DrawFeatures
    SwitchDissolveRuntime _switchDissolve{};
    PreviewRimRuntime _previewRim{};

private: //## Data::AttachedEffects
    vector<AttachedEffect> _attachedEffects{};

private: //## Data::Inspector
    bool _vanishComplete = false;

    RecentTrailEffect _recentTrailEffect{};

    bool _inspectorAutoPlayEffect = false;
    wstring _inspectorEffectName = L"";
    int _inspectorEffectTarget = static_cast<int>(EffectPlayTarget::OwnerRoot);
    wstring _inspectorPartTag = L"";
    string _inspectorSocketName = "";
    wstring _inspectorLayerTag = L"Layer_Effect";
    Vec3 _inspectorLocalOffset = Vec3::Zero;
    Vec3 _inspectorLocalRotation = Vec3::Zero;
    Vec3 _inspectorScale = Vec3::One;
    bool _inspectorFollowTarget = false;
    bool _inspectorRequiresManualRelease = false;
    bool _inspectorEffectPlayed = false;
    float _attachedEffectRevealAlphaMultiplier = 1.f;
    float _attachedEffectRevealAlphaErosion = 0.f;

private: //## Helper::AttachedEffects
    void Play_InspectorEffectIfEnabled();
    void Release_AttachedEffect(AttachedEffect& attached);
    void Begin_TrailDrain(AttachedEffect& attached);
    void Begin_SourceHistoryDrain(AttachedEffect& attached);
    bool Should_ReleaseAttachedEffect(const AttachedEffect& attached) const;
    void Prune_ReleasedAttachedEffects();
    void Apply_AttachedEffectMaterialReveal(const Shared<GameObject>& effectObject) const;

    bool Try_GetOwnerRootWorldMatrix(Matrix& outWorldMatrix) const;

    bool Try_GetSocketWorldMatrix(
        const wstring& partTag,
        const string& socketName,
        Matrix& outWorldMatrix) const;

    bool Try_GetPartRootWorldMatrix(
        const wstring& partTag,
        Matrix& outWorldMatrix) const;

    bool Try_ResolveEffectWorldTransform(
        EffectPlayTarget target,
        const wstring& partTag,
        const string& socketName,
        const Vec3& localOffset,
        const Vec3& localRotation,
        const Vec3& scale,
        const EffectTransformInheritance& transformInheritance,
        bool localOffsetZTowardCamera,
        Vec3& outWorldPosition,
        Quat& outWorldRotation,
        Vec3& outWorldScale) const;

    bool Try_ResolveTrailEffectName(
        const wstring& effectNameOverride,
        const wstring& partTag,
        wstring& outEffectName) const;

    bool Try_BuildTrailSample(
        const wstring& partTag,
        const TrailAnchorAsset& trailAnchorAsset,
        const TrailOffsetRuntimeDesc* offsetDesc,
        EffectTrailSample& outSample) const;

    bool Try_ResolveTrailAnchorWorldPosition(
        const PartObject& trailPart,
        const TrailAnchorPointDesc& anchor,
        Vec3& outWorldPosition) const;

    bool Try_ResolveSourceHistoryEffectName(
        const wstring& effectName,
        wstring& outEffectName) const;

    bool Try_BuildSourcePointSample(
        const wstring& partTag,
        SourceHistorySourceBindingMode sourceBindingMode,
        const string& socketName,
        const string& sourceHistoryAnchorName,
        const Vec3& sourceHistoryLocalOffset,
        const TrailAnchorAsset* trailAnchorAsset,
        EffectSourcePointSample& outSample) const;

    bool Try_ResolveWeaponAnchorWorldPosition(
        const PartObject& weaponPart,
        const string& anchorName,
        const Vec3& localOffset,
        Vec3& outWorldPosition) const;

public:
    static Shared<EffectCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
