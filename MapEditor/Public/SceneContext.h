#pragma once
#include "EditorObject.h"

NS_BEGIN(MapEditor)

struct SceneContext
{
    vector<Shared<EditorObject>> objects;
    Shared<EditorObject> selected;

    void Add(Shared<EditorObject> obj)
    {
        if (obj)
        {
            objects.push_back(move(obj));
        }
    }

    void Remove(const Shared<EditorObject>& obj)
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

    Shared<EditorObject> Duplicate(const Shared<EditorObject>& src)
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
};

NS_END
