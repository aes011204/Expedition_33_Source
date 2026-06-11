#include "pch.h"
#include "CurveClip.h"

NS_BEGIN(Engine)

namespace
{
    string Read_BinaryString(ifstream& in)
    {
        uint32 length{};
        in.read(reinterpret_cast<char*>(&length), sizeof(uint32));

        string value;
        value.resize(length);
        if (length > 0)
            in.read(value.data(), length);

        return value;
    }

    CurveClip::Curve Read_BinaryCurve(ifstream& in)
    {
        CurveClip::Curve curve{};
        curve.name = Read_BinaryString(in);
        in.read(reinterpret_cast<char*>(&curve.value), sizeof(float));
        return curve;
    }

    CurveClip::CurveClip_Pose Read_BinaryPose(ifstream& in)
    {
        CurveClip::CurveClip_Pose pose{};
        pose.name = Read_BinaryString(in);

        uint32 curveCount{};
        in.read(reinterpret_cast<char*>(&curveCount), sizeof(uint32));
        pose.curves.reserve(curveCount);

        for (uint32 curveIndex = 0; curveIndex < curveCount; ++curveIndex)
            pose.curves.push_back(Read_BinaryCurve(in));

        return pose;
    }

    CurveClip::TimedCurve Read_BinaryTimedCurve(ifstream& in)
    {
        CurveClip::TimedCurve curve{};
        curve.name = Read_BinaryString(in);

        uint32 keyCount{};
        in.read(reinterpret_cast<char*>(&keyCount), sizeof(uint32));
        curve.keys.resize(keyCount);
        if (keyCount > 0)
            in.read(reinterpret_cast<char*>(curve.keys.data()), sizeof(CurveClip::CurveKey) * keyCount);

        return curve;
    }

    float Evaluate_Keys(const vector<CurveClip::CurveKey>& keys, float timeSec)
    {
        if (keys.empty())
            return 0.f;

        if (timeSec <= keys.front().timeSec)
            return keys.front().value;

        for (size_t i = 1; i < keys.size(); ++i)
        {
            const CurveClip::CurveKey& previous = keys[i - 1];
            const CurveClip::CurveKey& current = keys[i];
            if (timeSec > current.timeSec)
                continue;

            const float span = current.timeSec - previous.timeSec;
            if (std::abs(span) <= 0.000001f)
                return current.value;

            const float alpha = (timeSec - previous.timeSec) / span;
            return previous.value + (current.value - previous.value) * alpha;
        }

        return keys.back().value;
    }
}

HRESULT CurveClip::Set_Data(string sourceName, const string& sourcePath, vector<CurveClip_Pose>&& poses)
{
    Clear();

    _sourceName = std::move(sourceName);
    _sourcePath = sourcePath;
    _filePath.clear();
    _poses = std::move(poses);

    Rebuild_PoseLookup();
    return !_poses.empty() ? S_OK : E_FAIL;
}

HRESULT CurveClip::Set_TimelineData(
    string sourceName,
    const string& sourcePath,
    vector<CurveClip_Pose>&& poses,
    vector<TimedCurve>&& timedCurves,
    float durationSec)
{
    Clear();

    _sourceName = std::move(sourceName);
    _sourcePath = sourcePath;
    _filePath.clear();
    _poses = std::move(poses);
    _timedCurves = std::move(timedCurves);
    _durationSec = std::max(0.f, durationSec);

    Rebuild_PoseLookup();
    return (!_poses.empty() || Has_Timeline()) ? S_OK : E_FAIL;
}

HRESULT CurveClip::Load_Binary(const string& filePath)
{
    Clear();

    ifstream in(fs::path(String::ToWString(filePath)), ios::binary);
    if (!in.is_open())
        return E_FAIL;

    uint32 version{}, poseCount{};
    if (!Read_BinaryHeader(in, version, poseCount))
        return E_FAIL;

    _sourceName = Read_BinaryString(in);
    _sourcePath = Read_BinaryString(in);
    _filePath = filePath;

    if (!Read_BinaryBody(in, version, poseCount))
    {
        Clear();
        return E_FAIL;
    }

    Rebuild_PoseLookup();
    return !_poses.empty() ? S_OK : E_FAIL;
}

void CurveClip::Clear()
{
    _sourceName.clear();
    _sourcePath.clear();
    _filePath.clear();
    _poses.clear();
    _timedCurves.clear();
    _durationSec = 0.f;
    _poseLookup.clear();
}

const CurveClip::CurveClip_Pose* CurveClip::Get_Pose(uint32 poseIndex) const
{
    if (poseIndex >= _poses.size())
        return nullptr;

    return &_poses[poseIndex];
}

const CurveClip::CurveClip_Pose* CurveClip::Find_Pose(const string& poseName) const
{
    const int32 poseIndex = Find_PoseIndex(poseName);
    return poseIndex >= 0 ? Get_Pose(static_cast<uint32>(poseIndex)) : nullptr;
}

int32 CurveClip::Find_PoseIndex(const string& poseName) const
{
    const auto iter = _poseLookup.find(poseName);
    if (iter == _poseLookup.end())
        return -1;

    return static_cast<int32>(iter->second);
}

bool CurveClip::Evaluate_Timeline(float timeSec, bool loop, CurveClip_Pose& outPose) const
{
    if (!Has_Timeline())
        return false;

    float evalTime = timeSec;
    if (loop && _durationSec > 0.f)
    {
        evalTime = std::fmod(evalTime, _durationSec);
        if (evalTime < 0.f)
            evalTime += _durationSec;
    }
    else
    {
        evalTime = std::clamp(evalTime, 0.f, _durationSec);
    }

    outPose.name = _sourceName + "_Timeline";
    outPose.curves.clear();
    outPose.curves.reserve(_timedCurves.size());

    for (const TimedCurve& curve : _timedCurves)
    {
        if (curve.name.empty())
            continue;

        const float value = Evaluate_Keys(curve.keys, evalTime);
        if (std::abs(value) <= 0.000001f)
            continue;

        outPose.curves.push_back(Curve{ curve.name, value });
    }

    return true;
}

void CurveClip::Rebuild_PoseLookup()
{
    _poseLookup.clear();
    for (uint32 i = 0; i < _poses.size(); ++i)
    {
        if (!_poses[i].name.empty())
            _poseLookup.emplace(_poses[i].name, i);
    }
}

bool CurveClip::Read_BinaryHeader(ifstream& in, uint32& outVersion, uint32& outPoseCount) const
{
    uint32 magic{};
    in.read(reinterpret_cast<char*>(&magic), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&outVersion), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&outPoseCount), sizeof(uint32));

    return in.good() &&
        magic == CurveClipFormat::MAGIC &&
        (outVersion == CurveClipFormat::VERSION_POSE_ONLY ||
            outVersion == CurveClipFormat::VERSION);
}

bool CurveClip::Read_BinaryBody(ifstream& in, uint32 version, uint32 poseCount)
{
    _poses.reserve(poseCount);
    for (uint32 poseIndex = 0; poseIndex < poseCount; ++poseIndex)
        _poses.push_back(Read_BinaryPose(in));

    if (version >= CurveClipFormat::VERSION)
    {
        uint32 timedCurveCount{};
        in.read(reinterpret_cast<char*>(&_durationSec), sizeof(float));
        in.read(reinterpret_cast<char*>(&timedCurveCount), sizeof(uint32));

        _timedCurves.reserve(timedCurveCount);
        for (uint32 curveIndex = 0; curveIndex < timedCurveCount; ++curveIndex)
            _timedCurves.push_back(Read_BinaryTimedCurve(in));
    }

    return in.good() || in.eof();
}

Shared<CurveClip> CurveClip::Create()
{
    return make_shared<CurveClip>();
}

void CurveClip::Free()
{
    Clear();
    Base::Free();
}

NS_END
