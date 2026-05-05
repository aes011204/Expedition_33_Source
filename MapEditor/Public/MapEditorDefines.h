#pragma once
#include "Engine_Define.h"
#include "Helper_String.h"

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

    inline const char* _mapSavePath = "../../Client/Bin/Resources/Map/SaveJson/";
    inline const char* _mapOriginPath = "../../Client/Bin/Resources/Map/OriginJson/";

    enum class EObjectType
    {
        StaticMesh,
        SpawnPoint,
        EncounterZone,
        TriggerVolume,
        CameraZone,
        PostProcessVolume,
        Light,
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
};

using namespace MapEditor;
