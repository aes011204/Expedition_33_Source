#pragma once

#include "AnimNotifyState.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Client)

class EffectCom;

class ANS_Trail : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_Trail)

public:
    ANS_Trail() = default;
    ~ANS_Trail() override = default;

public:
    string Get_TypeName() const override { return "ANS_Trail"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;
    json To_Json() const override;
    void From_Json(const json& data) override;

public:
    enum class TipExtendInterpolation : uint8
    {
        Linear,
        Constant
    };

    struct TipExtendKey
    {
        float phase = 0.f;
        float value = 0.f;
        TipExtendInterpolation interpolation = TipExtendInterpolation::Linear;
    };

    struct TipExtendCurve
    {
        bool enabled = false;
        vector<TipExtendKey> keys{};
    };

    struct LocalOffsetKey
    {
        float phase = 0.f;
        Vec3 value = Vec3::Zero;
        TipExtendInterpolation interpolation = TipExtendInterpolation::Linear;
    };

    struct LocalOffsetCurve
    {
        bool enabled = false;
        vector<LocalOffsetKey> keys{};
    };

public:
    static const char* Get_TipExtendInterpolationName(TipExtendInterpolation interpolation);
    static float Evaluate_TipExtendCurve(const TipExtendCurve& curve, float phase);
    static Vec3 Evaluate_LocalOffsetCurve(const LocalOffsetCurve& curve, float phase);
    static TrailAnchorAsset Make_OffsetTrailAnchorAsset(
        const TrailAnchorAsset& source,
        const TipExtendCurve& tipExtendCurve,
        const LocalOffsetCurve& baseLocalOffsetCurve,
        const LocalOffsetCurve& tipLocalOffsetCurve,
        float phase);

    const TipExtendCurve& Get_TipExtendCurve() const { return _tipExtendCurve; }
    const LocalOffsetCurve& Get_BaseLocalOffsetCurve() const { return _baseLocalOffsetCurve; }
    const LocalOffsetCurve& Get_TipLocalOffsetCurve() const { return _tipLocalOffsetCurve; }

    void Set_TipExtendCurve(const TipExtendCurve& curve);
    void Set_BaseLocalOffsetCurve(const LocalOffsetCurve& curve);
    void Set_TipLocalOffsetCurve(const LocalOffsetCurve& curve);

private:
    static TipExtendInterpolation Parse_TipExtendInterpolation(const json& data);
    static bool Has_TipExtendCurvePayload(const TipExtendCurve& curve);
    static bool Has_LocalOffsetCurvePayload(const LocalOffsetCurve& curve);
    static json TipExtendCurve_ToJson(const TipExtendCurve& curve);
    static TipExtendCurve TipExtendCurve_FromJson(const json& data);
    static json LocalOffsetCurve_ToJson(const LocalOffsetCurve& curve);
    static LocalOffsetCurve LocalOffsetCurve_FromJson(const json& data);
    static void Normalize_TipExtendCurve(TipExtendCurve& curve);
    static void Normalize_LocalOffsetCurve(LocalOffsetCurve& curve);
    static uint64 Generate_TrailNotifyToken();
    Shared<EffectCom> Resolve_EffectCom(const AnimNotifyContext& context) const;

private:
    wstring _effectName = L""; // 비어 있으면 Weapon 프리펩 기본 Trail 사용
    wstring _partTag = L"Part_Weapon";
    wstring _layerTag = L"Layer_Effect";
    TipExtendCurve _tipExtendCurve{};
    LocalOffsetCurve _baseLocalOffsetCurve{};
    LocalOffsetCurve _tipLocalOffsetCurve{};
    uint64 _trailNotifyToken = 0;
};

NS_END
