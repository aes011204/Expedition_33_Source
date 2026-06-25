#pragma once
#include "MapEditorDefines.h"
#include "VIBuffer_Landscape.h"

NS_BEGIN(MapEditor)

class MEStaticMeshActor;
class MEInstancedMeshActor;
class MEWaterActor;

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
    float _ppFogBaseHeight = 0.f;
    float _ppFogTopFadeStart = 0.4f;
    float _ppFogTopHeight = 0.9f;
    float _ppFogEdgeFade = 1.f;
    float _ppFogNoiseScale = 0.0025f;
    Vec2 _ppFogNoiseSpeed = { 0.005f, 0.003f };
    float _ppFogNoiseStrength = 0.9f;
    float _ppFogNoiseMin = 0.38f;
    float _ppGroundFogMaxDistance = 100.f;
    float _ppGroundFogMinAccumulationDistance = 10.f;
    float _ppGroundFogDistanceInfluence = 0.15f;
    float _ppGroundFogNearFadeDistance = 12.f;
    float _ppGroundFogNearMinFactor = 0.65f;

    float _shaftDensity = 1.f;
    Vec4 _shaftLightColor = Vec4(1.f, 0.95f, 0.8f, 1.f);
    float _shaftEdgeFade = 0.5f;
    float _shaftIntensity = 1.f;

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

    //Shared<MEInstancedMeshActor> _instancedRenderHandle;
    vector<Shared<MEInstancedMeshActor>> _instancedChunkHandles;
    vector<BoundingBox> _instancedChunkAABBs;
    vector<vector<uint32>> _instancedChunkGlobalIndices;

    bool _instancedParamsDirty = true;
    bool _instancedHasCollision = false;
    int32 _selectedInstanceIdx = -1;

    bool _visible = true;
    bool _cullEnabled = true;
    bool _castShadow = true;

    float _scatterRadius = 5.f;
    int32 _scatterCount = 5;
    float _scatterScaleMin = 0.8f;
    float _scatterScaleMax = 1.2f;
    float _scatterYRotMax = 180.f;

    VIBuffer_Landscape::LAND_DESC _waterDesc = {};
    WaterCB _waterParams = {};

    wstring _waterLargeWaveNormalPath = {};
    wstring _waterSmallWaveNormalPath = {};
    wstring _waterSeafoamPath = {};
    wstring _waterCubemapPath = {};

    Shared<MEWaterActor> _waterRenderHandle;
    bool _waterRenderHandleLoaded = false;

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
