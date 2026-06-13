#pragma once
#include "Base.h"

NS_BEGIN(Engine)

namespace FacialClipFormat
{
    inline constexpr uint32 MAGIC = 0x50434C46; // 'FLCP'
    inline constexpr uint32 VERSION = 1;
    inline constexpr uint32 FLAG_ADDITIVE_LOCAL = 1u << 0;
    inline constexpr wchar_t EXTENSION_W[] = L".facialclipbin";
}

class ENGINE_DLL FacialClip final : public Base
{
public:
    struct FloatKey
    {
        float timeSec{};
        float value{};
    };

    struct TransformKey
    {
        float timeSec{};
        Vec3 position{};
        Quat rotation{ Quat::Identity };
        Vec3 scale{ Vec3::One };
    };

    struct MorphTrack
    {
        string name{};
        vector<FloatKey> keys{};
    };

    struct BoneTrack
    {
        string name{};
        vector<TransformKey> keys{};
    };

public:
    FacialClip() = default;
    ~FacialClip() override = default;

public:
    HRESULT Load_Binary(const string& filePath);
    void Clear();

    const string& Get_SourceName() const { return _sourceName; }
    const string& Get_FilePath() const { return _filePath; }
    float Get_DurationSec() const { return _durationSec; }
    bool Is_AdditiveLocal() const { return _additiveLocal; }
    const vector<MorphTrack>& Get_MorphTracks() const { return _morphTracks; }
    const vector<BoneTrack>& Get_BoneTracks() const { return _boneTracks; }

    bool Evaluate_MorphTrack(const MorphTrack& track, float timeSec, bool loop, float& outValue) const;
    bool Evaluate_BoneTrack(const BoneTrack& track, float timeSec, bool loop, TransformKey& outKey) const;

private:
    float Normalize_Time(float timeSec, bool loop) const;

private:
    string _sourceName{};
    string _filePath{};
    float _durationSec{};
    bool _additiveLocal{ false };
    vector<MorphTrack> _morphTracks{};
    vector<BoneTrack> _boneTracks{};

public:
    static Shared<FacialClip> Create();
    void Free() override;
};

NS_END
