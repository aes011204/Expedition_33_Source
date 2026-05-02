#include "pch.h"
#include "ModelConverter.h"


ModelConverter::ModelConverter() : EditorBlock{} {}

HRESULT ModelConverter::Initialize()
{
    importFlags = aiProcess_ConvertToLeftHanded |
                  aiProcess_Triangulate |
                  aiProcess_CalcTangentSpace |
                  aiProcess_GenSmoothNormals |
                  aiProcess_PopulateArmatureData;

    return S_OK;
}

void ModelConverter::Update(float timeDelta)
{
    
}

void ModelConverter::Render()
{
    
}

HRESULT ModelConverter::Convert_Model(const wstring& fbxFilePath, const wstring& outputDir)
{
    _aiScene = nullptr;
    Assimp::Importer importer{};

    string utf8FilePath = string(fbxFilePath.begin(), fbxFilePath.end());
    _aiScene = importer.ReadFile(utf8FilePath, importFlags);

    if (nullptr == _aiScene || _aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !_aiScene->mRootNode)
    {
        LOG_ERROR("Assimp Error: {} \n", importer.GetErrorString());
        return E_FAIL;
    }

    string modelName = Get_FileNameWithoutExtension(fbxFilePath);

    if (FAILED(Export_MaterialData(_aiScene, outputDir, modelName)))
        return E_FAIL;

    if (FAILED(Export_MeshData(_aiScene, outputDir, modelName)))
        return E_FAIL;

    if (_aiScene->HasAnimations())
    {
        if (FAILED(Export_AnimationData(_aiScene, outputDir, modelName)))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT ModelConverter::Export_MaterialData(const aiScene* aiScene, const wstring& outputDir, const string& modelName)
{
    wstring matDir = outputDir + L"Matertial/";
    Ensure_DirectoryExists(matDir);
}

HRESULT ModelConverter::Export_MeshData(const aiScene* aiScene, const wstring& outputDir, const string& modelName)
{
}

HRESULT ModelConverter::Export_AnimationData(const aiScene* aiScene, const wstring& outputDir, const string& modelName)
{
}

Matrix ModelConverter::Convert_Matrix(const aiMatrix4x4& aiMat) const
{
    Matrix mat;
    mat._11 = aiMat.a1; mat._12 = aiMat.b1; mat._13 = aiMat.c1; mat._14 = aiMat.d1;
    mat._21 = aiMat.a2; mat._22 = aiMat.b2; mat._23 = aiMat.c2; mat._24 = aiMat.d2;
    mat._31 = aiMat.a3; mat._32 = aiMat.b3; mat._33 = aiMat.c3; mat._34 = aiMat.d3;
    mat._41 = aiMat.a4; mat._42 = aiMat.b4; mat._43 = aiMat.c4; mat._44 = aiMat.d4;

    return mat;
}

string ModelConverter::Get_FileNameWithoutExtension(const wstring& filePath) const
{
    fs::path path(filePath);
    return path.stem().string();
}

void ModelConverter::Ensure_DirectoryExists(const wstring& dirPath) const
{
    if (!fs::exists(dirPath))
    {
        fs::create_directories(dirPath);
    }
}

void ModelConverter::Extract_BoneWeights(const aiMesh* aiMesh, vector<vector<pair<uint32, float>>>& outVertexWeights, unordered_map<string, uint32>& outBoneNameToIndex)
{

}

Shared<ModelConverter> ModelConverter::Create()
{
    auto prototype = make_shared<ModelConverter>();

    if (FAILED(prototype->Initialize()))
    {
        MSG_BOX("Failed to Created : ModelConverter");
        return nullptr;
    }

    return prototype;
}

