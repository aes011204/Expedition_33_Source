#pragma once
#include "AnimationModifier.h"
#include "CurveClip.h"

NS_BEGIN(Engine)

class ENGINE_DLL CurveAnimationModifier : public AnimationModifier
{
public:
    struct CurveModifierDesc : public AnimationModifierDesc
    {
        Shared<CurveClip> clip{};
        string poseName{};
        int32 poseIndex{ -1 };
        float poseWeight{ 1.f };
        bool allowWeightOvershoot{ false };
        bool playTimeline{ false };
        bool loopTimeline{ true };
    };

public:
    CurveAnimationModifier() = default;
    ~CurveAnimationModifier() override = default;

public:
    HRESULT Initialize(AnimationModifierDesc* desc) override;
    void Evaluate_Modifier(ModelCom* model, float timeDelta) override;

    void Set_CurveClip(const Shared<CurveClip>& clip);
    bool Set_Pose(const string& poseName);
    bool Set_PoseIndex(int32 poseIndex);
    void Set_PoseWeight(float weight) { _poseWeight = weight; }
    void Set_AllowWeightOvershoot(bool allow) { _allowWeightOvershoot = allow; }
    void Set_PlayTimeline(bool play) { _playTimeline = play; }
    void Set_LoopTimeline(bool loop) { _loopTimeline = loop; }
    void Set_TimelineTimeSec(float timeSec) { _timelineTimeSec = std::max(0.f, timeSec); }
    HRESULT Load_Preset(const string& presetPath);
    HRESULT Load_PresetForModel(const string& modelPath);
    const vector<Shared<CurveClip>>& Get_Clips() const { return _clips; }

    static HRESULT Save_Preset(const string& presetPath, const vector<string>& clipPaths);
    static string Build_PresetPathForModel(const string& modelPath, const wstring& modifierTag);

protected:
    virtual void Begin_CurveEvaluation(ModelCom* model, const CurveClip::CurveClip_Pose& pose, float weight);
    virtual void Apply_Curve(ModelCom* model, const CurveClip::Curve& curve, float weight);
    virtual void End_CurveEvaluation(ModelCom* model, const CurveClip::CurveClip_Pose& pose, float weight);

protected:
    Shared<CurveClip> _clip{};
    vector<Shared<CurveClip>> _clips{};
    int32 _poseIndex{ -1 };
    float _poseWeight{ 1.f };
    float _timelineTimeSec{ 0.f };
    bool _allowWeightOvershoot{ false };
    bool _playTimeline{ false };
    bool _loopTimeline{ true };

public:
    static Shared<CurveAnimationModifier> Create(CurveModifierDesc* desc);
};

NS_END
