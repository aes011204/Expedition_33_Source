#pragma once
#include "MapEditorDefines.h"
#include "SceneContext.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(MapEditor)

class MEMaterialApplicator
{
public:
    struct Material_Desc
    {
        SceneContext* scene = {};
    };

public:
    MEMaterialApplicator() = default;
    ~MEMaterialApplicator() = default;

public:
    HRESULT Initialize(const Material_Desc& desc);

private:
    SceneContext* _scene = {};

public:
    void Apply_MaterialFromJson(const fs::path& modelPath, const string& bundleKey, Shared<ModelCom>& model);
    string Resolve_BundleKey(const fs::path& modelPath) const;
    void Apply_PassOverrideToModel(ModelCom& model, const string& modelFilename, Vec3 worldPos = Vec3(0.f, 0.f, 0.f), const string& fullModelPath = "");
    void Apply_PassOverrides();

public:
    static Unique<MEMaterialApplicator> Create(const Material_Desc& desc);
};

NS_END
