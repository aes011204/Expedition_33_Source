#pragma once
#include "MapEditorDefines.h"
#include "SceneContext.h"

NS_BEGIN(MapEditor)

class MECamera;

class MELandscapeManager
{
public:
    struct LandMG_Desc
    {
        ComPtr<Device> device;
        ComPtr<Context> context;
        SceneContext* scene = {};
        MECamera* camera = {};
        Shared<LandscapeSculptState> sculptState;
        wstring resourceRoot;
    };

public:
    MELandscapeManager() = default;
    ~MELandscapeManager() = default;

public:
    HRESULT Initialize(const LandMG_Desc& desc);

private:
    ComPtr<Device> _device;
    ComPtr<Context> _context;
    SceneContext* _scene = nullptr;
    MECamera* _camera = nullptr;
    Shared<LandscapeSculptState> _sculptState;
    wstring _resourceRoot;

public:
    void Add_Landscape(const LandscapeConfig& cfg);
    void Remove_Landscape(int32 idx);
    void Modify_Landscape(int32 idx, const LandscapeConfig& cfg);

    void Save_Landscape(int32 idx, const WCHAR* path);
    void Load_Landscape(int32 idx, const WCHAR* path);

    void Apply_Noise(int32 idx, const LandscapeNoiseParams& params);
    void Apply_Erosion(int32 idx, const LandscapeErosionParams& params);

    void Add_LandscapeLayer(int32 idx, const WCHAR* path);

    void Do_SculptAt(ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSize, float timeDelta);

public:
    static Unique<MELandscapeManager> Create(const LandMG_Desc& desc);
};

NS_END
