#include "pch.h"
#include "UI_DocumentSerializer.h"

namespace UIEditor
{
    namespace
    {
        constexpr int32 kCurrentUIJsonVersion = 1;
        constexpr const char* kDefaultCanvasName = "MainCanvas";
        constexpr const char* kDefaultFontPath = "../Bin/Resources/Fonts/33_MainKR_Font.spritefont";

        const char* Get_ObjectTypeName(UI_ObjectType objectType)
        {
            switch (objectType)
            {
            case UI_ObjectType::Image:
                return "Image";
            case UI_ObjectType::Text:
                return "Text";
            case UI_ObjectType::Panel:
                return "Panel";
            case UI_ObjectType::Button:
                return "Button";
            default:
                return "Image";
            }
        }

        bool Try_ParseObjectType(const string& typeName, UI_ObjectType& objectType)
        {
            if (typeName == "Image")
            {
                objectType = UI_ObjectType::Image;
                return true;
            }

            if (typeName == "Text")
            {
                objectType = UI_ObjectType::Text;
                return true;
            }

            if (typeName == "Panel")
            {
                objectType = UI_ObjectType::Panel;
                return true;
            }

            if (typeName == "Button")
            {
                objectType = UI_ObjectType::Button;
                return true;
            }

            return false;
        }

        const char* Get_TextAlignName(UI_TextAlign textAlign)
        {
            switch (textAlign)
            {
            case UI_TextAlign::Left:
                return "Left";
            case UI_TextAlign::Center:
                return "Center";
            case UI_TextAlign::Right:
                return "Right";
            default:
                return "Left";
            }
        }

        bool Try_ParseTextAlign(const string& alignName, UI_TextAlign& textAlign)
        {
            if (alignName == "Left" || alignName == "left")
            {
                textAlign = UI_TextAlign::Left;
                return true;
            }

            if (alignName == "Center" || alignName == "center" || alignName == "Middle" || alignName == "middle")
            {
                textAlign = UI_TextAlign::Center;
                return true;
            }

            if (alignName == "Right" || alignName == "right")
            {
                textAlign = UI_TextAlign::Right;
                return true;
            }

            return false;
        }

        string Make_DefaultObjectName(UI_ObjectType objectType)
        {
            return Get_ObjectTypeName(objectType);
        }

        string Make_FontTagFromPath(const string& fontPath)
        {
            if (fontPath.empty())
                return {};

            return String::ToString(fs::path(String::ToWString(fontPath)).stem().wstring());
        }

        int32 ColorFloatToByte(float value)
        {
            value = min(max(value, 0.f), 1.f);
            return static_cast<int32>(roundf(value * 255.f));
        }

        float ColorJsonToFloat(const json& value, float defaultValue)
        {
            if (!value.is_number())
                return defaultValue;

            const float number = value.get<float>();
            if (number > 1.f)
                return min(max(number / 255.f, 0.f), 1.f);

            return min(max(number, 0.f), 1.f);
        }

        string Read_String(const json& item, const char* key, const string& defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_string())
                return defaultValue;

            return iter->get<string>();
        }

        ObjectId Read_ObjectId(const json& item, const char* key, ObjectId defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_number_integer())
                return defaultValue;

            const int64 value = iter->get<int64>();
            if (value < 0 || value > numeric_limits<ObjectId>::max())
                return defaultValue;

            return static_cast<ObjectId>(value);
        }

        int32 Read_Int(const json& item, const char* key, int32 defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_number_integer())
                return defaultValue;

            return iter->get<int32>();
        }

        float Read_Float(const json& item, const char* key, float defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_number())
                return defaultValue;

            return iter->get<float>();
        }

        bool Read_Bool(const json& item, const char* key, bool defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_boolean())
                return defaultValue;

            return iter->get<bool>();
        }

        ImVec2 Read_Vec2(const json& item, const char* key, const ImVec2& defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_array() || iter->size() < 2)
                return defaultValue;

            ImVec2 value = defaultValue;
            if ((*iter)[0].is_number())
                value.x = (*iter)[0].get<float>();
            if ((*iter)[1].is_number())
                value.y = (*iter)[1].get<float>();

            return value;
        }

        ImVec4 Read_Color(const json& item, const char* key, const ImVec4& defaultValue)
        {
            const auto iter = item.find(key);
            if (iter == item.end() || !iter->is_array() || iter->size() < 4)
                return defaultValue;

            return ImVec4(
                ColorJsonToFloat((*iter)[0], defaultValue.x),
                ColorJsonToFloat((*iter)[1], defaultValue.y),
                ColorJsonToFloat((*iter)[2], defaultValue.z),
                ColorJsonToFloat((*iter)[3], defaultValue.w));
        }

        json Vec2_ToJson(const ImVec2& value)
        {
            return json::array({ value.x, value.y });
        }

        json Color_ToJson(const ImVec4& value)
        {
            return json::array({
                ColorFloatToByte(value.x),
                ColorFloatToByte(value.y),
                ColorFloatToByte(value.z),
                ColorFloatToByte(value.w)
            });
        }

        UI_Object Make_DefaultObject(UI_ObjectType objectType)
        {
            UI_Object object = {};
            object.type = objectType;
            object.name = Make_DefaultObjectName(objectType);
            object.position = ImVec2(0.f, 0.f);
            object.size = ImVec2(100.f, 100.f);
            object.scale = ImVec2(1.f, 1.f);
            object.rotationRadians = 0.f;
            object.sortOrder = 0;
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);

            if (objectType == UI_ObjectType::Text)
            {
                object.text = "Text";
                object.fontSize = 32.f;
                object.textAlign = UI_TextAlign::Left;
                object.textWrap = false;
                object.fontPath = kDefaultFontPath;
                object.fontTag = Make_FontTagFromPath(object.fontPath);
            }

            return object;
        }
    }

    bool UI_DocumentSerializer::Save_ToJsonFile(const UI_Document& document, const wstring& filePath)
    {
        if (filePath.empty())
        {
            LOG_ERROR("UI Document save failed: empty path");
            return false;
        }

        try
        {
            ofstream file{ fs::path(filePath) };
            if (!file.is_open())
            {
                LOG_ERROR("UI Document save failed: cannot open {}", String::ToString(filePath));
                return false;
            }

            file << To_Json(document).dump(4);
            if (!file.good())
            {
                LOG_ERROR("UI Document save failed while writing {}", String::ToString(filePath));
                return false;
            }

            LOG_INFO("UI Document saved: {}", String::ToString(filePath));
            return true;
        }
        catch (const exception& exception)
        {
            LOG_ERROR("UI Document save exception: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    bool UI_DocumentSerializer::Load_FromJsonFile(UI_Document& document, const wstring& filePath)
    {
        if (filePath.empty())
        {
            LOG_ERROR("UI Document load failed: empty path");
            return false;
        }

        try
        {
            ifstream file{ fs::path(filePath) };
            if (!file.is_open())
            {
                LOG_ERROR("UI Document load failed: cannot open {}", String::ToString(filePath));
                return false;
            }

            json root = {};
            file >> root;

            if (!From_Json(document, root))
            {
                LOG_ERROR("UI Document load failed: invalid data {}", String::ToString(filePath));
                return false;
            }

            LOG_INFO("UI Document loaded: {}", String::ToString(filePath));
            return true;
        }
        catch (const exception& exception)
        {
            LOG_ERROR("UI Document load exception: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    json UI_DocumentSerializer::To_Json(const UI_Document& document)
    {
        json root = {};
        root["version"] = kCurrentUIJsonVersion;
        root["canvas"] = {
            { "id", 0 },
            { "name", document._canvasName.empty() ? kDefaultCanvasName : document._canvasName },
            { "width", document._canvasSize.x },
            { "height", document._canvasSize.y }
        };

        root["objects"] = json::array();

        unordered_set<ObjectId> savedObjectIds;
        for (const UI_Object& object : document._objects)
        {
            if (object.id == 0 || savedObjectIds.find(object.id) != savedObjectIds.end())
            {
                LOG_WARN("UI Document save skipped duplicated/invalid object id: {}", object.id);
                continue;
            }
            savedObjectIds.insert(object.id);

            json item = {};
            item["id"] = object.id;
            item["type"] = Get_ObjectTypeName(object.type);
            item["name"] = object.name.empty() ? Make_DefaultObjectName(object.type) : object.name;
            item["parentId"] = object.parentId;
            item["position"] = Vec2_ToJson(object.position);
            item["size"] = Vec2_ToJson(object.size);
            item["scale"] = Vec2_ToJson(object.scale);
            item["rotation"] = object.rotationRadians;
            item["sortOrder"] = object.sortOrder;
            item["color"] = Color_ToJson(object.color);

            if (object.type == UI_ObjectType::Image)
            {
                item["texturePath"] = object.texturePath;
                if (!object.textureGuid.empty())
                    item["textureGuid"] = object.textureGuid;
                if (!object.maskTexturePath.empty())
                    item["maskTexturePath"] = object.maskTexturePath;
                if (!object.maskTextureGuid.empty())
                    item["maskTextureGuid"] = object.maskTextureGuid;
            }
            else if (object.type == UI_ObjectType::Text)
            {
                item["text"] = object.text;
                item["fontPath"] = object.fontPath;
                item["fontSize"] = object.fontSize;
                item["textAlign"] = Get_TextAlignName(object.textAlign);
                item["textWrap"] = object.textWrap;
                if (!object.fontGuid.empty())
                    item["fontGuid"] = object.fontGuid;
                if (!object.fontTag.empty())
                    item["fontTag"] = object.fontTag;
            }

            root["objects"].push_back(item);
        }

        return root;
    }

    bool UI_DocumentSerializer::From_Json(UI_Document& document, const json& root)
    {
        if (!root.is_object())
        {
            LOG_ERROR("UI Document load failed: root is not an object");
            return false;
        }

        const int32 version = Read_Int(root, "version", kCurrentUIJsonVersion);
        if (version != kCurrentUIJsonVersion)
            LOG_WARN("UI Document json version {} is not explicitly supported. Trying version 1 compatible load.", version);

        string nextCanvasName = kDefaultCanvasName;
        ImVec2 nextCanvasSize = ImVec2(1600.f, 900.f);

        const auto canvasIter = root.find("canvas");
        if (canvasIter != root.end() && canvasIter->is_object())
        {
            nextCanvasName = Read_String(*canvasIter, "name", kDefaultCanvasName);
            nextCanvasSize.x = Read_Float(*canvasIter, "width", 1600.f);
            nextCanvasSize.y = Read_Float(*canvasIter, "height", 900.f);
            nextCanvasSize.x = max(1.f, nextCanvasSize.x);
            nextCanvasSize.y = max(1.f, nextCanvasSize.y);
        }
        else
        {
            LOG_WARN("UI Document load: canvas field missing. Using defaults.");
        }

        vector<UI_Object> nextObjects;
        unordered_set<ObjectId> loadedObjectIds;

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
        {
            LOG_WARN("UI Document load: objects array missing. Loading empty canvas.");
        }
        else
        {
            for (const json& item : *objectsIter)
            {
                if (!item.is_object())
                {
                    LOG_WARN("UI Document load skipped non-object item.");
                    continue;
                }

                UI_ObjectType objectType = UI_ObjectType::Image;
                const string typeName = Read_String(item, "type", "");
                if (!Try_ParseObjectType(typeName, objectType))
                {
                    LOG_WARN("UI Document load skipped unknown object type: {}", typeName);
                    continue;
                }

                const ObjectId objectId = Read_ObjectId(item, "id", 0);
                if (objectId == 0)
                {
                    LOG_WARN("UI Document load skipped object with missing/invalid id.");
                    continue;
                }

                if (objectId == numeric_limits<ObjectId>::max())
                {
                    LOG_WARN("UI Document load skipped object with overflow-risk id: {}", objectId);
                    continue;
                }

                if (loadedObjectIds.find(objectId) != loadedObjectIds.end())
                {
                    LOG_WARN("UI Document load skipped duplicated object id: {}", objectId);
                    continue;
                }

                UI_Object object = Make_DefaultObject(objectType);
                object.id = objectId;
                object.type = objectType;
                object.parentId = Read_ObjectId(item, "parentId", 0);
                object.name = Read_String(item, "name", Make_DefaultObjectName(objectType));
                object.position = Read_Vec2(item, "position", ImVec2(0.f, 0.f));
                object.size = Read_Vec2(item, "size", ImVec2(100.f, 100.f));
                object.scale = Read_Vec2(item, "scale", ImVec2(1.f, 1.f));
                object.rotationRadians = Read_Float(item, "rotation", 0.f);
                object.sortOrder = Read_Int(item, "sortOrder", 0);
                object.color = Read_Color(item, "color", ImVec4(1.f, 1.f, 1.f, 1.f));

                object.size.x = max(1.f, object.size.x);
                object.size.y = max(1.f, object.size.y);
                object.scale.x = max(0.01f, object.scale.x);
                object.scale.y = max(0.01f, object.scale.y);

                if (object.type == UI_ObjectType::Image)
                {
                    object.texturePath = Read_String(item, "texturePath", "");
                    object.textureGuid = Read_String(item, "textureGuid", "");
                    object.maskTexturePath = Read_String(item, "maskTexturePath", "");
                    object.maskTextureGuid = Read_String(item, "maskTextureGuid", "");
                }
                else if (object.type == UI_ObjectType::Text)
                {
                    object.text = Read_String(item, "text", "Text");
                    object.fontPath = Read_String(item, "fontPath", kDefaultFontPath);
                    object.fontSize = max(4.f, Read_Float(item, "fontSize", 32.f));
                    UI_TextAlign textAlign = UI_TextAlign::Left;
                    string textAlignName = Read_String(item, "textAlign", "");
                    if (textAlignName.empty())
                        textAlignName = Read_String(item, "align", "");
                    if (Try_ParseTextAlign(textAlignName, textAlign))
                        object.textAlign = textAlign;
                    object.textWrap = Read_Bool(item, "textWrap", Read_Bool(item, "wrap", false));
                    object.fontGuid = Read_String(item, "fontGuid", "");
                    object.fontTag = Read_String(item, "fontTag", Make_FontTagFromPath(object.fontPath));
                    if (object.fontTag.empty())
                        object.fontTag = Make_FontTagFromPath(object.fontPath);
                }

                loadedObjectIds.insert(object.id);
                nextObjects.push_back(object);
            }
        }

        document._canvasName = nextCanvasName.empty() ? kDefaultCanvasName : nextCanvasName;
        document._canvasSize = nextCanvasSize;
        document._objects = move(nextObjects);
        document.Validate_ParentLinks();
        document.Resolve_NextObjectId();

        return true;
    }
}
