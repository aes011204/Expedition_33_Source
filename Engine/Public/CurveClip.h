#pragma once
#include "Base.h"

NS_BEGIN(Engine)

namespace CurveClipFormat
{
    inline constexpr uint32 MAGIC = 0x504C4343; // 'CCLP'
    inline constexpr uint32 VERSION = 1;
    inline constexpr char EXTENSION[] = ".curveclip";
    inline constexpr wchar_t EXTENSION_W[] = L".curveclip";
}

class ENGINE_DLL CurveClip final : public Base
{
public:
    struct Curve
    {
        string name{};
        float value{};
    };

    struct CurveClip_Pose
    {
        string name{};
        vector<Curve> curves{};
    };

public:
    CurveClip() = default;
    ~CurveClip() override = default;

public:
    HRESULT Set_Data(string sourceName, const string& sourcePath, vector<CurveClip_Pose>&& poses);
    HRESULT Load_Binary(const string& filePath);
    void Clear();

    const string& Get_SourceName() const { return _sourceName; }
    const string& Get_SourcePath() const { return _sourcePath; }
    const string& Get_FilePath() const { return _filePath; }
    const vector<CurveClip_Pose>& Get_Poses() const { return _poses; }
    const CurveClip_Pose* Get_Pose(uint32 poseIndex) const;
    const CurveClip_Pose* Find_Pose(const string& poseName) const;
    int32 Find_PoseIndex(const string& poseName) const;

private: /* Lookup */
    void Rebuild_PoseLookup();

private: /* Binary IO */
    bool Read_BinaryHeader(ifstream& in, uint32& outPoseCount) const;
    bool Read_BinaryBody(ifstream& in, uint32 poseCount);

private:
    string _sourceName{};
    string _sourcePath{};
    string _filePath{};
    vector<CurveClip_Pose> _poses{};
    umap<string, uint32> _poseLookup{};

public:
    static Shared<CurveClip> Create();
    void Free() override;
};

NS_END
