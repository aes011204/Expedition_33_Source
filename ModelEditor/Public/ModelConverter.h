#pragma once
#include "EditorBlock.h"

class ModelConverter final : public EditorBlock
{
public:
    ModelConverter();
    ~ModelConverter() override = default;

private:
    HRESULT Initialize() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

public:
    HRESULT Convert_Model(const wstring& fbxFilePath, const wstring& outputDir);

private:
    HRESULT Export_MaterialData(const aiScene * aiScene, const wstring & outputDir, const string& modelName);
    HRESULT Export_MeshData(const aiScene * aiScene, const wstring & outputDir, const string& modelName);
    HRESULT Export_AnimationData(const aiScene * aiScene, const wstring & outputDir, const string& modelName);

private:
    Matrix  Convert_Matrix(const aiMatrix4x4 & aiMat) const;
    string  Get_FileNameWithoutExtension(const wstring & filePath) const;
    void    Ensure_DirectoryExists(const wstring& dirPath) const;
    void    Extract_BoneWeights(const aiMesh * aiMesh, vector<vector<pair<uint32, float>>>&outVertexWeights, unordered_map<string, uint32>&outBoneNameToIndex);

private:
    uint32            importFlags{ 0 };
    const aiScene*    _aiScene{ nullptr };

    Shared<Assimp::Importer>    _importer;
    const aiScene*              _scene{ nullptr };
    bool                        _isSkeletal{ false };

    vector<ModelNodeData>               _nodes;
    vector<ModelMeshData>               _meshHeaders;
    vector<vector<VTXMESH>>             _staticVerts;
    vector<vector<VTXANIMMESH>>         _animVerts;
    vector<vector<uint32>>              _indices;

    vector<vector<uint32>>              _meshBoneRefs;
    vector<vector<Matrix>>              _meshOffsets;

    vector<ModelMaterialData>           _matHeaders;
    vector<vector<ModelTextureSlot>>    _matSlots;

    vector<ModelBoneData>               _bones;
    unordered_map<string, uint32>       _boneNameMap;

    vector<ModelAnimationData>           _animHeaders;

    vector<vector<ModelChannelData>>    _channelHeaders;
    vector<vector<vector<KEYFRAME>>>    _keyframes;

public:
    static Shared<ModelConverter> Create();

};

