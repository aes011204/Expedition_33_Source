#pragma once
#include "EditorBlock.h"

namespace tinygltf
{
    class Model;
}

NS_BEGIN(ModelEditor)

class GltfMorphBuilder final : public EditorBlock
{
private:
    struct ModelMeshHeader
    {
        string name;
        uint32 materialIndex{};
        uint32 vertexCount{};
        vector<Vec3> positions;
    };

    struct PrimitiveMorphSource
    {
        uint32 primitiveIndex{};
        uint32 materialIndex{};
        uint32 vertexCount{};
        vector<uint32> targetIndices;
        vector<Vec3> basePositions;
        vector<vector<Vec3>> positionDeltas;
        vector<vector<Vec3>> normalDeltas;
    };

public:
    GltfMorphBuilder();
    ~GltfMorphBuilder() override = default;

private:
    HRESULT Initialize() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private:
    void Render_PathRow(const char* label, char* buffer, size_t bufferSize, const wchar_t* filter);
    void Set_Status(string status) const;
    bool Browse_File(const wchar_t* filter, char* outBuffer, size_t bufferSize) const;

private:
    HRESULT Build_MorphFile();
    HRESULT Validate_InputPaths() const;
    HRESULT Load_ModelMeshHeaders(const fs::path& modelPath, vector<ModelMeshHeader>& outMeshes) const;
    HRESULT Load_GlbMorphSources(
        const fs::path& glbPath,
        vector<string>& outTargetNames,
        vector<PrimitiveMorphSource>& outSources) const;
    HRESULT Match_AndBuildMorphData(
        const vector<ModelMeshHeader>& modelMeshes,
        const vector<string>& targetNames,
        const vector<PrimitiveMorphSource>& glbSources,
        MorphData& outData) const;
    HRESULT Export_MorphBinary(const fs::path& outputPath, const MorphData& data) const;
    HRESULT Read_GltfVec3Accessor(const tinygltf::Model& model, int accessorIndex, vector<Vec3>& outValues) const;

private:
    static string Read_String(ifstream& in);
    static void Write_String(ofstream& out, const string& value);
    static bool Is_NearlyZero(const Vec3& value, float epsilon);
    static Vec3 Transform_GlbPosition(const Vec3& value);
    static Vec3 Transform_GlbDeltaPosition(const Vec3& value);
    static Vec3 Transform_GlbNormal(const Vec3& value);
    static string Make_PositionKey(const Vec3& value, float scale);
    static void Extract_GlbTargetNames(const tinygltf::Model& model, vector<string>& outTargetNames);
    static uint32 Get_GltfTypeElementCount(int type);

    template<typename T>
    static void Read_Raw(ifstream& in, T& value)
    {
        in.read(reinterpret_cast<char*>(&value), sizeof(T));
    }

    template<typename T>
    static void Write_Raw(ofstream& out, const T& value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

private:
    char _modelPathBuffer[MAX_PATH]{};
    char _glbPathBuffer[MAX_PATH]{};
    mutable string _status{ "Select .model and .glb." };
    bool _isBuilding{ false };

public:
    static Shared<GltfMorphBuilder> Create();
};

NS_END
