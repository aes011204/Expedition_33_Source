#include "UI_RuntimeLoader.h"

#include "GameInstance.h"
#include "UI_Image.h"
#include "UI_Text.h"

namespace
{
    // -----------------------------
    // Runtime 내부용 파싱 모델
    // -----------------------------
    enum class RuntimeUIType : uint8
    {
        Image,
        Text,
        Panel,
        Button,
        Unknown,
    };

    struct RuntimeUIObject
    {
        int32 id = 0;
        int32 parentId = 0;
        RuntimeUIType type = RuntimeUIType::Unknown;
        string name{};

        Vec2 position{ 0.f, 0.f };
        Vec2 worldPosition{ 0.f, 0.f };
        Vec2 size{ 100.f, 100.f };
        Vec2 scale{ 1.f, 1.f };
        float rotationRadians = 0.f;
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        int32 sortOrder = 0;

        string text{};
        float fontSize = 32.f;
        UI_Text::TextAlign textAlign = UI_Text::TextAlign::Left;
        bool textWrap = false;
        string fontTag{};
        string fontGuid{};
        string fontPath{};

        string textureGuid{};
        string texturePath{};
    };

    struct RuntimeUIDocument
    {
        string canvasName = "MainCanvas";
        Vec2 canvasSize{ 1600.f, 900.f };
        vector<RuntimeUIObject> objects{};
    };

    // -----------------------------
    // 기본 json 유틸
    // -----------------------------
    string Read_String(const json& item, const char* key, const string& defaultValue = "")
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_string())
            return defaultValue;

        return iter->get<string>();
    }

    int32 Read_Int(const json& item, const char* key, int32 defaultValue = 0)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_number_integer())
            return defaultValue;

        return iter->get<int32>();
    }

    float Read_Float(const json& item, const char* key, float defaultValue = 0.f)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_number())
            return defaultValue;

        return iter->get<float>();
    }

    bool Read_Bool(const json& item, const char* key, bool defaultValue = false)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_boolean())
            return defaultValue;

        return iter->get<bool>();
    }

    Vec2 Read_Vec2(const json& item, const char* key, const Vec2& defaultValue)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 2)
            return defaultValue;

        return Vec2(
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y);
    }

    Vec4 Read_Color(const json& item, const char* key, const Vec4& defaultValue)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 4)
            return defaultValue;

        Vec4 color(
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y,
            iter->at(2).is_number() ? iter->at(2).get<float>() : defaultValue.z,
            iter->at(3).is_number() ? iter->at(3).get<float>() : defaultValue.w);

        // UI_Editor 저장값이 0~255일 수 있으므로 런타임에서 0~1로 정규화한다.
        if (color.x > 1.f || color.y > 1.f || color.z > 1.f || color.w > 1.f)
            color *= 1.f / 255.f;

        return color;
    }

    bool StartsWith(const string& value, const string& prefix)
    {
        return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
    }

    RuntimeUIType Parse_UIType(const string& typeName)
    {
        if ("Image" == typeName)
            return RuntimeUIType::Image;
        if ("Text" == typeName)
            return RuntimeUIType::Text;
        if ("Panel" == typeName)
            return RuntimeUIType::Panel;
        if ("Button" == typeName)
            return RuntimeUIType::Button;

        return RuntimeUIType::Unknown;
    }

    UI_Text::TextAlign Read_TextAlign(const json& item)
    {
        string align = Read_String(item, "align");
        if (align.empty())
            align = Read_String(item, "textAlign");

        align = String::ToLowerCopy(align);

        if ("center" == align)
            return UI_Text::TextAlign::Center;
        if ("right" == align)
            return UI_Text::TextAlign::Right;

        return UI_Text::TextAlign::Left;
    }

    wstring Resolve_RuntimeResourcePath(const string& runtimePath)
    {
        if (runtimePath.empty())
            return {};

        string normalizedPath = runtimePath;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        const fs::path inputPath = fs::path(String::ToWString(normalizedPath));
        if (inputPath.is_absolute())
            return inputPath.lexically_normal().wstring();

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return fs::absolute(inputPath).lexically_normal().wstring();

        const string loweredPath = String::ToLowerCopy(normalizedPath);
        string resourceRelativePath = normalizedPath;

        constexpr const char* resourcePrefix = "../bin/resources/";
        constexpr const char* binResourcePrefix = "bin/resources/";
        constexpr const char* resourcesPrefix = "resources/";

        if (StartsWith(loweredPath, resourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcePrefix));
        else if (StartsWith(loweredPath, binResourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(binResourcePrefix));
        else if (StartsWith(loweredPath, resourcesPrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcesPrefix));

        return (fs::path(assetRoot) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    wstring Resolve_JsonAssetPath(const string& guid, const string& path)
    {
        // guid가 있으면 guid를 우선 신뢰한다.
        if (!guid.empty())
        {
            const wstring guidPath = GAME->Resolve_AssetPath(guid);
            if (!guidPath.empty() && fs::exists(guidPath))
                return guidPath;
        }

        // guid 해석이 안 되면 저장된 경로를 fallback으로 사용한다.
        return Resolve_RuntimeResourcePath(path);
    }

    bool Ensure_FontLoaded(const wstring& fontTag, const wstring& fontPath, unordered_set<wstring>& loadedFontTags)
    {
        if (fontTag.empty() || fontPath.empty())
            return false;

        // 같은 로더 실행 중 같은 tag를 가진 폰트는 한 번만 등록한다.
        if (loadedFontTags.find(fontTag) != loadedFontTags.end())
            return true;

        if (FAILED(GAME->Add_Font(fontTag, fontPath.c_str())))
        {
            LOG_ERROR("UI RuntimeLoader Add_Font failed. tag={}, path={}",
                String::ToString(fontTag),
                String::ToString(fontPath));
            return false;
        }

        loadedFontTags.insert(fontTag);
        return true;
    }

    bool Load_JsonRoot(const wstring& jsonPath, json& outRoot)
    {
        if (jsonPath.empty())
        {
            LOG_ERROR("UI RuntimeLoader load failed: empty json path");
            return false;
        }

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
        {
            LOG_ERROR("UI RuntimeLoader load failed: asset root is empty");
            return false;
        }

        const fs::path fullPath = (fs::path(assetRoot) / jsonPath).lexically_normal();
        if (!fs::exists(fullPath))
        {
            LOG_WARN("UI RuntimeLoader json not found: {}", String::ToString(fullPath.wstring()));
            return false;
        }

        try
        {
            ifstream file(fullPath);
            if (!file.is_open())
            {
                LOG_ERROR("UI RuntimeLoader failed to open json: {}", String::ToString(fullPath.wstring()));
                return false;
            }

            file >> outRoot;
            return true;
        }
        catch (const exception& exception)
        {
            LOG_ERROR("UI RuntimeLoader json parse failed: {} ({})",
                exception.what(),
                String::ToString(fullPath.wstring()));
            return false;
        }
    }

    void Parse_Canvas(const json& root, RuntimeUIDocument& outDocument)
    {
        const auto canvasIter = root.find("canvas");
        if (canvasIter == root.end() || !canvasIter->is_object())
            return;

        outDocument.canvasName = Read_String(*canvasIter, "name", "MainCanvas");
        outDocument.canvasSize = Vec2(
            max(1.f, Read_Float(*canvasIter, "width", 1600.f)),
            max(1.f, Read_Float(*canvasIter, "height", 900.f)));
    }

    RuntimeUIObject Parse_Object(const json& item)
    {
        RuntimeUIObject object{};

        object.id = Read_Int(item, "id", 0);
        object.parentId = Read_Int(item, "parentId", 0);
        object.name = Read_String(item, "name", "");
        object.type = Parse_UIType(Read_String(item, "type", ""));

        object.position = Read_Vec2(item, "position", Vec2(0.f, 0.f));
        object.size = Read_Vec2(item, "size", Vec2(100.f, 100.f));
        object.scale = Read_Vec2(item, "scale", Vec2(1.f, 1.f));
        object.rotationRadians = Read_Float(item, "rotation", Read_Float(item, "rotationRadians", 0.f));
        object.color = Read_Color(item, "color", Vec4(1.f, 1.f, 1.f, 1.f));
        object.sortOrder = Read_Int(item, "sortOrder", 0);

        object.text = Read_String(item, "text", "");
        object.fontSize = max(4.f, Read_Float(item, "fontSize", 32.f));
        object.textAlign = Read_TextAlign(item);
        object.textWrap = Read_Bool(item, "textWrap", Read_Bool(item, "wrap", false));
        object.fontTag = Read_String(item, "fontTag", "");
        object.fontGuid = Read_String(item, "fontGuid", "");
        object.fontPath = Read_String(item, "fontPath", "");

        object.textureGuid = Read_String(item, "textureGuid", "");
        object.texturePath = Read_String(item, "texturePath", "");

        return object;
    }

    Vec2 Resolve_WorldPosition(RuntimeUIDocument& document, RuntimeUIObject& object, unordered_set<int32>& resolvingIds)
    {
        if (object.parentId == 0)
        {
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        if (object.parentId == object.id || resolvingIds.find(object.id) != resolvingIds.end())
        {
            LOG_WARN("UI RuntimeLoader object {} has cyclic parent. Using canvas space.", object.id);
            object.parentId = 0;
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        const int32 originalParentId = object.parentId;
        RuntimeUIObject* parentObject = nullptr;
        for (RuntimeUIObject& candidate : document.objects)
        {
            if (candidate.id == originalParentId)
            {
                parentObject = &candidate;
                break;
            }
        }

        if (nullptr == parentObject)
        {
            LOG_WARN("UI RuntimeLoader object {} has missing parent {}. Using canvas space.", object.id, object.parentId);
            object.parentId = 0;
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        resolvingIds.insert(object.id);
        const Vec2 parentWorldPosition = Resolve_WorldPosition(document, *parentObject, resolvingIds);
        resolvingIds.erase(object.id);

        if (object.parentId != originalParentId)
        {
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        object.worldPosition = Vec2(parentWorldPosition.x + object.position.x, parentWorldPosition.y + object.position.y);
        return object.worldPosition;
    }

    void Resolve_WorldPositions(RuntimeUIDocument& document)
    {
        unordered_set<int32> resolvingIds;

        for (RuntimeUIObject& object : document.objects)
        {
            resolvingIds.clear();
            Resolve_WorldPosition(document, object, resolvingIds);
        }
    }

    bool Parse_Document(const json& root, RuntimeUIDocument& outDocument)
    {
        if (!root.is_object())
        {
            LOG_ERROR("UI RuntimeLoader root is not object");
            return false;
        }

        Parse_Canvas(root, outDocument);

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
        {
            LOG_WARN("UI RuntimeLoader json has no objects array");
            return true;
        }

        outDocument.objects.clear();
        outDocument.objects.reserve(objectsIter->size());

        for (const json& item : *objectsIter)
        {
            if (!item.is_object())
                continue;

            outDocument.objects.push_back(Parse_Object(item));
        }

        stable_sort(outDocument.objects.begin(), outDocument.objects.end(),
            [](const RuntimeUIObject& lhs, const RuntimeUIObject& rhs)
            {
                if (lhs.sortOrder != rhs.sortOrder)
                    return lhs.sortOrder < rhs.sortOrder;

                return lhs.id < rhs.id;
            });

        Resolve_WorldPositions(outDocument);

        return true;
    }

    HRESULT Spawn_Image(uint32 levelIndex, const RuntimeUIObject& object)
    {
        const wstring texturePath = Resolve_JsonAssetPath(object.textureGuid, object.texturePath);
        if (texturePath.empty() || !fs::exists(texturePath))
        {
            LOG_WARN("UI RuntimeLoader image skipped: missing texture. id={}", object.id);
            return S_OK;
        }

        UI_Image::UI_IMAGE_DESC desc{};
        desc.texturePath = texturePath;
        desc.color = object.color;
        desc.transformDesc.position = object.worldPosition;
        desc.transformDesc.size = object.size;
        desc.transformDesc.scale = object.scale;
        desc.transformDesc.rotationRadians = object.rotationRadians;
        desc.sortOrder = object.sortOrder;
        desc.isOpenOnCreate = true;

        // object id 기반 prototype tag를 유지해 기존 동작을 최대한 그대로 둔다.
        const wstring prototypeTag = L"UIJson_Image_" + to_wstring(object.id);

        if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Image::Create())))
            return E_FAIL;

        if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Spawn_Text(uint32 levelIndex, const RuntimeUIObject& object, unordered_set<wstring>& loadedFontTags)
    {
        const wstring fontPath = Resolve_JsonAssetPath(object.fontGuid, object.fontPath);
        if (fontPath.empty() || !fs::exists(fontPath))
        {
            LOG_WARN("UI RuntimeLoader text skipped: missing font. id={}", object.id);
            return S_OK;
        }

        // 저장된 tag가 있으면 우선 사용하고, 없으면 파일명 stem을 tag로 만든다.
        const wstring fontTag = object.fontTag.empty()
            ? fs::path(fontPath).stem().wstring()
            : String::ToWString(object.fontTag);

        if (!Ensure_FontLoaded(fontTag, fontPath, loadedFontTags))
            return E_FAIL;

        Vec2 scale = object.scale;
        scale.x *= object.fontSize / 32.f;

        UI_Text::UI_TEXT_DESC desc{};
        desc.text = String::ToWString(object.text);
        desc.fontTag = fontTag;
        desc.color = object.color;
        desc.textAlign = object.textAlign;
        desc.textWrap = object.textWrap;
        desc.transformDesc.position = object.worldPosition;
        desc.transformDesc.size = object.size;
        desc.transformDesc.scale = scale;
        desc.transformDesc.rotationRadians = object.rotationRadians;
        desc.sortOrder = object.sortOrder;
        desc.isOpenOnCreate = true;

        const wstring prototypeTag = L"UIJson_Text_" + to_wstring(object.id);

        if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Text::Create())))
            return E_FAIL;

        if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Spawn_Object(uint32 levelIndex, const RuntimeUIObject& object, unordered_set<wstring>& loadedFontTags)
    {
        switch (object.type)
        {
        case RuntimeUIType::Image:
            return Spawn_Image(levelIndex, object);

        case RuntimeUIType::Text:
            return Spawn_Text(levelIndex, object, loadedFontTags);

        case RuntimeUIType::Panel:
            // 추후에 container/panel 전용 runtime object 추가 예정.
            // 지금 단계에서는 json 파싱은 허용하되 렌더 생성은 건너뛴다.
            LOG_WARN("UI RuntimeLoader panel skipped (not supported yet). id={}", object.id);
            return S_OK;

        case RuntimeUIType::Button:
            // 버튼은 렌더뿐 아니라 상호작용 모델도 필요하므로 지금은 스킵한다.
            LOG_WARN("UI RuntimeLoader button skipped (not supported yet). id={}", object.id);
            return S_OK;

        case RuntimeUIType::Unknown:
        default:
            LOG_WARN("UI RuntimeLoader unknown type skipped. id={}, name={}", object.id, object.name);
            return S_OK;
        }
    }
}

HRESULT UI_RuntimeLoader::Load_Document(const UI_LoadDesc& desc)
{
    json root{};
    if (!Load_JsonRoot(desc.jsonPath, root))
        return E_FAIL;

    RuntimeUIDocument document{};
    if (!Parse_Document(root, document))
        return E_FAIL;

    unordered_set<wstring> loadedFontTags;

    for (const RuntimeUIObject& object : document.objects)
    {
        const HRESULT hr = Spawn_Object(desc.levelIndex, object, loadedFontTags);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}
