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

HRESULT CurveClip::Load_Binary(const string& filePath)
{
    Clear();

    ifstream in(fs::path(String::ToWString(filePath)), ios::binary);
    if (!in.is_open())
        return E_FAIL;

    uint32 poseCount{};
    if (!Read_BinaryHeader(in, poseCount))
        return E_FAIL;

    _sourceName = Read_BinaryString(in);
    _sourcePath = Read_BinaryString(in);
    _filePath = filePath;

    if (!Read_BinaryBody(in, poseCount))
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

void CurveClip::Rebuild_PoseLookup()
{
    _poseLookup.clear();
    for (uint32 i = 0; i < _poses.size(); ++i)
    {
        if (!_poses[i].name.empty())
            _poseLookup.emplace(_poses[i].name, i);
    }
}

bool CurveClip::Read_BinaryHeader(ifstream& in, uint32& outPoseCount) const
{
    uint32 magic{}, version{};
    in.read(reinterpret_cast<char*>(&magic), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&version), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&outPoseCount), sizeof(uint32));

    return in.good() &&
        magic == CurveClipFormat::MAGIC &&
        version == CurveClipFormat::VERSION;
}

bool CurveClip::Read_BinaryBody(ifstream& in, uint32 poseCount)
{
    _poses.reserve(poseCount);
    for (uint32 poseIndex = 0; poseIndex < poseCount; ++poseIndex)
        _poses.push_back(Read_BinaryPose(in));

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
