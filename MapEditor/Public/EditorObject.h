#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

struct EditorObject
{
    wstring _name = L"Object";
    wstring _modelPath = {};
    Vec3 _position = Vec3::Zero;
    Quat _rotation = Quat::Identity;
    Vec3 _scale = Vec3::One;
    Matrix _worldMatrix = Matrix::Identity;
    EObjectType _type = EObjectType::StaticMesh;

    EZoneShape _zoneShape = EZoneShape::Box;
    Vec3 _zoneExtent = { 5.f, 5.f, 5.f };
    float _zoneRadius = 5.f;

    string _zoneId = {};

    float _cameraFov = 60.f;
    bool _cameraFixedAngle = false;
    float _cameraTransitionTime = 0.5f;

    float _ppBloomIntensity = 1.f;
    float _ppExposureMin = 0.f;
    float _ppExposureMax = 2.f;
    float _ppFogDensity = {};
    Vec3 _ppFogColor = Vec3::One;

    void UpdateWorldMatrix()
    {
        _worldMatrix = Matrix::CreateScale(_scale) * Matrix::CreateFromQuaternion(_rotation) * Matrix::CreateTranslation(_position);
    }

    static Shared<EditorObject> Create(const wstring& name = L"Object", EObjectType type = EObjectType::StaticMesh)
    {
        auto obj = make_shared<EditorObject>();
        obj->_name = name;
        obj->_type = type;
        obj->UpdateWorldMatrix();
        return obj;
    }
};

NS_END
