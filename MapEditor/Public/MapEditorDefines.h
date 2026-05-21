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
    inline const char* _camDataPath = "../Bin/Resources/Json/CamData/";

    enum class EObjectType
    {
        StaticMesh,
        SpawnPoint,
        EncounterZone,
        TriggerVolume,
        CameraZone,
        PostProcessVolume,
        Light,
        InstancedMesh,
        Fail
    };

    enum class EZoneShape
    {
        Box,
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
        case MapEditor::EObjectType::Light:
            return "[LT]";
        case MapEditor::EObjectType::InstancedMesh:
            return "[IM]";
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

    struct SnapSettings
    {
        float translate = 0.5f;
        float rotate = 15.f;
        float scale = 0.1f;
    };

    struct ModelOverrideEntry
    {
        ShaderType shaderType = ShaderType::VtxMesh;
        int32 passIndex = -1;
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
    };
    inline constexpr int32 g_staticMeshPassOptionCount = 8;

    inline constexpr pair<const char*, uint32> g_instanceMeshPassOptions[] = {
    { "Default", VtxInstanceMeshExactPassIndex::DefaultPass },
    { "Shadow", VtxInstanceMeshExactPassIndex::ShadowPass },
    { "NoTexture", VtxInstanceMeshExactPassIndex::NoTexturePass },
    { "Masked", VtxInstanceMeshExactPassIndex::MaskedPass },
    { "TwoSided", VtxInstanceMeshExactPassIndex::TwoSidedPass },
    { "MaskedTwoSided", VtxInstanceMeshExactPassIndex::MaskedTwoSidedPass },
    { "OpaqueAR", VtxInstanceMeshExactPassIndex::OpaqueARPass },
    { "OpaqueARTwoSided", VtxInstanceMeshExactPassIndex::OpaqueARTwoSidedPass },
    };
    inline constexpr int32 g_instanceMeshPassOptionCount = 8;

    struct InstanceTransform
    {
        Vec3 _position;
        Vec4 _rotation;
        Vec3 _scale;
    };

    struct CameraBookmark
    {
        Vec3 position = {};
        float yaw = {};
        float pitch = {};
        bool valid = false;
    };
    inline constexpr int CameraBookmarkCount = 5;
};

using namespace MapEditor;
