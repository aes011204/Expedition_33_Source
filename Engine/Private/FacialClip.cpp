#include "pch.h"
#include "FacialClip.h"

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

    float Evaluate_FloatKeys(const vector<FacialClip::FloatKey>& keys, float timeSec)
    {
        if (keys.empty())
            return 0.f;

        if (timeSec <= keys.front().timeSec)
            return keys.front().value;

        for (size_t i = 1; i < keys.size(); ++i)
        {
            const FacialClip::FloatKey& previous = keys[i - 1];
            const FacialClip::FloatKey& current = keys[i];
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

    FacialClip::TransformKey Evaluate_TransformKeys(const vector<FacialClip::TransformKey>& keys, float timeSec)
    {
        if (keys.empty())
            return FacialClip::TransformKey{};

        if (timeSec <= keys.front().timeSec)
            return keys.front();

        for (size_t i = 1; i < keys.size(); ++i)
        {
            const FacialClip::TransformKey& previous = keys[i - 1];
            const FacialClip::TransformKey& current = keys[i];
            if (timeSec > current.timeSec)
                continue;

            const float span = current.timeSec - previous.timeSec;
            if (std::abs(span) <= 0.000001f)
                return current;

            const float alpha = (timeSec - previous.timeSec) / span;
            FacialClip::TransformKey result{};
            result.timeSec = timeSec;
            result.position = Vec3::Lerp(previous.position, current.position, alpha);
            result.rotation = Quat::Slerp(previous.rotation, current.rotation, alpha);
            result.scale = Vec3::Lerp(previous.scale, current.scale, alpha);
            return result;
        }

        return keys.back();
    }
}

HRESULT FacialClip::Load_Binary(const string& filePath)
{
    Clear();

    ifstream in(fs::path(String::ToWString(filePath)), ios::binary);
    if (!in.is_open())
        return E_FAIL;

    uint32 magic{}, version{}, flags{}, morphTrackCount{}, boneTrackCount{};
    in.read(reinterpret_cast<char*>(&magic), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&version), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&flags), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&_durationSec), sizeof(float));
    in.read(reinterpret_cast<char*>(&morphTrackCount), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&boneTrackCount), sizeof(uint32));

    if (!in.good() ||
        magic != FacialClipFormat::MAGIC ||
        version != FacialClipFormat::VERSION)
    {
        Clear();
        return E_FAIL;
    }

    _filePath = filePath;
    _additiveLocal = (flags & FacialClipFormat::FLAG_ADDITIVE_LOCAL) != 0;
    _sourceName = Read_BinaryString(in);

    _morphTracks.reserve(morphTrackCount);
    for (uint32 trackIndex = 0; trackIndex < morphTrackCount; ++trackIndex)
    {
        MorphTrack track{};
        track.name = Read_BinaryString(in);

        uint32 keyCount{};
        in.read(reinterpret_cast<char*>(&keyCount), sizeof(uint32));
        track.keys.resize(keyCount);
        for (FloatKey& key : track.keys)
        {
            in.read(reinterpret_cast<char*>(&key.timeSec), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.value), sizeof(float));
        }

        _morphTracks.push_back(std::move(track));
    }

    _boneTracks.reserve(boneTrackCount);
    for (uint32 trackIndex = 0; trackIndex < boneTrackCount; ++trackIndex)
    {
        BoneTrack track{};
        track.name = Read_BinaryString(in);

        uint32 keyCount{};
        in.read(reinterpret_cast<char*>(&keyCount), sizeof(uint32));
        track.keys.resize(keyCount);
        for (TransformKey& key : track.keys)
        {
            in.read(reinterpret_cast<char*>(&key.timeSec), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.position.x), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.position.y), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.position.z), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.rotation.x), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.rotation.y), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.rotation.z), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.rotation.w), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.scale.x), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.scale.y), sizeof(float));
            in.read(reinterpret_cast<char*>(&key.scale.z), sizeof(float));
        }

        _boneTracks.push_back(std::move(track));
    }

    if (!in.good() && !in.eof())
    {
        Clear();
        return E_FAIL;
    }

    return (_durationSec > 0.f && (!_morphTracks.empty() || !_boneTracks.empty())) ? S_OK : E_FAIL;
}

void FacialClip::Clear()
{
    _sourceName.clear();
    _filePath.clear();
    _durationSec = 0.f;
    _additiveLocal = false;
    _morphTracks.clear();
    _boneTracks.clear();
}

bool FacialClip::Evaluate_MorphTrack(const MorphTrack& track, float timeSec, bool loop, float& outValue) const
{
    if (track.keys.empty())
        return false;

    outValue = Evaluate_FloatKeys(track.keys, Normalize_Time(timeSec, loop));
    return true;
}

bool FacialClip::Evaluate_BoneTrack(const BoneTrack& track, float timeSec, bool loop, TransformKey& outKey) const
{
    if (track.keys.empty())
        return false;

    outKey = Evaluate_TransformKeys(track.keys, Normalize_Time(timeSec, loop));
    return true;
}

float FacialClip::Normalize_Time(float timeSec, bool loop) const
{
    if (_durationSec <= 0.f)
        return 0.f;

    if (!loop)
        return std::clamp(timeSec, 0.f, _durationSec);

    float normalized = std::fmod(timeSec, _durationSec);
    if (normalized < 0.f)
        normalized += _durationSec;

    return normalized;
}

Shared<FacialClip> FacialClip::Create()
{
    return make_shared<FacialClip>();
}

void FacialClip::Free()
{
    Clear();
    Base::Free();
}

NS_END
