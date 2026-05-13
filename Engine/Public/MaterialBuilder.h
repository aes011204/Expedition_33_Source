#pragma once

#include "Base.h"

#include "Material_Define.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

// forward decl
class ModelCom;

// .material.json 전용 런타임 로더/인덱서
// - MaterialsRoot 아래 *.material.json을 재귀 스캔해서 stem->path 맵만 구성한다.
// - 번들은 "자산(base)"로만 캐싱하며, 실제 Runtime Material 적용은 ModelCom이 이벤트 기반으로 수행한다.
class ENGINE_DLL MaterialBuilder final
{
public:
    MaterialBuilder();
    ~MaterialBuilder();

public:
    static Unique<MaterialBuilder> Create(const fs::path& materialsRoot);

    void Initialize(const fs::path& materialsRoot);

    const fs::path* FindBundlePath(std::string_view key) const;
    struct BundleAsset;
    const BundleAsset* GetOrLoadBundle(std::string_view bundleKey);

    // ModelCom prefab/json의 material_bindings 섹션을 읽어서, ModelCom의 binding 구조를 채운다.
    // (texture_guid는 여기서 Resolve_AssetPath까지 처리한다)
    bool Try_ApplyMaterialBindingsFromJson(ModelCom& model, const json& data) const;

    // legacy: material_overrides[]를 instance override로 흡수한다.
    bool Try_ApplyModelMaterialOverridesFromJson(ModelCom& model, const json& data) const;

private:
    struct BaseInstanceData
    {
        MaterialRenderState renderState{};
        MaterialCB cb{};

        bool baseColorOverride = false;
        bool normalOverride = false;
        bool ormOverride = false;
        bool emissiveOverride = false;
        bool opacityMaskOverride = false;

        wstring baseColorPath{};
        wstring normalPath{};
        wstring ormPath{};
        wstring emissivePath{};
        wstring opacityMaskPath{};
    };

public:
    struct BundleAsset
    {
        unordered_map<uint32, string> materialIndexToAssetName{};
        unordered_map<string, BaseInstanceData> instancesByAssetName{};
    };

private:
    static bool ParseBool(const json& value, bool fallback);
    static bool Try_ParseVec2(const json& value, Vec2& out);
    static bool Try_ParseVec4(const json& value, Vec4& out);
    static bool Try_ParseRenderState(const json& value, MaterialRenderState& out);
    static bool Try_ParseCB(const json& value, MaterialCB& out);
    static fs::path To_AbsoluteTexturePath(const fs::path& resourceRoot, const string& relGeneric);

    static bool Is_MaterialBundleFile(const fs::path& path);
    static string To_Key(const fs::path& path);
    bool Try_LoadBundleAsset(const fs::path& bundlePath, BundleAsset& outAsset) const;

private:
    fs::path _root{};
    unordered_map<string, fs::path> _keyToBundlePath{};
    unordered_map<string, Unique<BundleAsset>> _bundleCache{};
};

NS_END
