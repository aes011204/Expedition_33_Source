#pragma once
#include "EditorBlock.h"

NS_BEGIN(Engine)
struct MorphData;
class ModelCom;
class CurveClip;
NS_END

NS_BEGIN(ModelEditor)

class ModelTestPanel final : public EditorBlock
{
private:
    inline static constexpr char DEFAULT_CURVE_MODIFIER_TAG[] = "ModelEditor_CurveClip";

public:
	ModelTestPanel();
	~ModelTestPanel() override = default;

private:
	HRESULT Initialize() override;

public:
	void Update(float timeDelta) override;
	void Render() override;

private:
    void Render_MotionWarpingTest();
    void Render_MorphTargetTest();
    void Render_CurveClipTest();
    void Render_SelectedMorphDebug(const Engine::MorphData& morphData, const vector<float>* weights) const;

private: /* Shared panel helpers */
    Engine::ModelCom* Get_CurrentModelComponent() const;
    bool Browse_File(const wchar_t* filter, char* outBuffer, size_t bufferSize) const;

private: /* CurveClip test UI */
    void Render_CurveClipSourceControls();
    void Render_CurveClipPoseList();
    void Render_SelectedCurveClipPose();
    void Disable_CurveModifier();

private: /* CurveClip build/load utilities */
    HRESULT Load_CurveClip();
    HRESULT Build_SelectedCurveClipJson();
    void Apply_CurveClipPose();
    HRESULT Build_CurveClipsFromCurrentModelFolder();
    HRESULT Build_CurvePresetFromCurrentModelFolder();
    void Apply_CurvePresetModifier();
    HRESULT Build_CurveClipFromJsonFile(const fs::path& jsonPath, fs::path* outBuiltPath = nullptr) const;
    fs::path Get_CurrentModelDirectory() const;
    wstring Get_CurveModifierTag() const;
    pair<uint32, uint32> Count_CurveClipTargetMatches(const Engine::CurveClip& clip, uint32 poseIndex) const;

private: /* Motion Warping Test 관련 */
    bool    _testWarpingActive = false;
    float   _testTargetDistance = 5.0f;
    float   _testSpeed = 1.0f;
    float   _testPrevRatio = 0.0f;
    Vec3    _testStartPos = {};

    int32   _selectedAnimIndex = -1;

private:
    char    _morphFilter[128]{};
    int32   _selectedMorphIndex = -1;
    float   _selectedMorphWeight = 0.f;

private: /* CurveClip Test */
    char    _curveJsonPathBuffer[MAX_PATH]{};
    char    _curveClipPathBuffer[MAX_PATH]{};
    char    _curvePoseFilter[128]{};
    char    _curveModifierNameBuffer[128]{};
    Shared<Engine::CurveClip> _curveClip{};
    int32   _selectedCurvePoseIndex = -1;
    float   _curvePoseWeight = 1.f;
    float   _curveAnimSampleTimeSec = 1.f;
    bool    _curvePlayTimeline = true;
    bool    _curveLoopTimeline = true;
    string  _curveClipStatus{};

public:
	static Shared<ModelTestPanel> Create();
};

NS_END
