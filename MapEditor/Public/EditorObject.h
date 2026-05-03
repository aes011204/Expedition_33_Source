#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

struct EditorObject
{
    wstring name = L"Object";
    wstring modelPath = {};
    Vec3 position = Vec3::Zero;
    Quat rotation = Quat::Identity;
    Vec3 scale = Vec3::One;
    Matrix worldMatrix = Matrix::Identity;

    void UpdateWorldMatrix()
    {
        worldMatrix = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
    }

    static Shared<EditorObject> Create(const wstring& name = L"Object")
    {
        auto obj = make_shared<EditorObject>();
        obj->name = name;
        obj->UpdateWorldMatrix();
        return obj;
    }
};

NS_END
