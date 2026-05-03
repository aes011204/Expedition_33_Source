#include "SceneContext.h"
#include "Helper_String.h"

void SceneContext::SaveToFile(const string& filePath)
{
    json root;
    json& map = root["MapData"];

    map["MapName"] = String::ToString(mapName);

    json meshArray = json::array();
    for (auto& obj : objects)
    {
        json entry;
        entry["Id"] = String::ToString(obj->name);
        entry["ModelPath"] = String::ToString(obj->modelPath);
        entry["Position"] = { obj->position.x, obj->position.y, obj->position.z };
        entry["Rotation"] = { obj->rotation.x, obj->rotation.y, obj->rotation.z, obj->rotation.w };
        entry["Scale"] = { obj->scale.x, obj->scale.y, obj->scale.z };
        meshArray.push_back(entry);
    }

    map["StaticMeshes"] = meshArray;

    ofstream ofs(filePath);
    ofs << root.dump(4);
}

void SceneContext::LoadFromFile(const string& filePath)
{
    ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        return;
    }

    json root = json::parse(ifs);
    const json& map = root["MapData"];

    mapName = String::ToWString(map["MapName"].get<string>());

    objects.clear();
    selected = nullptr;

    for (auto& entry : map["StaticMeshes"])
    {
        auto obj = make_shared<EditorObject>();
        obj->name = String::ToWString(entry["Id"].get<string>());
        obj->modelPath = String::ToWString(entry["ModelPath"].get<string>());

        auto& pos = entry["Position"];
        obj->position = { pos[0], pos[1], pos[2] };

        auto& rot = entry["Rotation"];
        obj->rotation = { rot[0], rot[1], rot[2] };

        auto& scl = entry["Scale"];
        obj->scale = { scl[0], scl[1], scl[2] };

        obj->UpdateWorldMatrix();
        objects.push_back(obj);
    }
}

void SceneContext::Add(Shared<EditorObject> obj)
{
    if (obj)
    {
        objects.push_back(move(obj));
    }
}

void SceneContext::Remove(const Shared<EditorObject>& obj)
{
    if (!obj)
    {
        return;
    }
    auto it = find(objects.begin(), objects.end(), obj);
    if (it == objects.end())
    {
        return;
    }

    if (obj == selected)
    {
        selected = nullptr;
    }
    objects.erase(it);
}

Shared<EditorObject> SceneContext::Duplicate(const Shared<EditorObject>& src)
{
    if (!src)
    {
        return nullptr;
    }
    auto copy = make_shared<EditorObject>(*src);
    copy->name = src->name + L"_Copy";
    objects.push_back(copy);
    return copy;
}
