#include "MaterialCreateHelper.h"

#include <algorithm>
#include <fstream>

NS_BEGIN(MainEditor)

#pragma region UTILS
namespace
{
    string To_Lower(string value)
    {
        transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
        return value;
    }

    bool Try_ReadString(const json& obj, const string& key, string& out)
    {
        // obj가 object가 아니거나, key가 없거나, string이 아니면 실패로 본다
        if (!obj.is_object())
            return false;

        auto it = obj.find(key);
        if (it == obj.end() || !it->is_string())
            return false;

        out = it->get<string>();
        return true;
    }

    string Extract_AssetNameFromObjectName(const string& objectName)
    {
        // ObjectName 문자열에는 타입 토큰과 에셋 이름이 같이 들어온다
        // 여기서는 따옴표 구간을 먼저 찾고, 마지막 점 뒤쪽을 에셋 이름으로 쓴다
        // 점이 없으면 따옴표 구간 전체를 에셋 이름으로 본다
        const size_t quoteStart = objectName.find('\'');
        if (quoteStart == string::npos)
            return objectName;

        const size_t quoteEnd = objectName.find('\'', quoteStart + 1);
        if (quoteEnd == string::npos || quoteEnd <= quoteStart + 1)
            return objectName;

        const string quoted = objectName.substr(quoteStart + 1, quoteEnd - (quoteStart + 1));
        const size_t dotPos = quoted.find_last_of('.');
        if (dotPos == string::npos || dotPos + 1 >= quoted.size())
            return quoted;

        return quoted.substr(dotPos + 1);
    }

    MaterialRefKind Resolve_MaterialRefKindFromObjectName(const string& objectName)
    {
        // MaterialInterface가 가리키는 대상은 M_*, MI_* 두 가지가 섞여서 들어올 수 있다
        // 여기서는 ObjectName 문자열에 들어있는 타입 토큰만 보고 대략 분류한다
        if (objectName.find("MaterialInstance") != string::npos)
            return MaterialRefKind::MI;

        if (objectName.find("Material'") != string::npos || objectName.find("Material ") != string::npos)
            return MaterialRefKind::M;

        return MaterialRefKind::Unknown;
    }

    string Extract_UAssetBaseName(const string& uassetPath)
    {
        // 여기서 마지막 파일 이름만 뽑고, 마지막 점 앞부분만 남겨서 base name으로 쓴다
        // 이 base name을 텍스처 파일 stem과 매칭해서 실제 이미지 경로를 찾는다
        if (uassetPath.empty())
            return {};

        string value = uassetPath;
        const size_t slashPos = value.find_last_of("/\\");
        if (slashPos != string::npos)
            value = value.substr(slashPos + 1);

        const size_t dotPos = value.find_last_of('.');
        if (dotPos != string::npos)
            value = value.substr(0, dotPos);

        return value;
    }

    bool Try_ReadJsonFloat(const json& obj, const char* key, float& out)
    {
        if (!obj.is_object() || key == nullptr)
            return false;

        auto it = obj.find(key);
        if (it == obj.end())
            return false;

        if (!it->is_number_float() && !it->is_number_integer() && !it->is_number_unsigned())
            return false;

        out = it->get<float>();
        return true;
    }

    int Resolve_BlendModeIntFromString(const string& blendMode)
    {
        if (blendMode.find("Masked") != string::npos)
            return 1;

        if (blendMode.find("Translucent") != string::npos)
            return 2;

        return 0;
    }

    bool Convert_UnrealExportMaterialJsonToSummaryJson(const json& sourceRoot, json& outSummaryRoot)
    {
        const json* item = nullptr;

        if (sourceRoot.is_array())
        {
            if (sourceRoot.empty() || !sourceRoot[0].is_object())
                return false;

            item = &sourceRoot[0];
        }
        else if (sourceRoot.is_object())
        {
            item = &sourceRoot;
        }
        else
        {
            return false;
        }

        if (item == nullptr || !item->is_object())
            return false;

        const json& properties = item->value("Properties", json::object());

        outSummaryRoot = json::object();
        outSummaryRoot["Textures"] = json::object();
        outSummaryRoot["Parameters"] = json::object();
        outSummaryRoot["Parameters"]["BlendMode"] = 0;
        outSummaryRoot["Parameters"]["ShadingModel"] = 0;
        outSummaryRoot["Parameters"]["Colors"] = json::object();
        outSummaryRoot["Parameters"]["Scalars"] = json::object();
        outSummaryRoot["Parameters"]["Switches"] = json::object();
        outSummaryRoot["Parameters"]["Properties"] = json::object();

        json& textures = outSummaryRoot["Textures"];
        json& parameters = outSummaryRoot["Parameters"];
        json& colors = parameters["Colors"];
        json& scalars = parameters["Scalars"];
        json& switches = parameters["Switches"];
        json& parameterProperties = parameters["Properties"];

        const json& textureParams = properties.value("TextureParameterValues", json::array());
        if (textureParams.is_array())
        {
            for (const json& textureParam : textureParams)
            {
                if (!textureParam.is_object())
                    continue;

                const json& parameterInfo = textureParam.value("ParameterInfo", json::object());
                const string parameterName = parameterInfo.value("Name", string{});
                if (parameterName.empty())
                    continue;

                const json& parameterValue = textureParam.value("ParameterValue", json::object());
                const string objectPath = parameterValue.value("ObjectPath", string{});
                const string objectName = parameterValue.value("ObjectName", string{});

                if (objectPath.empty())
                    continue;

                const string assetName = Extract_AssetNameFromObjectName(objectName);
                textures[parameterName] = objectPath;

                if (!assetName.empty())
                    textures[assetName] = objectPath;

                const string parameterNameLower = To_Lower(parameterName);
                if (parameterNameLower == "texture")
                {
                    textures["PM_SpecularMasks"] = objectPath;
                }
                else if (parameterNameLower.find("base") != string::npos ||
                    parameterNameLower.find("diffuse") != string::npos ||
                    parameterNameLower.find("color") != string::npos)
                {
                    textures["PM_Diffuse"] = objectPath;
                }
                else if (parameterNameLower.find("normal") != string::npos)
                {
                    textures["PM_Normals"] = objectPath;
                }
                else if (parameterNameLower.find("orm") != string::npos ||
                    parameterNameLower.find("roughness") != string::npos ||
                    parameterNameLower.find("metal") != string::npos ||
                    parameterNameLower.find("specular") != string::npos)
                {
                    textures["PM_SpecularMasks"] = objectPath;
                }
                else if (parameterNameLower.find("emissive") != string::npos ||
                    parameterNameLower.find("glow") != string::npos)
                {
                    textures["Glow_Map"] = objectPath;
                }
                else if (parameterNameLower.find("opacity") != string::npos ||
                    parameterNameLower.find("mask") != string::npos ||
                    parameterNameLower.find("coverage") != string::npos)
                {
                    textures["Opacity Mask Map"] = objectPath;
                }
            }
        }

        const json& scalarParams = properties.value("ScalarParameterValues", json::array());
        if (scalarParams.is_array())
        {
            for (const json& scalarParam : scalarParams)
            {
                if (!scalarParam.is_object())
                    continue;

                const json& parameterInfo = scalarParam.value("ParameterInfo", json::object());
                const string parameterName = parameterInfo.value("Name", string{});
                if (parameterName.empty())
                    continue;

                float value = 0.f;
                if (Try_ReadJsonFloat(scalarParam, "ParameterValue", value))
                    scalars[parameterName] = value;
            }
        }

        const json& vectorParams = properties.value("VectorParameterValues", json::array());
        if (vectorParams.is_array())
        {
            for (const json& vectorParam : vectorParams)
            {
                if (!vectorParam.is_object())
                    continue;

                const json& parameterInfo = vectorParam.value("ParameterInfo", json::object());
                const string parameterName = parameterInfo.value("Name", string{});
                if (parameterName.empty())
                    continue;

                const json& parameterValue = vectorParam.value("ParameterValue", json::object());
                if (parameterValue.is_object())
                    colors[parameterName] = parameterValue;
            }
        }

        const json& staticSwitchParams = properties.value("StaticParameters", json::object()).value("StaticSwitchParameters", json::array());
        if (staticSwitchParams.is_array())
        {
            for (const json& switchParam : staticSwitchParams)
            {
                if (!switchParam.is_object())
                    continue;

                const json& parameterInfo = switchParam.value("ParameterInfo", json::object());
                const string parameterName = parameterInfo.value("Name", string{});
                if (parameterName.empty())
                    continue;

                auto itValue = switchParam.find("Value");
                if (itValue != switchParam.end() && itValue->is_boolean())
                    switches[parameterName] = itValue->get<bool>();
            }
        }

        const json& basePropertyOverrides = properties.value("BasePropertyOverrides", json::object());
        if (basePropertyOverrides.is_object())
        {
            parameterProperties["BasePropertyOverrides"] = basePropertyOverrides;

            string blendMode{};
            if (Try_ReadString(basePropertyOverrides, "BlendMode", blendMode))
                parameters["BlendMode"] = Resolve_BlendModeIntFromString(blendMode);
        }

        if (properties.contains("bIncludedInBaseGame"))
            parameterProperties["bIncludedInBaseGame"] = properties["bIncludedInBaseGame"];

        parameters["HasTopDiffuse"] = textures.contains("PM_Diffuse") || textures.contains("Base Color Map") || textures.contains("Color_MAIN") || textures.contains("Texture");
        parameters["HasTopNormals"] = textures.contains("PM_Normals") || textures.contains("Normal Map") || textures.contains("Normal_MAIN");
        parameters["HasTopSpecularMasks"] = textures.contains("PM_SpecularMasks") || textures.contains("ORM Map");
        parameters["HasTopEmissive"] = textures.contains("Glow_Map") || textures.contains("PM_Emissive");
        parameters["IsTranslucent"] = parameters.value("BlendMode", 0) == 2;
        parameters["IsNull"] = false;

        return true;
    }
}
#pragma endregion

MaterialCreateHelper::MaterialCreateHelper(
    const wstring& miRoot,
    const wstring& textureSearchRoot,
    unordered_map<string, wstring>* textureIndex,
    bool* textureIndexDirty)
    : _miRoot(miRoot)
    , _textureSearchRoot(textureSearchRoot)
    , _textureIndex(textureIndex)
    , _textureIndexDirty(textureIndexDirty)
{
}

bool MaterialCreateHelper::Try_LoadJson(const fs::path& filePath, json& out)
{
    // 파일 하나를 통째로 json으로 읽는다
    // 실패하면 false로 떨어지고, 예외는 밖으로 내보내지 않는다
    try
    {
        if (filePath.empty())
            return false;

        ifstream file(filePath);
        if (!file.is_open())
            return false;

        file >> out;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

string MaterialCreateHelper::Pick_TextureUAssetPath(const json& textures, initializer_list<const char*> keys, string* outUsedKey)
{
    // 텍스처 테이블이 key 이름이 제각각이라, 우선순위 리스트로 첫 번째 값을 고른다
    if (!textures.is_object())
        return {};

    for (const char* key : keys)
    {
        auto it = textures.find(key);
        if (it == textures.end() || !it->is_string())
            continue;

        const string value = it->get<string>();
        if (!value.empty())
        {
            if (outUsedKey != nullptr)
                *outUsedKey = key;
            return value;
        }
    }

    return {};
}

fs::path MaterialCreateHelper::Try_ResolveDirectoryPath(const wstring& userPath)
{
    // 유저 입력 경로는 상대 경로일 수도 있고, 실행 경로에 따라 바뀔 수 있으므로, 아래 순서로 디렉터리를 찾아본다
    // 1. 입력 값을 절대 경로로 바꿔서 존재하면 그걸 쓴다
    // 2. 실패하면 현재 경로에서 부모로 올라가면서 Client Bin Resources 쪽을 찾는다
    // 3. 찾았으면 그 기준으로 입력 suffix를 다시 붙여서 유효한지 본다
    try
    {
        fs::path raw(userPath);
        if (raw.empty())
            return {};

        fs::path candidate = raw;
        if (candidate.is_relative())
            candidate = fs::absolute(candidate);
        candidate = candidate.lexically_normal();

        if (fs::exists(candidate) && fs::is_directory(candidate))
            return candidate;

        const fs::path rel = raw.lexically_normal();
        fs::path cursor = fs::current_path();

        auto computeSuffixWithoutDotDots = [](const fs::path& p) -> fs::path
        {
            // dot dot이 섞이면 재조합할 때 경로가 이상해질 수 있어, suffix는 안전하게 정리한 뒤에만 붙인다
            fs::path suffix{};
            for (const fs::path& part : p)
            {
                const wstring w = part.wstring();
                if (w == L"." || w.empty())
                    continue;
                if (w == L"..")
                    continue;
                suffix /= part;
            }
            return suffix;
        };

        const fs::path suffix = computeSuffixWithoutDotDots(rel);

        for (int i = 0; i < 10; ++i)
        {
            const fs::path probeModels = (cursor / "Client" / "Bin" / "Resources" / "Models").lexically_normal();
            if (fs::exists(probeModels) && fs::is_directory(probeModels))
                return probeModels;

            const fs::path probeResources = (cursor / "Client" / "Bin" / "Resources").lexically_normal();
            if (fs::exists(probeResources) && fs::is_directory(probeResources))
            {
                fs::path rebased = (cursor / suffix).lexically_normal();
                if (fs::exists(rebased) && fs::is_directory(rebased))
                    return rebased;
            }

            if (!cursor.has_parent_path())
                break;

            fs::path parent = cursor.parent_path();
            if (parent == cursor)
                break;
            cursor = parent;
        }

        return {};
    }
    catch (...)
    {
        return {};
    }
}

bool MaterialCreateHelper::Is_TextureExtension(const fs::path& path)
{
    // TODO !!!!!!!!!!! DDS 확장 시 여기는 수정해야 한다. 근데 어떻게 될지 몰라서 일단은 패스.
    // 일단 우리가 지금 쓰는 포맷만 본다. 나중에 필요해지면 여기 확장자를 더 늘리면 된다
    const string ext = String::ToLowerCopy(String::ToString(path.extension().wstring()));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".bmp";
}

void MaterialCreateHelper::Rebuild_TextureIndexIfNeeded()
{
    // texture index는 stem lower를 키로 하고 실제 파일 경로를 값으로 둔다
    // 같은 stem이 여러 개면 먼저 만난 걸 하나만 넣는다
    // 이건 완벽하진 않지만 지금 단계에서는 파일을 빨리 찾는 게 우선이라 이렇게 해둔다
    if (_textureIndex == nullptr || _textureIndexDirty == nullptr)
        return;

    if (!(*_textureIndexDirty) && !_textureIndex->empty())
        return;

    _textureIndex->clear();
    *_textureIndexDirty = false;

    fs::path resolvedRoot{};
    try
    {
        resolvedRoot = Try_ResolveDirectoryPath(_textureSearchRoot);
        if (resolvedRoot.empty())
            return;

        for (const auto& entry : fs::recursive_directory_iterator(resolvedRoot))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path path = entry.path();
            if (!Is_TextureExtension(path))
                continue;

            const string stemLower = To_Lower(String::ToString(path.stem().wstring()));
            if (stemLower.empty())
                continue;

            if (_textureIndex->contains(stemLower))
                continue;

            _textureIndex->emplace(stemLower, path.lexically_normal().wstring());
        }
    }
    catch (...)
    {
    }
}

wstring MaterialCreateHelper::Resolve_TextureFileByBaseName(const string& baseName)
{
    // baseName은 파일 stem을 기대한다
    // texture index가 비어있거나 키가 없으면 빈 문자열을 돌려준다
    if (_textureIndex == nullptr)
        return {};

    const string key = To_Lower(baseName);
    const auto it = _textureIndex->find(key);
    if (it == _textureIndex->end())
        return {};
    return it->second;
}

wstring MaterialCreateHelper::Resolve_TextureFileByUAsset(const string& uassetPath)
{
    // uassetPath는 보통 파일이름.파일이름 형태라서 base name 추출을 먼저 한다
    // 추출이 실패하면 resolve도 실패로 본다
    const string base = Extract_UAssetBaseName(uassetPath);
    if (base.empty())
        return {};
    return Resolve_TextureFileByBaseName(base);
}

bool MaterialCreateHelper::Load_MiJsonByName(const string& assetName, json& outMiRoot, vector<HeuristicNote>* inOutHeuristics) const
{
    try
    {
        if (assetName.empty())
            return false;

        const fs::path fileName = String::ToWString(assetName) + L".json";

        const fs::path primaryRoot(_miRoot);
        const fs::path primaryPath = (primaryRoot / fileName).lexically_normal();

        if (Try_LoadJson(primaryPath, outMiRoot))
            return true;

        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool MaterialCreateHelper::Load_SkFile(const wstring& skFilePath, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>& inOutHeuristics)
{
    // 같은 파일을 여러 번 읽어도 mapping이 계속 쌓이지 않게 sourceFile이 같은 기존 엔트리는 먼저 제거하고 다시 넣는다
    json root{};
    const fs::path skPath(skFilePath);
    if (!Try_LoadJson(skPath, root))
        return false;

    const string fileName = String::ToString(skPath.filename().wstring());
    inOutMappings.erase(remove_if(inOutMappings.begin(), inOutMappings.end(), [&fileName](const MaterialMapping& m)
    {
        return m.source == MaterialSourceKind::SK && m.sourceFile == fileName;
    }), inOutMappings.end());

    return Load_MaterialRefs_FromSkJson(skPath, root, inOutMappings, inOutHeuristics);
}

bool MaterialCreateHelper::Load_SmFile(const wstring& smFilePath, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>&)
{
    // SM도 SK랑 처리 방식은 같고, source만 다르다
    json root{};
    const fs::path smPath(smFilePath);
    if (!Try_LoadJson(smPath, root))
        return false;

    const string fileName = String::ToString(smPath.filename().wstring());
    inOutMappings.erase(remove_if(inOutMappings.begin(), inOutMappings.end(), [&fileName](const MaterialMapping& m)
    {
        return m.source == MaterialSourceKind::SM && m.sourceFile == fileName;
    }), inOutMappings.end());

    return Load_MaterialRefs_FromSmJson(smPath, root, inOutMappings);
}

bool MaterialCreateHelper::Load_FxFile(const wstring& fxFilePath, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>&)
{
    // FX는 emitter renderer 단위로 mapping이 여러 개 나온다
    json root{};
    const fs::path fxPath(fxFilePath);
    if (!Try_LoadJson(fxPath, root))
        return false;

    const string fileName = String::ToString(fxPath.filename().wstring());
    inOutMappings.erase(remove_if(inOutMappings.begin(), inOutMappings.end(), [&fileName](const MaterialMapping& m)
    {
        return m.source == MaterialSourceKind::FX && m.sourceFile == fileName;
    }), inOutMappings.end());

    return Load_MaterialRefs_FromFxJson(fxPath, root, inOutMappings);
}

bool MaterialCreateHelper::Load_MaterialRefs_FromSkJson(const fs::path& skPath, const json& root, vector<MaterialMapping>& inOutMappings, vector<HeuristicNote>& inOutHeuristics)
{
    // SK 덤프는 SkeletalMaterials가 어디에 붙어있을지 매번 다를 수 있다
    // 그래서 root 배열을 훑으면서 아래 후보를 순서대로 찾는다
    // 1. item 내부의 SkeletalMaterials StaticMaterials Materials
    // 2. item.Properties 내부의 SkeletalMaterials StaticMaterials Materials

    // 그리고 LODModels가 있는 쪽을 우선한다
    // LODModels가 있는 엔트리가 보통 실제 메시 데이터에 더 가깝게 붙어있는 경우가 많았다
    if (!root.is_array())
        return false;

    const json* skeletalMaterialsPtr = nullptr;
    string pickedKey{};
    bool pickedFromProps = false;
    bool pickedHasLodModels = false;
    int32 pickedItemIndex = -1;

    auto tryPickMaterialsArray = [](const json& container, string& outKey) -> const json*
    {
        // 키 이름은 덤프 방식에 따라 달라질 수 있어서 몇 개를 같이 본다
        static const char* keys[] = { "SkeletalMaterials", "StaticMaterials", "Materials" };
        for (const char* key : keys)
        {
            if (!container.is_object())
                return nullptr;

            auto it = container.find(key);
            if (it == container.end())
                continue;

            if (it->is_array() && !it->empty())
            {
                outKey = key;
                return &(*it);
            }
        }
        return nullptr;
    };

    const json* bestMaterialsPtr = nullptr;
    bool bestHasLodModels = false;
    string bestPickedKey{};
    bool bestPickedFromProps = false;
    int32 bestItemIndex = -1;

    int32 itemIndex = 0;
    for (const auto& item : root)
    {
        if (!item.is_object())
        {
            ++itemIndex;
            continue;
        }

        const json& props = item.value("Properties", json::object());

        string keyInItem{};
        string keyInProps{};
        const json* picked = tryPickMaterialsArray(item, keyInItem);
        bool fromProps = false;
        if (picked == nullptr)
        {
            picked = tryPickMaterialsArray(props, keyInProps);
            fromProps = true;
        }
        if (picked == nullptr)
        {
            ++itemIndex;
            continue;
        }

        bool hasLodModels = false;
        {
            auto hasNonEmptyArray = [](const json& container, const char* key) -> bool
            {
                if (!container.is_object())
                    return false;
                auto it = container.find(key);
                return it != container.end() && it->is_array() && !it->empty();
            };

            hasLodModels =
                hasNonEmptyArray(item, "LODModels") ||
                hasNonEmptyArray(props, "LODModels");
        }

        if (bestMaterialsPtr == nullptr)
        {
            bestMaterialsPtr = picked;
            bestHasLodModels = hasLodModels;
            bestPickedKey = fromProps ? keyInProps : keyInItem;
            bestPickedFromProps = fromProps;
            bestItemIndex = itemIndex;
            if (bestHasLodModels)
                break;

            ++itemIndex;
            continue;
        }

        if (!bestHasLodModels && hasLodModels)
        {
            bestMaterialsPtr = picked;
            bestHasLodModels = hasLodModels;
            bestPickedKey = fromProps ? keyInProps : keyInItem;
            bestPickedFromProps = fromProps;
            bestItemIndex = itemIndex;
            break;
        }

        ++itemIndex;
    }

    skeletalMaterialsPtr = bestMaterialsPtr;
    pickedHasLodModels = bestHasLodModels;
    pickedKey = bestPickedKey;
    pickedFromProps = bestPickedFromProps;
    pickedItemIndex = bestItemIndex;

    if (skeletalMaterialsPtr == nullptr || !skeletalMaterialsPtr->is_array() || skeletalMaterialsPtr->empty())
        return false;

    {
        // 어떤 후보를 골랐는지 나중에 다시 확인할 수 있게 남겨둔다
        HeuristicNote note{};
        note.kind = "sk_pick";
        note.detail = String::ToString(skPath.filename().wstring()) +
            " item=" + to_string(pickedItemIndex) +
            " key=" + pickedKey +
            " fromProps=" + to_string(pickedFromProps ? 1 : 0) +
            " hasLodModels=" + to_string(pickedHasLodModels ? 1 : 0);
        inOutHeuristics.push_back(move(note));
    }

    uint32 materialIndex = 0;
    for (const auto& mat : *skeletalMaterialsPtr)
    {
        // 배열 항목이 object가 아니면 그냥 건너뛴다
        // materialIndex는 스킵 여부와 상관없이 배열 순서를 그대로 쓴다
        if (!mat.is_object())
        {
            ++materialIndex;
            continue;
        }

        MaterialMapping mapping{};
        mapping.source = MaterialSourceKind::SK;
        mapping.sourceFile = String::ToString(skPath.filename().wstring());
        mapping.materialIndex = materialIndex;
        mapping.originalMaterialIndex = materialIndex;
        mapping.slotName.clear();

        string slotName{};
        if (Try_ReadString(mat, "MaterialSlotName", slotName))
            mapping.slotName = slotName;

        const json& mi =
            mat.contains("MaterialInterface") ? mat.value("MaterialInterface", json::object()) :
            mat.value("Material", json::object());
        // MaterialInterface가 있으면 그걸 우선으로 보고  없으면 Material 쪽을 대체 후보로 본다
        string objectName{};
        string objectPath{};
        Try_ReadString(mi, "ObjectName", objectName);
        Try_ReadString(mi, "ObjectPath", objectPath);

        mapping.material.objectName = objectName;
        mapping.material.objectPath = objectPath;
        mapping.material.assetName = Extract_AssetNameFromObjectName(objectName);
        mapping.material.kind = Resolve_MaterialRefKindFromObjectName(objectName);

        inOutMappings.push_back(move(mapping));
        ++materialIndex;
    }

    return true;
}

bool MaterialCreateHelper::Load_MaterialRefs_FromSmJson(const fs::path& smPath, const json& root, vector<MaterialMapping>& inOutMappings)
{
    // SM은 StaticMaterials 배열 아래에 슬롯 정보가 들어있다
    // 여기서 MaterialInterface를 읽어오는데, 대상은 MI일 수도 있고 마스터 Material일 수도 있다
    if (!root.is_array())
        return false;

    for (const auto& item : root)
    {
        if (!item.is_object())
            continue;

        const json& props = item.value("Properties", json::object());
        const json& staticMaterials = props.value("StaticMaterials", json::array());
        if (!staticMaterials.is_array() || staticMaterials.empty())
            continue;

        uint32 materialIndex = 0;
        for (const auto& mat : staticMaterials)
        {
            if (!mat.is_object())
            {
                ++materialIndex;
                continue;
            }

            MaterialMapping mapping{};
            mapping.source = MaterialSourceKind::SM;
            mapping.sourceFile = String::ToString(smPath.filename().wstring());
            mapping.materialIndex = materialIndex;
            mapping.originalMaterialIndex = materialIndex;
            mapping.slotName.clear();

            string slotName{};
            if (Try_ReadString(mat, "MaterialSlotName", slotName))
                mapping.slotName = slotName;

            const json& mi = mat.value("MaterialInterface", json::object());
            string objectName{};
            string objectPath{};
            Try_ReadString(mi, "ObjectName", objectName);
            Try_ReadString(mi, "ObjectPath", objectPath);

            mapping.material.objectName = objectName;
            mapping.material.objectPath = objectPath;
            mapping.material.assetName = Extract_AssetNameFromObjectName(objectName);
            mapping.material.kind = Resolve_MaterialRefKindFromObjectName(objectName);

            inOutMappings.push_back(move(mapping));
            ++materialIndex;
        }

        return true;
    }

    return false;
}

bool MaterialCreateHelper::Load_MaterialRefs_FromFxJson(const fs::path& fxPath, const json& root, vector<MaterialMapping>& inOutMappings)
{
    // FX는 Niagara 쪽 덤프라서 구조가 조금 다르다
    // 상위에 Emitters 배열이 있고, 각 emitter는 RendererProperties 배열을 가진다
    //
    // RendererProperties 항목 타입에 따라 참조 위치가 갈린다
    // SpriteRenderer는 Properties.Material
    // MeshRenderer는 Properties.OverrideMaterials 배열 안의 ExplicitMat
    if (!root.is_array())
        return false;

    for (const auto& item : root)
    {
        if (!item.is_object())
            continue;

        const json& props = item.value("Properties", json::object());
        const json& emitters = props.value("Emitters", json::array());
        if (!emitters.is_array() || emitters.empty())
            continue;

        for (const auto& emitter : emitters)
        {
            if (!emitter.is_object())
                continue;

            string emitterName{};
            {
                // emitterName은 mapping에서 구분자로 계속 쓰인다
                // 같은 fx 파일 안에서도 emitter마다 소재가 다를 수 있다
                const json& emitterProps = emitter.value("Properties", json::object());
                Try_ReadString(emitterProps, "UniqueEmitterName", emitterName);
            }

            const json& rendererPropsArr = emitter.value("RendererProperties", json::array());
            if (!rendererPropsArr.is_array())
                continue;

            uint32 rendererIndex = 0;
            for (const auto& rendererProps : rendererPropsArr)
            {
                // rendererIndex는 emitter 안에서 순번으로만 쓴다
                // 덤프 순서가 바뀌면 값도 바뀔 수 있어서, 여기서는 저장과 재현 용도로만 본다
                if (!rendererProps.is_object())
                {
                    ++rendererIndex;
                    continue;
                }

                string objectName{};
                string objectPath{};
                {
                    // Properties.Material은 renderer 기본 소재다
                    // OverrideMaterials가 없으면 이 값만으로도 충분한 케이스가 많다
                    const json& m = rendererProps.value("Properties", json::object()).value("Material", json::object());
                    Try_ReadString(m, "ObjectName", objectName);
                    Try_ReadString(m, "ObjectPath", objectPath);
                }

                if (!objectName.empty())
                {
                    // 기본 소재가 있으면 slotIndex 0으로 하나 만들어 둔다
                    MaterialMapping mapping{};
                    mapping.source = MaterialSourceKind::FX;
                    mapping.sourceFile = String::ToString(fxPath.filename().wstring());
                    mapping.emitter = emitterName;
                    mapping.rendererType = rendererProps.value("Type", string{});
                    mapping.rendererIndex = rendererIndex;
                    mapping.slotIndex = 0;
                    mapping.material.objectName = objectName;
                    mapping.material.objectPath = objectPath;
                    mapping.material.assetName = Extract_AssetNameFromObjectName(objectName);
                    mapping.material.kind = Resolve_MaterialRefKindFromObjectName(objectName);
                    inOutMappings.push_back(move(mapping));
                }

                const json& overrideMaterials = rendererProps.value("Properties", json::object()).value("OverrideMaterials", json::array());
                if (overrideMaterials.is_array())
                {
                    // OverrideMaterials는 슬롯별로 다른 소재가 들어갈 수 있다
                    // ExplicitMat만 쓰고, 다른 필드는 지금 단계에서는 보지 않는다
                    uint32 slotIndex = 0;
                    for (const auto& ov : overrideMaterials)
                    {
                        if (!ov.is_object())
                        {
                            ++slotIndex;
                            continue;
                        }

                        const json& explicitMat = ov.value("ExplicitMat", json::object());
                        string ovName{};
                        string ovPath{};
                        Try_ReadString(explicitMat, "ObjectName", ovName);
                        Try_ReadString(explicitMat, "ObjectPath", ovPath);
                        if (!ovName.empty())
                        {
                            MaterialMapping mapping{};
                            mapping.source = MaterialSourceKind::FX;
                            mapping.sourceFile = String::ToString(fxPath.filename().wstring());
                            mapping.emitter = emitterName;
                            mapping.rendererType = rendererProps.value("Type", string{});
                            mapping.rendererIndex = rendererIndex;
                            mapping.slotIndex = slotIndex;
                            mapping.material.objectName = ovName;
                            mapping.material.objectPath = ovPath;
                            mapping.material.assetName = Extract_AssetNameFromObjectName(ovName);
                            mapping.material.kind = Resolve_MaterialRefKindFromObjectName(ovName);
                            inOutMappings.push_back(move(mapping));
                        }

                        ++slotIndex;
                    }
                }

                ++rendererIndex;
            }
        }

        return true;
    }

    return false;
}

bool MaterialCreateHelper::Build_MaterialInstances(
    const vector<MaterialMapping>& mappings,
    unordered_map<string, MaterialInstanceData>& inOutInstances,
    vector<HeuristicNote>& inOutHeuristics)
{
    // 텍스처 인덱스가 없으면 resolve가 전부 실패하니까, 여기서 먼저 준비한다
    Rebuild_TextureIndexIfNeeded();

    // 버튼을 여러 번 눌러도 결과가 누적되지 않게 매번 초기화한다
    inOutInstances.clear();

    // tex_key template_pick은 Build 할 때마다 새로 쌓이는 종류라서
    // 이전 결과가 남아있으면 오해하기 쉬워서 여기서 한번 정리한다
    inOutHeuristics.erase(remove_if(inOutHeuristics.begin(), inOutHeuristics.end(), [](const HeuristicNote& n)
    {
        return n.kind == "tex_key" || n.kind == "template_pick";
    }), inOutHeuristics.end());

    // resolve 결과를 한 줄로 요약 로그 찍을 때 쓰는 카운터들이다
    // MI별로 다 찍으면 로그 너무 많아서 여기서는 누적만 해둔다
    unordered_map<string, uint32> missingBaseCounts{};
    unordered_map<string, uint32> emptyUassetCounts{};
    unordered_map<string, uint32> invalidUassetCounts{};
    unordered_map<string, uint32> indexEmptyCounts{};
    unordered_map<string, uint32> missingMiJsonCounts{};

    const bool textureIndexEmpty = (_textureIndex == nullptr) ? true : _textureIndex->empty();

    auto recordMissingBase = [&missingBaseCounts, &emptyUassetCounts, &invalidUassetCounts, &indexEmptyCounts, textureIndexEmpty](
        const char* slotLabel,
        const string& uassetPath,
        const wstring& resolvedPath)
    {
        // slotLabel은 로그 표시용 이름이다
        // uassetPath가 비어있으면 MI가 그 슬롯을 아예 안쓰는 케이스로 본다
        // base name 추출이 실패하면 uasset 문자열이 기대 형태가 아닌 케이스다
        // texture index가 비어있으면 Texture Root가 잘못 잡힌 케이스로 본다
        // resolvedPath가 비어있으면 실제 파일이 폴더 안에 없거나 이름이 다른 케이스다
        if (slotLabel == nullptr)
            slotLabel = "Unknown";

        if (uassetPath.empty())
        {
            emptyUassetCounts[slotLabel] += 1;
            return;
        }

        const string base = Extract_UAssetBaseName(uassetPath);
        if (base.empty())
        {
            invalidUassetCounts[slotLabel] += 1;
            return;
        }

        if (textureIndexEmpty)
        {
            indexEmptyCounts[base] += 1;
            return;
        }

        if (resolvedPath.empty())
            missingBaseCounts[base] += 1;
    };

    auto tryReadBool = [](const json& obj, const char* key, bool& out)
    {
        // 없는 키는 그냥 무시하고 기본값을 유지한다
        if (!obj.is_object() || key == nullptr)
            return;
        auto it = obj.find(key);
        if (it == obj.end())
            return;
        if (it->is_boolean())
            out = it->get<bool>();
    };

    auto tryReadFloat = [](const json& obj, const char* key, float& out)
    {
        // float로만 안오고 int로 오는 경우도 있어서 숫자 타입은 넓게 받는다
        if (!obj.is_object() || key == nullptr)
            return;
        auto it = obj.find(key);
        if (it == obj.end())
            return;
        if (it->is_number_float() || it->is_number_integer() || it->is_number_unsigned())
            out = it->get<float>();
    };

    auto resolveBlendMode = [&](const json& parameters, const json& baseOverrides) -> Engine::MaterialBlendMode
    {
        // BlendMode는 int로도 들어오고 문자열로도 들어온다
        // 둘 다 없으면 Opaque로 본다
        int blendModeInt = 0;
        if (parameters.is_object())
        {
            auto it = parameters.find("BlendMode");
            if (it != parameters.end() && (it->is_number_integer() || it->is_number_unsigned()))
                blendModeInt = it->get<int>();
        }

        if (blendModeInt == 1)
            return Engine::MaterialBlendMode::Masked;
        if (blendModeInt == 2)
            return Engine::MaterialBlendMode::Translucent;

        string blendModeStr{};
        if (Try_ReadString(baseOverrides, "BlendMode", blendModeStr))
        {
            if (blendModeStr.find("Masked") != string::npos)
                return Engine::MaterialBlendMode::Masked;
            if (blendModeStr.find("Translucent") != string::npos)
                return Engine::MaterialBlendMode::Translucent;
        }

        return Engine::MaterialBlendMode::Opaque;
    };

    auto templateName = [](Engine::MaterialShaderTemplate id) -> const char*
    {
        // 저장용 문자열을 만들 때만 쓴다
        switch (id)
        {
        case Engine::MaterialShaderTemplate::OpaquePBR: return "OpaquePBR";
        case Engine::MaterialShaderTemplate::MaskedPBR: return "MaskedPBR";
        case Engine::MaterialShaderTemplate::EmissivePBR: return "EmissivePBR";
        case Engine::MaterialShaderTemplate::TransparentPBR: return "TransparentPBR";
        default: return "Unknown";
        }
    };

    for (const MaterialMapping& mapping : mappings)
    {
        if (mapping.material.kind != MaterialRefKind::MI &&
            mapping.material.kind != MaterialRefKind::M)
        {
            continue;
        }

        const string assetName = mapping.material.assetName;
        if (assetName.empty())
            continue;

        // 같은 MI가 여러 곳에서 참조되어도, instance 데이터는 한번만 만들면 된다
        if (inOutInstances.contains(assetName))
            continue;

        json miRoot{};
        if (!Load_MiJsonByName(assetName, miRoot, &inOutHeuristics))
        {
            missingMiJsonCounts[assetName] += 1;
            continue;
        }

        json normalizedRoot{};

        if (miRoot.is_object() && miRoot.contains("Textures"))
        {
            normalizedRoot = miRoot;
        }
        else if (!Convert_UnrealExportMaterialJsonToSummaryJson(miRoot, normalizedRoot))
        {
            HeuristicNote note{};
            note.kind = "material_json";
            note.detail = assetName + " unsupported json root type=" + miRoot.type_name();
            inOutHeuristics.push_back(move(note));
            continue;
        }

        MaterialInstanceData inst{};
        inst.assetName = assetName;

        const json& textures = normalizedRoot.value("Textures", json::object());
        const json& parameters = normalizedRoot.value("Parameters", json::object());

        // 텍스처는 key 후보가 여러 개라서 우선순위로 하나를 고른다
        // 첫 번째가 아닌 다른 키를 썼으면 휴리스틱 노트로 남긴다
        string usedKey{};
        const string baseColorU = Pick_TextureUAssetPath(textures, { "Base Color Map", "PM_Diffuse", "Color_MAIN", "Texture" }, &usedKey);        if (!baseColorU.empty() && usedKey != "Base Color Map")
        {
            HeuristicNote note{};
            note.kind = "tex_key";
            note.detail = assetName + " BC " + usedKey;
            inOutHeuristics.push_back(move(note));
        }

        usedKey.clear();
        const string normalU = Pick_TextureUAssetPath(textures, { "Normal Map", "PM_Normals", "Normal_MAIN" }, &usedKey);
        if (!normalU.empty() && usedKey != "Normal Map")
        {
            HeuristicNote note{};
            note.kind = "tex_key";
            note.detail = assetName + " N " + usedKey;
            inOutHeuristics.push_back(move(note));
        }

        const string ormU = Pick_TextureUAssetPath(textures, { "ORM Map" });

        usedKey.clear();
        const string emissiveU = Pick_TextureUAssetPath(textures, { "Glow_Map", "PM_Emissive" }, &usedKey);
        if (!emissiveU.empty() && usedKey != "Glow_Map")
        {
            HeuristicNote note{};
            note.kind = "tex_key";
            note.detail = assetName + " E " + usedKey;
            inOutHeuristics.push_back(move(note));
        }

        usedKey.clear();
        const string opacityMaskU = Pick_TextureUAssetPath(
            textures,
            { "Opacity Mask Map", "OpacityMask", "Opacity Mask", "Opacity_Map", "Mask_Tiling_Normal", "Texture" },
            &usedKey);
        if (!opacityMaskU.empty() && usedKey != "Opacity Mask Map")
        {
            HeuristicNote note{};
            note.kind = "tex_key";
            note.detail = assetName + " OpacityMask " + usedKey;
            inOutHeuristics.push_back(move(note));
        }

        // uasset 경로 문자열을 실제 이미지 파일 경로로 바꾼다
        // 텍스처 루트가 맞아야 여기서 제대로 resolve 된다
        inst.baseColorPath = Resolve_TextureFileByUAsset(baseColorU);
        inst.normalPath = Resolve_TextureFileByUAsset(normalU);
        inst.ormPath = Resolve_TextureFileByUAsset(ormU);
        inst.emissivePath = Resolve_TextureFileByUAsset(emissiveU);
        inst.opacityMaskPath = Resolve_TextureFileByUAsset(opacityMaskU);

        // 여기 단계에서는 resolve된 슬롯만 덮어쓰는 쪽이 안전하다
        // 경로가 비어있는 건 미해결이거나 해당 슬롯이 없을 수 있어서, 기본은 false로 둔다
        inst.baseColorOverride = !inst.baseColorPath.empty();
        inst.normalOverride = !inst.normalPath.empty();
        inst.ormOverride = !inst.ormPath.empty();
        inst.emissiveOverride = !inst.emissivePath.empty();
        inst.opacityMaskOverride = !inst.opacityMaskPath.empty();

        // 누락 원인을 한 번에 보기 좋게 요약하기 위해 카운트를 쌓는다
        recordMissingBase("BC", baseColorU, inst.baseColorPath);
        recordMissingBase("N", normalU, inst.normalPath);
        recordMissingBase("ORM", ormU, inst.ormPath);
        recordMissingBase("E", emissiveU, inst.emissivePath);
        recordMissingBase("OpacityMask", opacityMaskU, inst.opacityMaskPath);

        // 렌더 상태는 Parameters.Properties.BasePropertyOverrides 아래에 많이 몰려있다
        // 없는 경우도 있으니 기본값 유지에 가깝게 처리한다
        const json& props = parameters.value("Properties", json::object());
        const json& baseOverrides = props.value("BasePropertyOverrides", json::object());

        bool twoSided = false;
        tryReadBool(baseOverrides, "TwoSided", twoSided);

        float opacityClip = inst.renderState.opacityMaskClipValue;
        tryReadFloat(baseOverrides, "OpacityMaskClipValue", opacityClip);

        inst.renderState.twoSided = twoSided;
        inst.renderState.blendMode = resolveBlendMode(parameters, baseOverrides);
        inst.renderState.opacityMaskClipValue = opacityClip;

        inst.cb.opacityMaskClipValue = opacityClip;

        {
            // GlowColor는 베이스 컬러 tint로 쓰지 않고, emissive 컬러로만 사용한다
            const json& colors = parameters.value("Colors", json::object());
            const json& glow = colors.value("GlowColor", json::object());
            float r = 1.f, g = 1.f, b = 1.f, a = 0.f;
            tryReadFloat(glow, "R", r);
            tryReadFloat(glow, "G", g);
            tryReadFloat(glow, "B", b);
            tryReadFloat(glow, "A", a);
            inst.cb.emissiveColor = Vec4(r, g, b, a);
        }

        {
            // Scalars는 지금 단계에서 필요한 것만 골라서 넣는다
            // 나머지는 셰이더 확장하면서 점점 늘릴 수 있다
            const json& scalars = parameters.value("Scalars", json::object());
            float glow = 0.f;
            tryReadFloat(scalars, "Value_Glow", glow);
            inst.cb.emissiveIntensity = glow;

            // 기본값(1.0) 그대로 두면 Emissive 텍스처가 있는 머테리얼이
            // 별도 의도 없이도 baseColor에 더해져 색이 망가지는 경우가 많다.
            // Emissive는 intensity/명시 파라미터가 있을 때만 켜지도록 기본값은 0으로 둔다.
            inst.cb.emissiveMaskMul = 0.f;
            tryReadFloat(scalars, "Glow_Mask_Mult", inst.cb.emissiveMaskMul);
            tryReadFloat(scalars, "GlowMaskMul", inst.cb.emissiveMaskMul);

            float normalStrength = 1.f;
            tryReadFloat(scalars, "NormalStrength", normalStrength);
            inst.cb.normalStrength = normalStrength;
        }

        Engine::MaterialShaderTemplate pickedTemplate = Engine::MaterialShaderTemplate::OpaquePBR;
        string templateReason = "blend opaque";
        // 템플릿은 지금은 간단하게 분류한다
        // masked가 보이면 masked로, translucent면 transparent로, 그 외는 emissive 여부를 본다
        if (inst.renderState.blendMode == Engine::MaterialBlendMode::Masked)
        {
            pickedTemplate = Engine::MaterialShaderTemplate::MaskedPBR;
            templateReason = "blend masked";
        }
        else if (inst.renderState.blendMode == Engine::MaterialBlendMode::Translucent)
        {
            pickedTemplate = Engine::MaterialShaderTemplate::TransparentPBR;
            templateReason = "blend translucent";
        }
        else if (!inst.emissivePath.empty() || inst.cb.emissiveIntensity > 0.f)
        {
            pickedTemplate = Engine::MaterialShaderTemplate::EmissivePBR;
            templateReason = "emissive";
        }

        inst.templateId = pickedTemplate;

        {
            // 어떤 근거로 템플릿을 골랐는지 남겨둔다
            HeuristicNote note{};
            note.kind = "template_pick";
            note.detail = assetName + " " + templateName(pickedTemplate) + " " + templateReason;
            inOutHeuristics.push_back(move(note));
        }

        inOutInstances.emplace(assetName, move(inst));
    }

#ifdef _DEBUG
    if (!missingBaseCounts.empty() || !emptyUassetCounts.empty() || !invalidUassetCounts.empty() || !indexEmptyCounts.empty() || !missingMiJsonCounts.empty())
    {
        // 로그 스팸을 막으려고 MI별 상세 로그 대신 요약만 한번 찍는다
        // 여기서 가장 많이 보이는 항목부터 보면 대개 Texture Root 문제나 파일명 불일치가 바로 보인다
        constexpr size_t kMaxPrint = 16;

        string msg = "[Material_View] Missing textures summary: ";

        if (!emptyUassetCounts.empty())
        {
            vector<pair<string, uint32>> empties{};
            empties.reserve(emptyUassetCounts.size());
            for (const auto& kv : emptyUassetCounts)
                empties.push_back(kv);

            sort(empties.begin(), empties.end(), [](const auto& a, const auto& b)
            {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
            });

            msg += "empty_uasset=[";
            const size_t ecount = (std::min)(empties.size(), kMaxPrint);
            for (size_t i = 0; i < ecount; ++i)
            {
                msg += empties[i].first + " x" + to_string(empties[i].second);
                if (i + 1 < ecount) msg += ", ";
            }
            if (empties.size() > kMaxPrint) msg += " ...";
            msg += "] ";
        }

        if (!indexEmptyCounts.empty())
        {
            vector<pair<string, uint32>> items{};
            items.reserve(indexEmptyCounts.size());
            for (const auto& kv : indexEmptyCounts)
                items.push_back(kv);

            sort(items.begin(), items.end(), [](const auto& a, const auto& b)
            {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
            });

            msg += "index_empty=[";
            const size_t count = (std::min)(items.size(), kMaxPrint);
            for (size_t i = 0; i < count; ++i)
            {
                msg += items[i].first + " x" + to_string(items[i].second);
                if (i + 1 < count) msg += ", ";
            }
            if (items.size() > kMaxPrint) msg += " ...";
            msg += "] ";
        }

        if (!invalidUassetCounts.empty())
        {
            vector<pair<string, uint32>> items{};
            items.reserve(invalidUassetCounts.size());
            for (const auto& kv : invalidUassetCounts)
                items.push_back(kv);

            sort(items.begin(), items.end(), [](const auto& a, const auto& b)
            {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
            });

            msg += "invalid_uasset=[";
            const size_t count = (std::min)(items.size(), kMaxPrint);
            for (size_t i = 0; i < count; ++i)
            {
                msg += items[i].first + " x" + to_string(items[i].second);
                if (i + 1 < count) msg += ", ";
            }
            if (items.size() > kMaxPrint) msg += " ...";
            msg += "] ";
        }

        if (!missingBaseCounts.empty())
        {
            vector<pair<string, uint32>> items{};
            items.reserve(missingBaseCounts.size());
            for (const auto& kv : missingBaseCounts)
                items.push_back(kv);

            sort(items.begin(), items.end(), [](const auto& a, const auto& b)
            {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
            });

            msg += "missing_files=[";
            const size_t count = (std::min)(items.size(), kMaxPrint);
            for (size_t i = 0; i < count; ++i)
            {
                msg += items[i].first + " x" + to_string(items[i].second);
                if (i + 1 < count) msg += ", ";
            }
            if (items.size() > kMaxPrint) msg += " ...";
            msg += "] ";
        }

        if (!missingMiJsonCounts.empty())
        {
            vector<pair<string, uint32>> items{};
            items.reserve(missingMiJsonCounts.size());
            for (const auto& kv : missingMiJsonCounts)
                items.push_back(kv);

            sort(items.begin(), items.end(), [](const auto& a, const auto& b)
            {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
            });

            msg += "missing_mi=[";
            const size_t count = (std::min)(items.size(), kMaxPrint);
            for (size_t i = 0; i < count; ++i)
            {
                msg += items[i].first + " x" + to_string(items[i].second);
                if (i + 1 < count) msg += ", ";
            }
            if (items.size() > kMaxPrint) msg += " ...";
            msg += "]";
        }

        LOG_WARN(msg.c_str());
    }
#endif

    return true;
}

NS_END

