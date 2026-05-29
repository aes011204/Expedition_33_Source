#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Emitter_View;

class CurveEditor_View final : public Editor_Window
{
public:
    CurveEditor_View();
    ~CurveEditor_View() override = default;

public:
    void Render() override;
    void Pin_ModuleTargets(uint32 emitterId, uint32 moduleId);
    void Clear_PinnedTracks();

private:
    enum class CurveTargetChannel : uint8
    {
        Value,
        X,
        Y,
    };

    enum class CurveTargetPayloadKind : uint8
    {
        Float,
        Vector2,
    };

    struct CurveTargetIdentity
    {
        uint32 emitterId{};
        uint32 moduleId{};
        AuthoringModuleType moduleType{ AuthoringModuleType::Required };
        string propertyId{};
        CurveTargetChannel channel{ CurveTargetChannel::Value };

        bool operator==(const CurveTargetIdentity& rhs) const = default;
    };

    struct PinnedTrack
    {
        CurveTargetIdentity identity{};
        bool visible{ true };
    };

    struct CurveTarget
    {
        CurveTargetIdentity identity{};
        AuthoringEmitter* emitter{};
        AuthoringModule* module{};
        variant<FloatDistributionData*, Vector2DistributionData*> distribution{};
        string label{};
        string propertyLabel{};
        CurveTargetPayloadKind payloadKind{ CurveTargetPayloadKind::Float };
        CurveTargetChannel channel{ CurveTargetChannel::Value };
        ImU32 color{};
    };

private:
    static bool Is_SameIdentity(const CurveTargetIdentity& lhs, const CurveTargetIdentity& rhs);
    static const char* Get_ChannelLabel(CurveTargetChannel channel);
    static float Read_Vector2Channel(const Vec2& value, CurveTargetChannel channel);
    static void Write_Vector2Channel(Vec2& value, CurveTargetChannel channel, float channelValue);
    static string Build_Label(const AuthoringEmitter& emitter, const AuthoringModule& module, const string& propertyLabel, CurveTargetChannel channel);
    static ImU32 Get_TargetColor(const CurveTarget& target);
    static float Get_TargetValue(const CurveTarget& target, const FloatCurveKeyData& key);
    static float Get_TargetValue(const CurveTarget& target, const Vector2CurveKeyData& key);
    static void Set_TargetValue(const CurveTarget& target, FloatCurveKeyData& key, float value);
    static void Set_TargetValue(const CurveTarget& target, Vector2CurveKeyData& key, float value);
    static float Get_DefaultValue(const FloatDistributionData& distribution);
    static Vec2 Get_DefaultValue(const Vector2DistributionData& distribution);
    static ConstantCurveFloatDistributionData* Resolve_CurvePayload(FloatDistributionData& distribution);
    static ConstantCurveVector2DistributionData* Resolve_CurvePayload(Vector2DistributionData& distribution);
    static void Normalize_CurveKeys(ConstantCurveFloatDistributionData& curve);
    static void Normalize_CurveKeys(ConstantCurveVector2DistributionData& curve);
    static float Evaluate_Curve(const ConstantCurveFloatDistributionData& curve, float x);
    static Vec2 Evaluate_Curve(const ConstantCurveVector2DistributionData& curve, float x);

    Shared<Emitter_View> Resolve_EmitterView() const;
    vector<CurveTarget> Collect_ModuleTargets(AuthoringEmitter& emitter, AuthoringModule& module) const;
    vector<CurveTarget> Resolve_PinnedTargets();
    optional<CurveTarget> Resolve_Target(const CurveTargetIdentity& identity) const;
    optional<size_t> Find_PinnedTrackIndex(const CurveTargetIdentity& identity) const;
    optional<size_t> Find_ResolvedTargetIndex(const vector<CurveTarget>& targets, const CurveTargetIdentity& identity) const;
    optional<CurveTarget> Get_ActiveTarget(const vector<CurveTarget>& targets) const;

    void Add_FloatTarget(vector<CurveTarget>& targets, AuthoringEmitter& emitter, AuthoringModule& module, FloatDistributionData& distribution, const string& propertyId, const string& label, ImU32 color) const;
    void Add_Vector2Target(vector<CurveTarget>& targets, AuthoringEmitter& emitter, AuthoringModule& module, Vector2DistributionData& distribution, const string& propertyId, const string& label) const;
    void Pin_Targets(const vector<CurveTarget>& targets);
    void Remove_StalePinnedTracks(const vector<CurveTarget>& targets);
    void Ensure_ActiveTrack(const vector<CurveTarget>& targets);

    bool Convert_ActiveTargetToConstantCurve(CurveTarget& target);
    void Draw_EmptyState();
    void Draw_TrackList(const vector<CurveTarget>& targets);
    void Draw_TargetHeader(CurveTarget& target);
    void Draw_CurveGraph(const vector<CurveTarget>& targets, CurveTarget& activeTarget);
    void Draw_SelectedKeyInspector(CurveTarget& target);

public:
    static Shared<CurveEditor_View> Create();

private:
    vector<PinnedTrack> _pinnedTracks{};
    optional<CurveTargetIdentity> _activeTrack{};
    optional<size_t> _selectedKeyIndex{};
};

NS_END
