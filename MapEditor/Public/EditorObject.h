#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

class MEStaticMeshActor;
class MEInstancedMeshActor;

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

    LightType _lightType = LightType::Directional;
    Vec4 _lightDiffuse = { 1.f, 1.f, 1.f, 1.f };
    Vec4 _lightAmbient = { 0.1f, 0.1f, 0.1f, 1.f };
    Vec4 _lightSpecular = { 1.f, 1.f, 1.f, 1.f };
    Vec4 _lightDirection = { 0.f, -1.f, 0.f, 0.f };
    float _lightRange = 10.f;

    Shared<MEStaticMeshActor> _renderHandle;
    bool _renderHandleLoaded = false;
    bool _isInstanced = false;

    wstring _instancedModelPath = {};
    vector<InstanceTransform> _instancedTransforms;

    Shared<MEInstancedMeshActor> _instancedRenderHandle;
    bool _instancedParamsDirty = true;

    bool _isDefaultLight = false;

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
