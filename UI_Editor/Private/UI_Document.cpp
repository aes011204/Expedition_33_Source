#include "pch.h"
#include "UI_Document.h"

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
                object.fontPath = kDefaultFontPath;
                object.fontTag = Make_FontTagFromPath(object.fontPath);
            }

            return object;
        }
    }

    UI_Document::UI_Document()
    {
        Reset();
    }

    void UI_Document::Reset()
    {
        _canvasName = kDefaultCanvasName;
        _canvasSize = ImVec2(1600.f, 900.f);
        _nextObjectId = 1;

        _objects.clear();
    }

    const string& UI_Document::Get_CanvasName() const
    {
        return _canvasName;
    }

    const ImVec2& UI_Document::Get_CanvasSize() const
    {
        return _canvasSize;
    }

    const vector<UI_Object>& UI_Document::Get_Objects() const
    {
        return _objects;
    }

    vector<UI_Object>& UI_Document::Get_MutableObjects()
    {
        return _objects;
    }

    ObjectId UI_Document::Create_Object(UI_ObjectType objectType, ObjectId parentId)
    {
        if (parentId != 0 && nullptr == Find_Object(parentId))
            parentId = 0;

        UI_Object object = {};
        object.id = _nextObjectId++;
        object.parentId = parentId;
        object.type = objectType;
        object.position = ImVec2(100.f, 100.f);
        object.scale = ImVec2(1.f, 1.f);
        object.rotationRadians = 0.f;
        object.sortOrder = static_cast<int32>(_objects.size());

        switch (objectType)
        {
        case UI_ObjectType::Image:
            object.name = "Image";
            object.size = ImVec2(100.f, 100.f);
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);
            break;

        case UI_ObjectType::Text:
            object.name = "Text";
            object.size = ImVec2(300.f, 50.f);
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);
            object.text = "New Text";
            object.fontSize = 32.f;
            break;

        case UI_ObjectType::Panel:
            object.name = "Panel";
            object.size = ImVec2(400.f, 300.f);
            object.color = ImVec4(0.35f, 0.35f, 0.35f, 0.6f);
            break;

        case UI_ObjectType::Button:
            object.name = "Button";
            object.size = ImVec2(200.f, 60.f);
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);
            break;

        default:
            object.name = "Object";
            object.size = ImVec2(100.f, 100.f);
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);
            break;
        }

        _objects.push_back(object);
        return object.id;
    }

    ObjectId UI_Document::Duplicate_Object(ObjectId objectId, const ImVec2& offset)
    {
        const UI_Object* sourceObject = Find_Object(objectId);
        if (nullptr == sourceObject)
            return 0;

        return Duplicate_ObjectRecursive(*sourceObject, sourceObject->parentId, offset);
    }

    bool UI_Document::Delete_Object(ObjectId objectId)
    {
        if (objectId == 0 || nullptr == Find_Object(objectId))
            return false;

        vector<ObjectId> deleteObjectIds;
        Collect_ObjectAndChildren(objectId, deleteObjectIds);

        _objects.erase(
            remove_if(
                _objects.begin(),
                _objects.end(),
                [&deleteObjectIds](const UI_Object& object)
                {
                    return find(deleteObjectIds.begin(), deleteObjectIds.end(), object.id) != deleteObjectIds.end();
                }),
            _objects.end());

        return true;
    }

    UI_Object* UI_Document::Find_Object(ObjectId objectId)
    {
        for (UI_Object& object : _objects)
        {
            if (object.id == objectId)
                return &object;
        }

        return nullptr;
    }

    const UI_Object* UI_Document::Find_Object(ObjectId objectId) const
    {
        for (const UI_Object& object : _objects)
        {
            if (object.id == objectId)
                return &object;
        }

        return nullptr;
    }

    bool UI_Document::Save_ToJsonFile(const wstring& filePath) const
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

            file << To_Json().dump(4);
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

    bool UI_Document::Load_FromJsonFile(const wstring& filePath)
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

            if (!From_Json(root))
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

    json UI_Document::To_Json() const
    {
        json root = {};
        root["version"] = kCurrentUIJsonVersion;
        root["canvas"] = {
            { "id", 0 },
            { "name", _canvasName.empty() ? kDefaultCanvasName : _canvasName },
            { "width", _canvasSize.x },
            { "height", _canvasSize.y }
        };

        root["objects"] = json::array();

        unordered_set<ObjectId> savedObjectIds;
        for (const UI_Object& object : _objects)
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
            }
            else if (object.type == UI_ObjectType::Text)
            {
                item["text"] = object.text;
                item["fontPath"] = object.fontPath;
                item["fontSize"] = object.fontSize;
                if (!object.fontGuid.empty())
                    item["fontGuid"] = object.fontGuid;
                if (!object.fontTag.empty())
                    item["fontTag"] = object.fontTag;
            }

            root["objects"].push_back(item);
        }

        return root;
    }

    bool UI_Document::From_Json(const json& root)
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
        ObjectId maxObjectId = 0;

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
                }
                else if (object.type == UI_ObjectType::Text)
                {
                    object.text = Read_String(item, "text", "Text");
                    object.fontPath = Read_String(item, "fontPath", kDefaultFontPath);
                    object.fontSize = max(4.f, Read_Float(item, "fontSize", 32.f));
                    object.fontGuid = Read_String(item, "fontGuid", "");
                    object.fontTag = Read_String(item, "fontTag", Make_FontTagFromPath(object.fontPath));
                    if (object.fontTag.empty())
                        object.fontTag = Make_FontTagFromPath(object.fontPath);
                }

                loadedObjectIds.insert(object.id);
                maxObjectId = max(maxObjectId, object.id);
                nextObjects.push_back(object);
            }
        }

        for (UI_Object& object : nextObjects)
        {
            if (object.parentId != 0 && loadedObjectIds.find(object.parentId) == loadedObjectIds.end())
            {
                LOG_WARN("UI Document load: object {} parent {} missing. Reparenting to Canvas.", object.id, object.parentId);
                object.parentId = 0;
            }
        }

        _canvasName = nextCanvasName.empty() ? kDefaultCanvasName : nextCanvasName;
        _canvasSize = nextCanvasSize;
        _objects = move(nextObjects);
        _nextObjectId = maxObjectId + 1;
        if (_nextObjectId == 0)
            _nextObjectId = 1;

        return true;
    }

    void UI_Document::Set_CanvasSize(const ImVec2& canvasSize)
    {
        _canvasSize.x = max(1.f, canvasSize.x);
        _canvasSize.y = max(1.f, canvasSize.y);
    }

    void UI_Document::Set_CanvasName(const string& canvasName)
    {
        _canvasName = canvasName.empty() ? kDefaultCanvasName : canvasName;
    }

    void UI_Document::Resolve_NextObjectId()
    {
        ObjectId maxObjectId = 0;
        for (const UI_Object& object : _objects)
            maxObjectId = max(maxObjectId, object.id);

        _nextObjectId = maxObjectId + 1;
        if (_nextObjectId == 0)
            _nextObjectId = 1;
    }

    void UI_Document::Collect_ObjectAndChildren(ObjectId objectId, vector<ObjectId>& objectIds) const
    {
        objectIds.push_back(objectId);

        for (const UI_Object& object : _objects)
        {
            if (object.parentId == objectId)
                Collect_ObjectAndChildren(object.id, objectIds);
        }
    }

    ObjectId UI_Document::Duplicate_ObjectRecursive(UI_Object sourceObject, ObjectId parentId, const ImVec2& offset)
    {
        const ObjectId sourceObjectId = sourceObject.id;

        UI_Object duplicateObject = sourceObject;
        duplicateObject.id = _nextObjectId++;
        duplicateObject.parentId = parentId;
        duplicateObject.name = sourceObject.name + "_Copy";
        duplicateObject.position.x += offset.x;
        duplicateObject.position.y += offset.y;
        duplicateObject.sortOrder = static_cast<int32>(_objects.size());

        _objects.push_back(duplicateObject);

        for (size_t i = 0; i < _objects.size(); ++i)
        {
            if (_objects[i].parentId == sourceObjectId)
            {
                UI_Object childSourceObject = _objects[i];
                Duplicate_ObjectRecursive(childSourceObject, duplicateObject.id, offset);
            }
        }

        return duplicateObject.id;
    }
}
