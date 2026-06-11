#include "pch.h"
#include "CurveAnimationModifier.h"

#include "ModelCom.h"

NS_BEGIN(Engine)

namespace
{
    constexpr uint32 CURVE_PRESET_MAGIC = 0x54535043; // 'CPST'
    constexpr uint32 CURVE_PRESET_VERSION = 1;
    constexpr float CURVE_WEIGHT_EPSILON = 0.000001f;
    constexpr wchar_t CURVE_PRESET_EXTENSION[] = L".curvepreset";
}

HRESULT CurveAnimationModifier::Initialize(AnimationModifierDesc* desc)
{
    CHECK_FAILED(__super::Initialize(desc), E_FAIL);

    auto curveDesc = static_cast<CurveModifierDesc*>(desc);
    _clip = curveDesc->clip;
    _poseWeight = curveDesc->poseWeight;
    _allowWeightOvershoot = curveDesc->allowWeightOvershoot;
    _playTimeline = curveDesc->playTimeline;
    _loopTimeline = curveDesc->loopTimeline;

    if (_clip != nullptr)
        _clips = { _clip };

    if (_clip != nullptr && !curveDesc->poseName.empty())
    {
        if (!Set_Pose(curveDesc->poseName))
            return E_FAIL;
    }
    else if (_clip != nullptr && curveDesc->poseIndex >= 0)
    {
        if (!Set_PoseIndex(curveDesc->poseIndex))
            return E_FAIL;
    }

    return S_OK;
}

void CurveAnimationModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
{
    if (!_isActive || model == nullptr || _clip == nullptr)
        return;

    CurveClip::CurveClip_Pose timelinePose{};
    const CurveClip::CurveClip_Pose* pose = nullptr;
    if (_playTimeline && _clip->Has_Timeline())
    {
        _timelineTimeSec += std::max(0.f, timeDelta);
        if (!_clip->Evaluate_Timeline(_timelineTimeSec, _loopTimeline, timelinePose))
            return;

        pose = &timelinePose;
    }
    else
    {
        if (_poseIndex < 0)
            return;

        pose = _clip->Get_Pose(static_cast<uint32>(_poseIndex));
    }

    if (pose == nullptr)
        return;

    const float effectiveWeight = _strengthAlpha * _poseWeight;
    if (std::abs(effectiveWeight) <= CURVE_WEIGHT_EPSILON)
        return;

    Begin_CurveEvaluation(model, *pose, effectiveWeight);

    for (const CurveClip::Curve& curve : pose->curves)
        Apply_Curve(model, curve, effectiveWeight);

    End_CurveEvaluation(model, *pose, effectiveWeight);
}

void CurveAnimationModifier::Set_CurveClip(const Shared<CurveClip>& clip)
{
    _clip = clip;
    _clips.clear();
    if (_clip != nullptr)
        _clips.push_back(_clip);

    _poseIndex = -1;
    _timelineTimeSec = 0.f;
}

bool CurveAnimationModifier::Set_Pose(const string& poseName)
{
    if (_clip == nullptr)
        return false;

    return Set_PoseIndex(_clip->Find_PoseIndex(poseName));
}

bool CurveAnimationModifier::Set_PoseIndex(int32 poseIndex)
{
    if (_clip == nullptr || poseIndex < 0)
        return false;

    if (_clip->Get_Pose(static_cast<uint32>(poseIndex)) == nullptr)
        return false;

    _poseIndex = poseIndex;
    return true;
}

HRESULT CurveAnimationModifier::Load_Preset(const string& presetPath)
{
    const fs::path presetFilePath = String::ToWString(presetPath);
    ifstream in(presetFilePath, ios::binary);
    if (!in.is_open())
        return E_FAIL;

    uint32 magic{}, version{}, clipCount{};
    in.read(reinterpret_cast<char*>(&magic), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&version), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&clipCount), sizeof(uint32));

    if (magic != CURVE_PRESET_MAGIC || version != CURVE_PRESET_VERSION)
        return E_FAIL;

    vector<Shared<CurveClip>> loadedClips{};
    loadedClips.reserve(clipCount);

    auto ReadString = [](ifstream& stream)
    {
        uint32 length{};
        stream.read(reinterpret_cast<char*>(&length), sizeof(uint32));

        string value;
        value.resize(length);
        if (length > 0)
            stream.read(value.data(), length);

        return value;
    };

    const fs::path presetDir = presetFilePath.parent_path();
    for (uint32 i = 0; i < clipCount; ++i)
    {
        fs::path clipPath = String::ToWString(ReadString(in));
        if (clipPath.is_relative())
            clipPath = presetDir / clipPath;

        auto clip = CurveClip::Create();
        CHECK_FAILED(clip->Load_Binary(String::ToString(clipPath.wstring())), E_FAIL);
        loadedClips.push_back(clip);
    }

    if (loadedClips.empty())
        return E_FAIL;

    _clips = std::move(loadedClips);
    _clip = _clips.front();
    _poseIndex = !_clip->Get_Poses().empty() ? 0 : -1;
    _playTimeline = _clip->Has_Timeline();
    _timelineTimeSec = 0.f;
    return (_poseIndex >= 0 || _playTimeline) ? S_OK : E_FAIL;
}

HRESULT CurveAnimationModifier::Load_PresetForModel(const string& modelPath)
{
    return Load_Preset(Build_PresetPathForModel(modelPath, _modifierTag));
}

HRESULT CurveAnimationModifier::Save_Preset(const string& presetPath, const vector<string>& clipPaths)
{
    if (clipPaths.empty())
        return E_FAIL;

    const fs::path presetFilePath = String::ToWString(presetPath);
    ofstream out(presetFilePath, ios::binary);
    if (!out.is_open())
        return E_FAIL;

    const uint32 clipCount = static_cast<uint32>(clipPaths.size());
    out.write(reinterpret_cast<const char*>(&CURVE_PRESET_MAGIC), sizeof(uint32));
    out.write(reinterpret_cast<const char*>(&CURVE_PRESET_VERSION), sizeof(uint32));
    out.write(reinterpret_cast<const char*>(&clipCount), sizeof(uint32));

    auto WriteString = [](ofstream& stream, const string& value)
    {
        const uint32 length = static_cast<uint32>(value.size());
        stream.write(reinterpret_cast<const char*>(&length), sizeof(uint32));
        if (length > 0)
            stream.write(value.c_str(), length);
    };

    const fs::path presetDir = presetFilePath.parent_path();
    for (const string& clipPathString : clipPaths)
    {
        const fs::path clipPath = String::ToWString(clipPathString);
        fs::path storedPath = clipPath;
        if (!presetDir.empty())
        {
            const fs::path relativePath = clipPath.lexically_relative(presetDir);
            if (!relativePath.empty())
                storedPath = relativePath;
        }

        WriteString(out, String::ToString(storedPath.wstring()));
    }

    return out.good() ? S_OK : E_FAIL;
}

string CurveAnimationModifier::Build_PresetPathForModel(const string& modelPath, const wstring& modifierTag)
{
    fs::path modelDir = String::ToWString(modelPath);
    if (modelDir.has_extension())
        modelDir = modelDir.parent_path();

    const fs::path presetPath = modelDir / (modifierTag + CURVE_PRESET_EXTENSION);
    return String::ToString(presetPath.wstring());
}

void CurveAnimationModifier::Begin_CurveEvaluation(ModelCom* model, const CurveClip::CurveClip_Pose& pose, float weight)
{
    UNREFERENCED_PARAMETER(pose);
    UNREFERENCED_PARAMETER(weight);

    if (model != nullptr)
        model->Reset_MorphWeights();
}

void CurveAnimationModifier::Apply_Curve(ModelCom* model, const CurveClip::Curve& curve, float weight)
{
    if (model == nullptr || curve.name.empty())
        return;

    const int32 targetIndex = model->Find_MorphTargetIndex(curve.name);
    if (targetIndex < 0)
        return;

    const float morphWeight = curve.value * weight;
    if (_allowWeightOvershoot)
    {
        model->Set_MorphWeight_Unclamped(static_cast<uint32>(targetIndex), morphWeight);
        return;
    }

    model->Set_MorphWeight(static_cast<uint32>(targetIndex), morphWeight);
}

void CurveAnimationModifier::End_CurveEvaluation(ModelCom* model, const CurveClip::CurveClip_Pose& pose, float weight)
{
    UNREFERENCED_PARAMETER(model);
    UNREFERENCED_PARAMETER(pose);
    UNREFERENCED_PARAMETER(weight);
}

Shared<CurveAnimationModifier> CurveAnimationModifier::Create(CurveModifierDesc* desc)
{
    auto instance = make_shared<CurveAnimationModifier>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END
