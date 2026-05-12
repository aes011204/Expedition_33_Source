#include "MaterialBuilder.h"

#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"

MaterialBuilder::MaterialBuilder()
{
}

MaterialBuilder::~MaterialBuilder()
{
}

Unique<MaterialBuilder> MaterialBuilder::Create(const fs::path& materialsRoot)
{
    Unique<MaterialBuilder> instance(new MaterialBuilder());
    instance->Initialize(materialsRoot);
    return instance;
}

bool MaterialBuilder::Try_ParseVec2(const json& value, Vec2& out)
{
    if (!value.is_array() || value.size() < 2)
        return false;
    out.x = value[0].get<float>();
    out.y = value[1].get<float>();
    return true;
}

bool MaterialBuilder::Try_ParseVec4(const json& value, Vec4& out)
{
    if (!value.is_array() || value.size() < 4)
        return false;
    out.x = value[0].get<float>();
    out.y = value[1].get<float>();
    out.z = value[2].get<float>();
    out.w = value[3].get<float>();
    return true;
}

bool MaterialBuilder::Try_ParseRenderState(const json& value, MaterialRenderState& out)
{
    if (!value.is_object())
        return false;

    out.twoSided = value.value("twoSided", out.twoSided);

    const uint32 blendMode = value.value("blendMode", static_cast<uint32>(out.blendMode));
    if (blendMode < static_cast<uint32>(MaterialBlendMode::END))
        out.blendMode = static_cast<MaterialBlendMode>(blendMode);

    out.opacityMaskClipValue = value.value("opacityMaskClipValue", out.opacityMaskClipValue);

    // alphaClip은 번들에 없을 수도 있다. (template/블렌드로 유도 가능)
    out.alphaClip = value.value("alphaClip", out.alphaClip);
    return true;
}

bool MaterialBuilder::Try_ParseCB(const json& value, MaterialCB& out)
{
    if (!value.is_object())
        return false;

    if (value.contains("baseColorTint"))
        (void)Try_ParseVec4(value["baseColorTint"], out.baseColorTint);
    if (value.contains("emissiveColor"))
        (void)Try_ParseVec4(value["emissiveColor"], out.emissiveColor);

    out.emissiveIntensity = value.value("emissiveIntensity", out.emissiveIntensity);
    out.emissiveMaskMul = value.value("emissiveMaskMul", out.emissiveMaskMul);
    out.normalStrength = value.value("normalStrength", out.normalStrength);
    out.opacityMaskClipValue = value.value("opacityMaskClipValue", out.opacityMaskClipValue);
    out.roughnessScale = value.value("roughnessScale", out.roughnessScale);
    out.metalnessScale = value.value("metalnessScale", out.metalnessScale);
    out.aoScale = value.value("aoScale", out.aoScale);

    if (value.contains("uvScale"))
        (void)Try_ParseVec2(value["uvScale"], out.uvScale);
    if (value.contains("uvOffset"))
        (void)Try_ParseVec2(value["uvOffset"], out.uvOffset);

    return true;
}

fs::path MaterialBuilder::To_AbsoluteTexturePath(const fs::path& resourceRoot, const string& relGeneric)
{
    if (relGeneric.empty())
        return {};

    fs::path rel(relGeneric);
    if (rel.is_absolute())
        return rel.lexically_normal();

    return (resourceRoot / rel).lexically_normal();
}

void MaterialBuilder::Initialize(const fs::path& materialsRoot)
{
    _keyToBundlePath.clear();
    _root.clear();

    if (materialsRoot.empty())
        return;

    std::error_code ec{};
    fs::path absRoot = materialsRoot;
    if (absRoot.is_relative())
        absRoot = fs::absolute(absRoot, ec);
    absRoot = absRoot.lexically_normal();

    if (ec || !fs::exists(absRoot, ec) || !fs::is_directory(absRoot, ec))
        return;

    _root = absRoot;

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(_root, ec))
    {
        if (ec)
            break;

        if (!entry.is_regular_file(ec))
            continue;

        const fs::path& path = entry.path();
        if (!Is_MaterialBundleFile(path))
            continue;

        const string key = To_Key(path);
        if (key.empty())
            continue;

        const fs::path absPath = fs::absolute(path, ec).lexically_normal();
        if (ec)
            continue;

        const auto it = _keyToBundlePath.find(key);
        if (it != _keyToBundlePath.end())
        {
            LOG_WARN("[MaterialBuilder] Duplicate bundle key='{}' path='{}' existing='{}'",
                key,
                String::ToString(absPath.wstring()),
                String::ToString(it->second.wstring()));
            continue;
        }

        _keyToBundlePath.emplace(key, absPath);
    }
}

const fs::path* MaterialBuilder::FindBundlePath(std::string_view key) const
{
    if (key.empty())
        return nullptr;

    const auto it = _keyToBundlePath.find(string(key));
    if (it == _keyToBundlePath.end())
        return nullptr;

    return &it->second;
}

bool MaterialBuilder::Try_ApplyToModelByKey(ModelCom& model, std::string_view key) const
{
    const fs::path* bundlePathPtr = FindBundlePath(key);
    if (bundlePathPtr == nullptr)
        return false;

    const fs::path& bundlePath = *bundlePathPtr;

    std::ifstream file(bundlePath);
    if (!file.is_open())
        return false;

    json root{};
    try
    {
        file >> root;
    }
    catch (...)
    {
        return false;
    }

    if (!root.is_object())
        return false;

    // 번들 파일이 들어갈 Resources 폴더를 기준으로 상대 경로를 저장한다 (Material_View 저장 규칙)
    const fs::path resourceRoot = bundlePath.has_parent_path()
        ? bundlePath.parent_path().parent_path()
        : fs::path{};

    unordered_map<string, BundleInstance> instances{};
    if (root.contains("instances") && root["instances"].is_array())
    {
        for (const json& ji : root["instances"])
        {
            if (!ji.is_object())
                continue;

            const string assetName = ji.value("assetName", string{});
            if (assetName.empty())
                continue;

            BundleInstance inst{};

            if (ji.contains("renderState"))
                (void)Try_ParseRenderState(ji["renderState"], inst.renderState);

            if (ji.contains("cb"))
                (void)Try_ParseCB(ji["cb"], inst.cb);

            if (ji.contains("overrides") && ji["overrides"].is_object())
            {
                const json& jov = ji["overrides"];
                inst.baseColorOverride = jov.value("baseColor", inst.baseColorOverride);
                inst.normalOverride = jov.value("normal", inst.normalOverride);
                inst.ormOverride = jov.value("orm", inst.ormOverride);
                inst.emissiveOverride = jov.value("emissive", inst.emissiveOverride);
                inst.opacityMaskOverride = jov.value("opacityMask", inst.opacityMaskOverride);
            }

            if (ji.contains("textures") && ji["textures"].is_object())
            {
                const json& jtex = ji["textures"];
                inst.baseColorRel = jtex.value("baseColor", string{});
                inst.normalRel = jtex.value("normal", string{});
                inst.ormRel = jtex.value("orm", string{});
                inst.emissiveRel = jtex.value("emissive", string{});
                inst.opacityMaskRel = jtex.value("opacityMask", string{});
            }

            instances.emplace(assetName, std::move(inst));
        }
    }

    if (!root.contains("mappings") || !root["mappings"].is_array())
        return false;

    for (const json& jm : root["mappings"])
    {
        if (!jm.is_object())
            continue;

        const uint32 materialIndex = jm.value("materialIndex", UINT32_MAX);
        if (materialIndex == UINT32_MAX)
            continue;

        if (!jm.contains("ref") || !jm["ref"].is_object())
            continue;

        const json& jref = jm["ref"];
        const string assetName = jref.value("assetName", string{});
        if (assetName.empty())
            continue;

        const auto itInst = instances.find(assetName);
        if (itInst == instances.end())
            continue;

        const BundleInstance& inst = itInst->second;

        const Shared<Material> material = model.Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        material->Set_RenderState(inst.renderState);
        material->Set_CB(inst.cb);

        auto applySlot = [&](Engine::MaterialTextureSlot slot, bool enabled, const string& relPath) -> void
        {
            if (!enabled)
                return;

            const fs::path abs = To_AbsoluteTexturePath(resourceRoot, relPath);
            if (abs.empty())
            {
                (void)material->Override_TextureSlot(slot, 0, L"");
                return;
            }

            (void)material->Override_TextureSlot(slot, 0, abs.wstring());
        };

        applySlot(Engine::MaterialTextureSlot::BaseColor, inst.baseColorOverride, inst.baseColorRel);
        applySlot(Engine::MaterialTextureSlot::Normal, inst.normalOverride, inst.normalRel);
        applySlot(Engine::MaterialTextureSlot::ORM, inst.ormOverride, inst.ormRel);
        applySlot(Engine::MaterialTextureSlot::Emissive, inst.emissiveOverride, inst.emissiveRel);

        // opacityMask는 에디터 Apply와 동일 정책 유지
        // - override true: relPath가 비어있으면 clear
        // - override false: 명시적으로 clear
        if (inst.opacityMaskOverride)
        {
            applySlot(Engine::MaterialTextureSlot::OpacityMask, true, inst.opacityMaskRel);
        }
        else
        {
            (void)material->Override_TextureSlot(Engine::MaterialTextureSlot::OpacityMask, 0, L"");
        }
    }

    return true;
}

bool MaterialBuilder::Is_MaterialBundleFile(const fs::path& path)
{
    const string name = String::ToLowerCopy(String::ToString(path.filename().wstring()));
    return name.ends_with(".material.json");
}

string MaterialBuilder::To_Key(const fs::path& path)
{
    string key = String::ToString(path.stem().wstring());

    if (key.ends_with(".material"))
        key.resize(key.size() - strlen(".material"));

    // Windows 경로는 대소문자 영향이 없으니, key는 소문자로 정규화한다.
    return String::ToLowerCopy(key);
}
