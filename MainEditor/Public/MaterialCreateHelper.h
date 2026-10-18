#pragma once
#include "Material_BundleTypes.h"

NS_BEGIN(MainEditor)

// UI 쪽 코드를 너무 비대해져서, json 파싱과 텍스처 resolve를 여기로 모아뒀다
class MaterialCreateHelper final
{
public:
    MaterialCreateHelper(
        const wstring& miRoot,
        const wstring& textureSearchRoot,
        unordered_map<string, wstring>* textureIndex,
        bool* textureIndexDirty);

public:
    // mapping src를 읽어 참조 중인 M, MI* 목록을 만든다
    bool Load_SkFile(const wstring& skFilePath, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>& inOutHeuristics);
    bool Load_SmFile(const wstring& smFilePath, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>& inOutHeuristics);
    bool Load_FxFile(const wstring& fxFilePath, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>& inOutHeuristics);

    // mapping에 등장한 MI만 대상으로 MI json을 읽고 최종 머테리얼 데이터를 만든다
    bool Build_MaterialInstances(
        const vector<MaterialMapping>& mappings,
        unordered_map<string, MaterialInstanceData>& inOutInstances,
        vector<HeuristicNote>& inOutHeuristics);

public:

    static fs::path Try_ResolveDirectoryPath(const wstring& userPath);

private:
    // json 파일을 통째로 읽는다
    static bool Try_LoadJson(const fs::path& filePath, json& out);
    static string Pick_TextureUAssetPath(const json& textures, initializer_list<const char*> keys, string* outUsedKey = nullptr);

    bool Load_MaterialRefs_FromSkJson(const fs::path& skPath, const json& root, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>& inOutHeuristics);
    bool Load_MaterialRefs_FromSmJson(const fs::path& smPath, const json& root, vector<MaterialMapping>& inOutMappings);
    bool Load_MaterialRefs_FromFxJson(const fs::path& fxPath, const json& root, vector<MaterialMapping>& inOutMappings);

    bool Load_MiJsonByName(const string& assetName, json& outMiRoot, vector<HeuristicNote>* inOutHeuristics = nullptr) const;

    void Rebuild_TextureIndexIfNeeded();

    // uasset 경로 문자열을 받아서 실제 이미지 파일 경로로 바꾼다
    wstring Resolve_TextureFileByUAsset(const string& uassetPath);
    // base name을 받아서 실제 이미지 파일 경로로 바꾼다
    wstring Resolve_TextureFileByBaseName(const string& baseName);

    // 확장자만 보고 텍스처 파일인지 대충 걸러낸다
    static bool   Is_TextureExtension(const fs::path& path);

private:
    wstring _miRoot{};
    wstring _textureSearchRoot{};
    unordered_map<string, wstring>* _textureIndex = nullptr;
    bool* _textureIndexDirty = nullptr;
};

NS_END
