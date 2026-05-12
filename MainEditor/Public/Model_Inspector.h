#pragma once

#include "Component_Inspector.h"
#include <filesystem>

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(MainEditor)

class ModelCom_Inspector final : public Component_Inspector
{
public:
    ModelCom_Inspector() = default;
    ~ModelCom_Inspector() override = default;

public:
    void Draw_Inspector(const Shared<Component>& component) override;

private:
    struct AnimPreviewState
    {
        int32 selectedAnimIndex = -1;
        bool loop = true;
        float blendDuration = 0.1f;
    };

private:
    static string Build_ModelPickerLabel(const AssetMeta& assetMeta);
    static bool Is_ModelAssetOfType(const AssetMeta& assetMeta, Engine::ModelType expectedModelType);
    static void Draw_ModelPicker(const Shared<Engine::ModelCom>& model);
    static const char* Get_TextureTypeLabel(uint32 typeIndex);
    static string To_DisplayTexturePath(const wstring& resolvedPath);
    static bool Is_TextureExtension(const fs::path& path);
    static vector<fs::path> Collect_TextureCandidates(const wstring& resolvedPath, const wstring& sourceModelPath);
    static ID3D11ShaderResourceView* Get_TexturePreviewSRV(const fs::path& texturePath);
    static AnimPreviewState& Get_AnimPreviewState(const Engine::ModelCom* model);
    static void Draw_AnimationSection(const Shared<Engine::ModelCom>& model);

public:
    static Shared<ModelCom_Inspector> Create();
};

NS_END
