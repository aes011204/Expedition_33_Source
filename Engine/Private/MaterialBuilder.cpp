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
    // json 값이 없거나 형식이 다르면 기존 out 값을 유지한다.
    if (!value.is_array() || value.size() < 2)
        return false;
    out.x = value[0].get<float>();
    out.y = value[1].get<float>();
    return true;
}

bool MaterialBuilder::ParseBool(const json& value, bool fallback)
{
    // 예전 데이터에서 0, 1로 저장한 경우를 대비해서 bool/int/uint를 같이 처리한다.
    if (value.is_boolean())
        return value.get<bool>();

    if (value.is_number_integer())
        return value.get<int32>() != 0;

    if (value.is_number_unsigned())
        return value.get<uint32>() != 0u;

    return fallback;
}

bool MaterialBuilder::Try_ParseVec4(const json& value, Vec4& out)
{
    if (!value.is_array() || value.size() < 4)
        return false;
    // 잘못된 값 하나 때문에 전체 로드를 실패시키지 않기 위해 bool로 성공 여부만 반환한다.
    out.x = value[0].get<float>();
    out.y = value[1].get<float>();
    out.z = value[2].get<float>();
    out.w = value[3].get<float>();
    return true;
}

bool MaterialBuilder::Try_ParseRenderState(const json& value, MaterialRenderState& out)
{
    // renderState는 머테리얼이 결정하는 렌더 상태이다.
    if (!value.is_object())
        return false;

    out.twoSided = value.value("twoSided", out.twoSided);

    const uint32 blendMode = value.value("blendMode", static_cast<uint32>(out.blendMode));

    // 범위를 벗어난 값이면 잘못된 캐스팅을 막고 기존 값을 유지한다.
    if (blendMode < static_cast<uint32>(MaterialBlendMode::END))
        out.blendMode = static_cast<MaterialBlendMode>(blendMode);

    out.opacityMaskClipValue = value.value("opacityMaskClipValue", out.opacityMaskClipValue);

    // alphaClip은 번들에 없을 수도 있음. 
    out.alphaClip = value.value("alphaClip", out.alphaClip);
    return true;
}

bool MaterialBuilder::Try_ParseCB(const json& value, MaterialCB& out)
{
    // MaterialCB는 셰이더에 넘길 머테리얼 파라미터 묶음이다.

    // NOTE !!!!!!!!!!!!!!! 일부 값만 저장될 수 있으니, 존재하는 필드만 덮어쓴다.
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
    out.ormAoSource = value.value("ormAoSource", out.ormAoSource);
    out.ormRoughnessSource = value.value("ormRoughnessSource", out.ormRoughnessSource);
    out.ormMetalnessSource = value.value("ormMetalnessSource", out.ormMetalnessSource);
    out.ormAoSourceSlot = value.value("ormAoSourceSlot", out.ormAoSourceSlot);
    out.ormRoughnessSourceSlot = value.value("ormRoughnessSourceSlot", out.ormRoughnessSourceSlot);
    out.ormMetalnessSourceSlot = value.value("ormMetalnessSourceSlot", out.ormMetalnessSourceSlot);
    out.opacityMaskSource = value.value("opacityMaskSource", out.opacityMaskSource);

    if (value.contains("uvScale"))
        (void)Try_ParseVec2(value["uvScale"], out.uvScale);
    if (value.contains("uvOffset"))
        (void)Try_ParseVec2(value["uvOffset"], out.uvOffset);

    return true;
}

fs::path MaterialBuilder::To_AbsoluteTexturePath(const fs::path& resourceRoot, const string& relGeneric)
{
    // 번들에는 Resources 기준 상대 경로가 들어오는 것을 기본으로 본다.
    // 이미 절대 경로로 들어온 값은 그대로 정규화해서 사용한다.
    if (relGeneric.empty())
        return {};

    fs::path rel(relGeneric);
    if (rel.is_absolute())
        return rel.lexically_normal();

    return (resourceRoot / rel).lexically_normal();
}

wstring MaterialBuilder::Resolve_TexturePathFromJson(const fs::path& resourceRoot, const json& textureValue)
{
    if (textureValue.is_string())
        return To_AbsoluteTexturePath(resourceRoot, textureValue.get<string>()).wstring();

    if (!textureValue.is_object())
        return {};

    const string guid = textureValue.value("texture_guid", string{});
    if (!guid.empty())
    {
        const wstring resolvedByGuid = GAME->Resolve_AssetPath(guid);
        if (!resolvedByGuid.empty())
            return resolvedByGuid;
    }

    const string fallbackPath = textureValue.value("texture_path", string{});
    return To_AbsoluteTexturePath(resourceRoot, fallbackPath).wstring();
}

void MaterialBuilder::Initialize(const fs::path& materialsRoot)
{
    // MaterialsRoot 아래의 *.material.json을 찾아 key -> path 인덱스를 만든다.
    // 여기서는 파일 내용을 파싱하지 않고, 실제 json 파싱은 GetOrLoadBundle()에서 처음 요청될 때 1회만 한다.
    _keyToBundlePath.clear();
    _bundleCache.clear();
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

        // 파일명에서 번들 key를 만든다.
        // e.g., lunebody.material.json -> lunebody
        const string key = To_Key(path);
        if (key.empty())
            continue;

        const fs::path absPath = fs::absolute(path, ec).lexically_normal();
        if (ec)
            continue;

        // NOTE ! 중복 시 먼저 발견한 파일을 유지하고, 중복 파일은 경고만 남긴다.
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

    // 소문자로 통일
    const auto it = _keyToBundlePath.find(String::ToLowerCopy(string(key)));
    if (it == _keyToBundlePath.end())
        return nullptr;

    return &it->second;
}

const MaterialBuilder::BundleAsset* MaterialBuilder::GetOrLoadBundle(std::string_view bundleKey)
{
    // bundleKey로 번들 데이터를 찾아 반환하는데, 이미 로드된 번들이면 캐시를 바로 반환하고 처음 요청된 번들이면 이 시점에 파일을 읽어서 캐시에 올린다.
    if (bundleKey.empty())
        return nullptr;

    const string key = String::ToLowerCopy(string(bundleKey));

    std::lock_guard<std::mutex> lock(_bundleCacheMutex);

    auto itCache = _bundleCache.find(key);
    if (itCache != _bundleCache.end())
        return itCache->second.get();

    const fs::path* bundlePathPtr = FindBundlePath(key);
    if (bundlePathPtr == nullptr)
        return nullptr;

    Unique<BundleAsset> asset(new BundleAsset());
    if (!Try_LoadBundleAsset(*bundlePathPtr, *asset))
        return nullptr;

    BundleAsset* raw = asset.get();
    _bundleCache.emplace(key, std::move(asset));
    return raw;
}

bool MaterialBuilder::Try_LoadBundleAsset(const fs::path& bundlePath, BundleAsset& outAsset) const
{
    // .material.json 하나를 읽어서 BundleAsset으로 변환한다.
    // BundleAsset은 [ materialIndex -> assetName 매핑 ]과  [ assetName -> 실제 기본 머테리얼 데이터 ] 를 가진다.
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

    outAsset.materialIndexToAssetName.clear();
    outAsset.instancesByAssetName.clear();

    // 현재 저장 포맷은 텍스처 경로를 Resources 기준 상대경로로 저장한다.
    // Materials/<map>/<model>.material.json처럼 폴더가 깊어져도 Resources 루트를 찾아 기준을 유지한다.
    fs::path resourceRoot{};
    for (fs::path cursor = bundlePath.parent_path(); !cursor.empty(); cursor = cursor.parent_path())
    {
        const string folderName = String::ToLowerCopy(String::ToString(cursor.filename().wstring()));
        if (folderName == "resources")
        {
            resourceRoot = cursor;
            break;
        }
    }

    if (resourceRoot.empty())
    {
        resourceRoot = bundlePath.has_parent_path()
            ? bundlePath.parent_path().parent_path()
            : fs::path{};
    }

    if (root.contains("instances") && root["instances"].is_array())
    {
        for (const json& ji : root["instances"])
        {
            if (!ji.is_object())
                continue;

            const string assetName = ji.value("assetName", string{});
            if (assetName.empty())
                continue;

            BaseInstanceData inst{};

            // 인스턴스에 저장된 기본 렌더 상태를 읽는다.
            if (ji.contains("renderState"))
                (void)Try_ParseRenderState(ji["renderState"], inst.renderState);

            inst.forcePassIndex = ji.value("forcePassIndex", inst.forcePassIndex);

            // 셰이더 CB 값도 저장된 값만 덮어쓴다.
            if (ji.contains("cb"))
                (void)Try_ParseCB(ji["cb"], inst.cb);

            // overrides는 이 인스턴스가 어떤 텍스처 슬롯을 실제로 사용하는지 나타낸다.
            // path가 비어 있어도 override가 true면 비우는 의미로 쓸 수 있다. 예를 들어 마스크 텍스처를 비워서 사용하지 않는다 등 
            if (ji.contains("overrides") && ji["overrides"].is_object())
            {
                const json& jov = ji["overrides"];
                inst.baseColorOverride = jov.value("baseColor", inst.baseColorOverride);
                inst.normalOverride = jov.value("normal", inst.normalOverride);
                inst.ormOverride = jov.value("orm", inst.ormOverride);
                inst.emissiveOverride = jov.value("emissive", inst.emissiveOverride);
                inst.opacityMaskOverride = jov.value("opacityMask", inst.opacityMaskOverride);
                inst.hairDepthOverride = jov.value("hairDepth", inst.hairDepthOverride);
                inst.hairIDOverride = jov.value("hairID", inst.hairIDOverride);
                inst.irisHeightOverride = jov.value("irisHeight", inst.irisHeightOverride);
                inst.irisMaskOverride = jov.value("irisMask", inst.irisMaskOverride);
                inst.orm2Override = jov.value("orm2", inst.orm2Override);
                inst.orm3Override = jov.value("orm3", inst.orm3Override);
            }

            // 상대 경로로 저장 된 번들의 텍스쳐 경로를 런타임 접근을 위한 절대로 변경
            if (ji.contains("textures") && ji["textures"].is_object())
            {
                const json& jtex = ji["textures"];
                if (jtex.contains("baseColor")) inst.baseColorPath = Resolve_TexturePathFromJson(resourceRoot, jtex["baseColor"]);
                if (jtex.contains("normal")) inst.normalPath = Resolve_TexturePathFromJson(resourceRoot, jtex["normal"]);
                if (jtex.contains("orm")) inst.ormPath = Resolve_TexturePathFromJson(resourceRoot, jtex["orm"]);
                if (jtex.contains("emissive")) inst.emissivePath = Resolve_TexturePathFromJson(resourceRoot, jtex["emissive"]);
                if (jtex.contains("opacityMask")) inst.opacityMaskPath = Resolve_TexturePathFromJson(resourceRoot, jtex["opacityMask"]);
                if (jtex.contains("hairDepth")) inst.hairDepthPath = Resolve_TexturePathFromJson(resourceRoot, jtex["hairDepth"]);
                if (jtex.contains("hairID")) inst.hairIDPath = Resolve_TexturePathFromJson(resourceRoot, jtex["hairID"]);
                if (jtex.contains("irisHeight")) inst.irisHeightPath = Resolve_TexturePathFromJson(resourceRoot, jtex["irisHeight"]);
                if (jtex.contains("irisMask")) inst.irisMaskPath = Resolve_TexturePathFromJson(resourceRoot, jtex["irisMask"]);
                if (jtex.contains("orm2")) inst.orm2Path = Resolve_TexturePathFromJson(resourceRoot, jtex["orm2"]);
                if (jtex.contains("orm3")) inst.orm3Path = Resolve_TexturePathFromJson(resourceRoot, jtex["orm3"]);
            }

            outAsset.instancesByAssetName.emplace(assetName, std::move(inst));
        }
    }

    if (root.contains("mappings") && root["mappings"].is_array())
    {
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

            outAsset.materialIndexToAssetName[materialIndex] = assetName;
        }
    }

    return true;
}

bool MaterialBuilder::Try_ApplyMaterialBindingsFromJson(ModelCom& model, const json& data) const
{
    // material_bindings를 ModelCom에 아래 정보를 복원한다.
    // 1. 이 모델 슬롯이 어떤 번들을 참조하는지
    // 2. 오브젝트별 인스턴스 오버라이드가 있는지
    if (!data.contains("material_bindings") || !data["material_bindings"].is_array())
        return false;

    model.Ensure_MaterialBindings();
    vector<MaterialBinding>& bindings = model.Get_MaterialBindings();

    bool anyApplied = false;

    for (const json& item : data["material_bindings"])
    {
        if (!item.is_object())
            continue;

        const uint32 materialIndex = item.value("material_index", UINT32_MAX);
        if (materialIndex >= bindings.size())
            continue;

        MaterialBinding& binding = bindings[materialIndex];

        // base_bundle_key는 이 materialIndex가 참조할 .material.json 번들의 key다.
        // 실제 번들 로드는 ModelCom 쪽에서 Apply할 때 MaterialBuilder에게 요청한다.
        if (item.contains("base_bundle_key") && item["base_bundle_key"].is_string())
            binding.baseBundleKey = String::ToLowerCopy(item["base_bundle_key"].get<string>());

        // override 플래그가 켜진 항목만 실제 base 값 위에 덮어쓴다.
        if (!item.contains("instance_override") || !item["instance_override"].is_object())
        {
            anyApplied = true;
            continue;
        }

        const json& instJson = item["instance_override"];
        MaterialInstanceOverride& inst = binding.instance;
        inst.overrideTemplateEnabled =
            ParseBool(instJson.value("override_template_enabled", inst.overrideTemplateEnabled), inst.overrideTemplateEnabled);
        if (inst.overrideTemplateEnabled)
        {
            const uint32 templateValue = instJson.value(
                "template",
                instJson.value("template_id", static_cast<uint32>(inst.templateId)));
            if (templateValue < static_cast<uint32>(MaterialShaderTemplate::END))
                inst.templateId = static_cast<MaterialShaderTemplate>(templateValue);
        }

        inst.forcedPassIndex = static_cast<int32>(instJson.value("force_pass_index", inst.forcedPassIndex));
        if (inst.forcedPassIndex < 0 && instJson.contains("forcePassIndex"))
            inst.forcedPassIndex = static_cast<int32>(instJson.value("forcePassIndex", inst.forcedPassIndex));

        inst.overrideRenderStateEnabled =
            ParseBool(instJson.value("override_render_state_enabled", inst.overrideRenderStateEnabled), inst.overrideRenderStateEnabled);

        if (inst.overrideRenderStateEnabled && instJson.contains("render_state"))
            (void)Try_ParseRenderState(instJson["render_state"], inst.renderState);

        // CB는 "필드별 override"만 지원한다(레거시 무시).
        inst.cbMask.Clear();
        if (instJson.contains("cb_overrides") && instJson["cb_overrides"].is_object())
        {
            const json& jmask = instJson["cb_overrides"];
#define MB_READ_CB_MASK(_key, _member, _kind) \
            if (jmask.contains(_key)) inst.cbMask._member = ParseBool(jmask[_key], false);
            MATERIAL_CB_OVERRIDE_FIELDS(MB_READ_CB_MASK)
#undef MB_READ_CB_MASK
        }

        if (inst.cbMask.Any() && instJson.contains("cb") && instJson["cb"].is_object())
        {
            const json& jcb = instJson["cb"];

            if (inst.cbMask.baseColorTint && jcb.contains("baseColorTint"))
                (void)Try_ParseVec4(jcb["baseColorTint"], inst.cb.baseColorTint);
            if (inst.cbMask.emissiveColor && jcb.contains("emissiveColor"))
                (void)Try_ParseVec4(jcb["emissiveColor"], inst.cb.emissiveColor);

            if (inst.cbMask.emissiveIntensity) inst.cb.emissiveIntensity = jcb.value("emissiveIntensity", inst.cb.emissiveIntensity);
            if (inst.cbMask.emissiveMaskMul) inst.cb.emissiveMaskMul = jcb.value("emissiveMaskMul", inst.cb.emissiveMaskMul);
            if (inst.cbMask.normalStrength) inst.cb.normalStrength = jcb.value("normalStrength", inst.cb.normalStrength);
            if (inst.cbMask.opacityMaskClipValue) inst.cb.opacityMaskClipValue = jcb.value("opacityMaskClipValue", inst.cb.opacityMaskClipValue);
            if (inst.cbMask.roughnessScale) inst.cb.roughnessScale = jcb.value("roughnessScale", inst.cb.roughnessScale);
            if (inst.cbMask.metalnessScale) inst.cb.metalnessScale = jcb.value("metalnessScale", inst.cb.metalnessScale);
            if (inst.cbMask.aoScale) inst.cb.aoScale = jcb.value("aoScale", inst.cb.aoScale);
            if (inst.cbMask.ormAoSource) inst.cb.ormAoSource = jcb.value("ormAoSource", inst.cb.ormAoSource);
            if (inst.cbMask.ormRoughnessSource) inst.cb.ormRoughnessSource = jcb.value("ormRoughnessSource", inst.cb.ormRoughnessSource);
            if (inst.cbMask.ormMetalnessSource) inst.cb.ormMetalnessSource = jcb.value("ormMetalnessSource", inst.cb.ormMetalnessSource);
            if (inst.cbMask.opacityMaskSource) inst.cb.opacityMaskSource = jcb.value("opacityMaskSource", inst.cb.opacityMaskSource);

            if (inst.cbMask.uvScale && jcb.contains("uvScale"))
                (void)Try_ParseVec2(jcb["uvScale"], inst.cb.uvScale);
            if (inst.cbMask.uvOffset && jcb.contains("uvOffset"))
                (void)Try_ParseVec2(jcb["uvOffset"], inst.cb.uvOffset);
            if (inst.cbMask.uvRotation) inst.cb.uvRotation = jcb.value("uvRotation", inst.cb.uvRotation);
            if (inst.cbMask.materialMask) inst.cb.materialMask = jcb.value("materialMask", inst.cb.materialMask);
        }

        auto parseTextureOverride = [&](const char* name, TextureOverride& out) -> void
        {
            if (!instJson.contains("textures") || !instJson["textures"].is_object())
                return;

            const json& texObj = instJson["textures"];
            if (!texObj.contains(name) || !texObj[name].is_object())
                return;

            const json& tex = texObj[name];
            out.enabled = ParseBool(tex.value("enabled", out.enabled), out.enabled);
            if (!out.enabled)
                return;

            wstring resolved{};
            const string guid = tex.value("texture_guid", string{});
            if (!guid.empty())
                resolved = GAME->Resolve_AssetPath(guid);

            if (resolved.empty())
            {
                const string path = tex.value("texture_path", string{});
                resolved = String::ToWString(path);
            }

            out.path = resolved;
        };

        parseTextureOverride("base_color", inst.baseColor);
        parseTextureOverride("normal", inst.normal);
        parseTextureOverride("orm", inst.orm);
        parseTextureOverride("orm2", inst.orm2);
        parseTextureOverride("orm3", inst.orm3);
        parseTextureOverride("emissive", inst.emissive);
        parseTextureOverride("opacity_mask", inst.opacityMask);

        parseTextureOverride("hair_depth", inst.hairDepth);
        parseTextureOverride("hair_id", inst.hairID);
        parseTextureOverride("iris_height", inst.irisHeight);
        parseTextureOverride("iris_mask", inst.irisMask);

        anyApplied = true;
    }

    return anyApplied;
}

bool MaterialBuilder::Try_ApplyModelMaterialOverridesFromJson(ModelCom& model, const json& data) const
{
    if (!data.contains("material_overrides") || !data["material_overrides"].is_array())
        return false;

    model.Ensure_MaterialBindings();
    vector<MaterialBinding>& bindings = model.Get_MaterialBindings();

    bool anyApplied = false;

    for (const json& overrideData : data["material_overrides"])
    {
        if (!overrideData.is_object())
            continue;

        const uint32 materialIndex = overrideData.value("material_index", UINT32_MAX);
        if (materialIndex >= bindings.size())
            continue;

        if (!overrideData.contains("slot"))
            continue;

        MaterialTextureSlot slot = MaterialTextureSlot::END;
        if (overrideData["slot"].is_string())
        {
            const string slotName = overrideData["slot"].get<string>();
            if (slotName == "BaseColor")        slot = MaterialTextureSlot::BaseColor;
            else if (slotName == "Normal")      slot = MaterialTextureSlot::Normal;
            else if (slotName == "Emissive")    slot = MaterialTextureSlot::Emissive;
            else if (slotName == "OpacityMask") slot = MaterialTextureSlot::OpacityMask;
            else if (slotName == "ORM")         slot = MaterialTextureSlot::ORM;
            else if (slotName == "ORM2")        slot = MaterialTextureSlot::ORM2;
            else if (slotName == "ORM3")        slot = MaterialTextureSlot::ORM3;
            else if (slotName == "HairDepth")   slot = MaterialTextureSlot::HairDepth;
            else if (slotName == "HairID")      slot = MaterialTextureSlot::HairID;
            else if (slotName == "IrisHeight")  slot = MaterialTextureSlot::IrisHeight;
            else if (slotName == "IrisMask")    slot = MaterialTextureSlot::IrisMask;
        }

        if (slot == MaterialTextureSlot::END)
            continue;

        const uint32 textureIndex = overrideData.value("texture_index", 0u);
        if (textureIndex != 0u)
            continue;

        wstring texturePath{};
        const string textureGuid = overrideData.value("texture_guid", string{});
        if (!textureGuid.empty())
            texturePath = GAME->Resolve_AssetPath(textureGuid);

        if (texturePath.empty())
        {
            const string rawPath = overrideData.value("texture_path", string{});
            if (!rawPath.empty())
                texturePath = String::ToWString(rawPath);
        }

        if (texturePath.empty())
            continue;

        MaterialInstanceOverride& inst = bindings[materialIndex].instance;
        TextureOverride* target = nullptr;
        if (slot == MaterialTextureSlot::BaseColor) target = &inst.baseColor;
        else if (slot == MaterialTextureSlot::Normal) target = &inst.normal;
        else if (slot == MaterialTextureSlot::ORM) target = &inst.orm;
        else if (slot == MaterialTextureSlot::ORM2) target = &inst.orm2;
        else if (slot == MaterialTextureSlot::ORM3) target = &inst.orm3;
        else if (slot == MaterialTextureSlot::Emissive) target = &inst.emissive;
        else if (slot == MaterialTextureSlot::OpacityMask) target = &inst.opacityMask;
        else if (slot == MaterialTextureSlot::HairDepth) target = &inst.hairDepth;
        else if (slot == MaterialTextureSlot::HairID) target = &inst.hairID;
        else if (slot == MaterialTextureSlot::IrisHeight) target = &inst.irisHeight;
        else if (slot == MaterialTextureSlot::IrisMask) target = &inst.irisMask;

        if (target == nullptr)
            continue;

        target->enabled = true;
        target->path = texturePath;
        anyApplied = true;
    }

    return anyApplied;
}

bool MaterialBuilder::Try_GetBaseCBForMaterial(std::string_view bundleKey, uint32 materialIndex, MaterialCB& outCB) const
{
    if (bundleKey.empty())
        return false;

    const BundleAsset* bundle = const_cast<MaterialBuilder*>(this)->GetOrLoadBundle(bundleKey);
    if (bundle == nullptr)
        return false;

    const auto itName = bundle->materialIndexToAssetName.find(materialIndex);
    if (itName == bundle->materialIndexToAssetName.end())
        return false;

    const auto itInst = bundle->instancesByAssetName.find(itName->second);
    if (itInst == bundle->instancesByAssetName.end())
        return false;

    outCB = itInst->second.cb;
    return true;
}

bool MaterialBuilder::Try_ApplyBundleMaterialToMaterial(std::string_view bundleKey, uint32 materialIndex, Material& material) const
{
    if (bundleKey.empty())
        return false;

    const BundleAsset* bundle = const_cast<MaterialBuilder*>(this)->GetOrLoadBundle(bundleKey);
    if (bundle == nullptr)
        return false;

    const auto itName = bundle->materialIndexToAssetName.find(materialIndex);
    if (itName == bundle->materialIndexToAssetName.end())
        return false;

    const auto itInst = bundle->instancesByAssetName.find(itName->second);
    if (itInst == bundle->instancesByAssetName.end())
        return false;

    const BaseInstanceData& base = itInst->second;
    material.Set_RenderState(base.renderState);
    material.Set_CB(base.cb);
    material.Set_ForcedPassIndex(base.forcePassIndex);

    if (base.baseColorOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::BaseColor, 0, base.baseColorPath);
    if (base.normalOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::Normal, 0, base.normalPath);
    if (base.ormOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::ORM, 0, base.ormPath);
    if (base.orm2Override)
        (void)material.Override_TextureSlot(MaterialTextureSlot::ORM2, 0, base.orm2Path);
    if (base.orm3Override)
        (void)material.Override_TextureSlot(MaterialTextureSlot::ORM3, 0, base.orm3Path);
    if (base.emissiveOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::Emissive, 0, base.emissivePath);
    if (base.hairDepthOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::HairDepth, 0, base.hairDepthPath);
    if (base.hairIDOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::HairID, 0, base.hairIDPath);
    if (base.irisHeightOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::IrisHeight, 0, base.irisHeightPath);
    if (base.irisMaskOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::IrisMask, 0, base.irisMaskPath);

    if (base.opacityMaskOverride)
        (void)material.Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, base.opacityMaskPath);
    else
        (void)material.Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, L"");

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
