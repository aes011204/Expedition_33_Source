#pragma once

#include "Base.h"

#include "Material_Define.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

    class ModelCom;

// .material.json 전용 런타임 로더/인덱서
// - MaterialsRoot 아래 *.material.json을 재귀 스캔해서 stem->path 맵만 구성한다.
// - ModelCom이 model stem으로 query하여, 존재하면 해당 번들을 파싱해 Material에 적용한다.
class ENGINE_DLL MaterialBuilder final
{
public:
    MaterialBuilder();
    ~MaterialBuilder();

public:
    static Unique<MaterialBuilder> Create(const fs::path& materialsRoot);

    void Initialize(const fs::path& materialsRoot);

    const fs::path* FindBundlePath(std::string_view key) const;
    bool Try_ApplyToModelByKey(ModelCom& model, std::string_view key) const;

private:
    struct BundleInstance
    {
        MaterialRenderState renderState{};
        MaterialCB cb{};

        bool baseColorOverride = false;
        bool normalOverride = false;
        bool ormOverride = false;
        bool emissiveOverride = false;
        bool opacityMaskOverride = false;

        string baseColorRel{};
        string normalRel{};
        string ormRel{};
        string emissiveRel{};
        string opacityMaskRel{};
    };

private:
    static bool Try_ParseVec2(const json& value, Vec2& out);
    static bool Try_ParseVec4(const json& value, Vec4& out);
    static bool Try_ParseRenderState(const json& value, MaterialRenderState& out);
    static bool Try_ParseCB(const json& value, MaterialCB& out);
    static fs::path To_AbsoluteTexturePath(const fs::path& resourceRoot, const string& relGeneric);

    static bool Is_MaterialBundleFile(const fs::path& path);
    static string To_Key(const fs::path& path);

private:
    fs::path _root{};
    unordered_map<string, fs::path> _keyToBundlePath{};
};

NS_END
