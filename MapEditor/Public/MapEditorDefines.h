#pragma once
#include "Engine_Define.h"
#include "Helper_String.h"
#include "ShaderPassFinder.h"

#ifdef new
#pragma push_macro("new")
#undef new
#define MAPEDITOR_RESTORE_NEW
#endif

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "implot.h"
#include "ImGuizmo.h"

#ifdef MAPEDITOR_RESTORE_NEW
#pragma pop_macro("new")
#undef MAPEDITOR_RESTORE_NEW
#endif

namespace MapEditor
{
    inline constexpr uint32 g_iWinSizeX = 1600u;
    inline constexpr uint32 g_iWinSizeY = 900u;

    inline const char* _mapSavePath = "../../../Client/Bin/Resources/Map/";
    inline const char* _materialsPath = "../../../Client/Bin/Resources/Materials/";
    inline const char* _camDataPath = "../Bin/Resources/Json/CamData/";
    inline const WCHAR* _waterPath = L"../../../Client/Bin/Resources/Map/Water/";

    enum class EObjectType
    {
        StaticMesh,
        SpawnPoint,
        EncounterZone,
        TriggerVolume,
        CameraZone,
        PostProcessVolume,
        ShaftVolume,
        Light,
        InstancedMesh,
        Terrain,
        Landscape,
        Water,
        Fail
    };

    enum class EZoneShape
    {
        Box,
        OBB,
        Sphere
    };

    inline const char* ObjectTypeLabel(EObjectType t)
    {
        switch (t)
        {
        case MapEditor::EObjectType::StaticMesh:
            return "[SM]";
        case MapEditor::EObjectType::SpawnPoint:
            return "[SP]";
        case MapEditor::EObjectType::EncounterZone:
            return "[EZ]";
        case MapEditor::EObjectType::TriggerVolume:
            return "[TV]";
        case MapEditor::EObjectType::CameraZone:
            return "[CZ]";
        case MapEditor::EObjectType::PostProcessVolume:
            return "[PP]";
        case MapEditor::EObjectType::ShaftVolume:
            return "[SV]";
        case MapEditor::EObjectType::Light:
            return "[LT]";
        case MapEditor::EObjectType::InstancedMesh:
            return "[IM]";
        case MapEditor::EObjectType::Terrain:
            return "[TR]";
        case MapEditor::EObjectType::Landscape:
            return "[LS]";
        case MapEditor::EObjectType::Water:
            return "[WT]";
        case MapEditor::EObjectType::Fail:
        default:
            return "[??]";
        }
    }

    inline const char* ObjectTypeName(EObjectType t)
    {
        return magic_enum::enum_name(t).data();
    }

    inline EObjectType ObjectTypeFromName(const string& name)
    {
        optional<EObjectType> obj = magic_enum::enum_cast<EObjectType>(name);
        if (obj.has_value())
        {
            return obj.value();
        }
        return EObjectType::Fail;
    }

    enum class EMapMaterialObjectKind
    {
        StaticMesh,
        InstancedMesh,
        Terrain,
        Landscape,
        Water,
        Unsupported
    };

    inline const char* MapMaterialObjectKindName(EMapMaterialObjectKind kind)
    {
        return magic_enum::enum_name(kind).data();
    }

    inline EMapMaterialObjectKind MapMaterialObjectKindFromName(const string& name)
    {
        optional<EMapMaterialObjectKind> kind = magic_enum::enum_cast<EMapMaterialObjectKind>(name);
        if (kind.has_value())
        {
            return kind.value();
        }
        return EMapMaterialObjectKind::Unsupported;
    }

    inline EMapMaterialObjectKind MapMaterialObjectKindFromObjectType(EObjectType type)
    {
        switch (type)
        {
        case EObjectType::StaticMesh:
            return EMapMaterialObjectKind::StaticMesh;
        case EObjectType::InstancedMesh:
            return EMapMaterialObjectKind::InstancedMesh;
        case EObjectType::Terrain:
            return EMapMaterialObjectKind::Terrain;
        case EObjectType::Landscape:
            return EMapMaterialObjectKind::Landscape;
        case EObjectType::Water:
            return EMapMaterialObjectKind::Water;
        default:
            return EMapMaterialObjectKind::Unsupported;
        }
    }

    struct SnapSettings
    {
        float translate = 0.5f;
        float rotate = 15.f;
        float scale = 0.1f;
    };

    struct ModelOverrideEntry
    {
        ShaderType shaderType = ShaderType::VtxMesh;
        map<int32, string> textureSlots;
        bool hasMaterialCB = false;
        Engine::MaterialCB materialCB{};

        bool hasMaterialBindings = false;
        EMapMaterialObjectKind materialObjectKind = EMapMaterialObjectKind::Unsupported;
        string baseBundleKey = {};
        json materialBindingsJson = json::array();
        json materialMetaJson = json::object();

        bool HasLegacyOverride() const;
        bool HasMaterialBindingsOverride() const;
        bool HasAnyMaterialOverride() const;
        void LoadLegacyMaterialJson(const json& root);
        void LoadMaterialBindingsJson(const json& root);
        json BuildMaterialJson(const string& modelPath, const string& mapName) const;
        void ExportToNativeMaterial(const string& modelPath, const string& mapName) const;
    };

    inline constexpr pair<const char*, uint32> g_staticMeshPassOptions[] = {
    { "Default", VtxMeshPassIndex::DefaultPass },
    { "Shadow", VtxMeshPassIndex::ShadowPass },
    { "NoTexture", VtxMeshPassIndex::NoTexturePass },
    { "Masked", VtxMeshPassIndex::MaskedPass },
    { "TwoSided", VtxMeshPassIndex::TwoSidedPass },
    { "MaskedTwoSided", VtxMeshPassIndex::MaskedTwoSidedPass },
    { "OpaqueAR", VtxMeshPassIndex::OpaqueARPass },
    { "OpaqueARTwoSided", VtxMeshPassIndex::OpaqueARTwoSidedPass },
    { "LandscapeBlend", VtxMeshPassIndex::LandscapeBlendPass },
    { "SignDecal", VtxMeshPassIndex::SignDecalPass },
    { "EclipseSphere", VtxMeshPassIndex::EclipseSpherePass },
    };
    inline constexpr int32 g_staticMeshPassOptionCount = 11;

    inline constexpr pair<const char*, uint32> g_instanceMeshPassOptions[] = {
    { "Default", VtxInstanceMeshExactPassIndex::DefaultPass },
    { "Shadow", VtxInstanceMeshExactPassIndex::ShadowPass },
    { "NoTexture", VtxInstanceMeshExactPassIndex::NoTexturePass },
    { "Masked", VtxInstanceMeshExactPassIndex::MaskedPass },
    { "TwoSided", VtxInstanceMeshExactPassIndex::TwoSidedPass },
    { "MaskedTwoSided", VtxInstanceMeshExactPassIndex::MaskedTwoSidedPass },
    { "OpaqueAR", VtxInstanceMeshExactPassIndex::OpaqueARPass },
    { "OpaqueARTwoSided", VtxInstanceMeshExactPassIndex::OpaqueARTwoSidedPass },
    { "EmissiveGlow", VtxInstanceMeshExactPassIndex::EmissiveGlowPass },
    { "UnlitFlame", VtxInstanceMeshExactPassIndex::UnlitFlamePass },
    };
    inline constexpr int32 g_instanceMeshPassOptionCount = 10;

    struct InstanceTransform
    {
        Vec3 _position;
        Vec4 _rotation;
        Vec3 _scale;
    };

    struct ScatterZone
    {
        Vec3 center = Vec3::Zero;
        float radius = 10.f;
        int count = 20;
        int seed = 0;
        float scaleMin = 0.8f;
        float scaleMax = 1.2f;
        float yRotMax = 180.f;
        float minSpacing = 1.f;
    };

    struct ScatterBoxZone
    {
        Vec3 center = Vec3::Zero;
        // Y는 보류 — 인스턴스는 center.y 높이에 배치
        float halfExtentX = 5.f;
        float halfExtentZ = 5.f;
        int count = 20;
        int seed = 0;
        float scaleMin = 0.8f;
        float scaleMax = 1.2f;
        float yRotMax = 180.f;
        float minSpacing = 1.f;
    };

    struct CameraBookmark
    {
        Vec3 position = {};
        float yaw = {};
        float pitch = {};
        bool valid = false;
    };
    inline constexpr int CameraBookmarkCount = 5;

    struct TerrainTile
    {
        string file;
        int32 gridX = 0;
        int32 gridY = 0;
    };

    struct TerrainConfig
    {
        string heightmapFolder;
        float worldOffsetX = 0.f;
        float worldOffsetY = 0.f;
        float worldOffsetZ = 0.f;
        float scaleXZ = 1.f;
        float scaleY = 1.f;
        vector<TerrainTile> tiles;
        bool IsValid() const { return !tiles.empty(); }
    };

    enum class ESculptMode
    {
        None,
        Raise,
        Lower,
        Smooth,
        Flatten,
        Paint
    };

    struct LandscapeSculptState
    {
        ESculptMode mode = ESculptMode::None;
        float brushRadius = 5.f;
        float brushStrength = 0.5f;
        float flattenHeight = 0.f;
        uint32 paintLayerIdx = 0;
    };

    struct LandscapeConfig
    {
        string name = "Landscape_0";
        uint32 countX = 64;
        uint32 countZ = 64;
        float sizeX = 1.f;
        float sizeZ = 1.f;
        float worldOffsetX = 0.f;
        float worldOffsetY = 0.f;
        float worldOffsetZ = 0.f;
        uint32 chunkSize = 16;
        vector<float> heights;
    };

    struct LandscapeNoiseParams
    {
        float scale = 0.05f;
        float amplitude = 10.f;
        int32 octaves = 4;
        float persistence = 0.5f;
        int32 seed = 42;
        bool additive = true;
    };

    struct LandscapeErosionParams
    {
        int32 iterations = 50;
        float talusAngle = 0.5f;
        float erosionRate = 0.5f;
    };

    struct LandscapePanelContext
    {
        Shared<LandscapeSculptState> sculptState;
        function<void(const LandscapeConfig&)> onAdd;
        function<void(int32)> onRemove;
        function<void(int32, const WCHAR*)> onSave;
        function<void(int32, const WCHAR*)> onLoad;
        function<void(int32, const LandscapeNoiseParams&)> onApplyNoise;
        function<void(int32, const LandscapeErosionParams&)> onApplyErosion;
        function<void(int32, const WCHAR*, const WCHAR*, const WCHAR*)> onAddLayer;
        function<void(int32, uint32)> onRemoveLayer;
        function<void(int32, uint32)> onSetBaseLayer;
        function<void(int32, const LandscapeConfig&)> onModify;
    };
};

using namespace MapEditor;
