#pragma once

#include "UI_Document.h"
#include "UI_EditorReadModel.h"

namespace UIEditor
{
    class UI_BrushPresetService final
    {
    public:
        bool Can_CreatePatternOverlayFromImage(const UI_Document& document, const UI_EditorReadModel& readModel, ObjectId sourceObjectId) const;
        ObjectId Create_PatternOverlayFromImage(UI_Document& document, const UI_EditorReadModel& readModel, ObjectId sourceObjectId) const;

        bool Can_CreateHoverBrushPreset(const UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId) const;
        bool Create_HoverBrushPreset(UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId, bool allowStateAnimationOverwrite) const;

        bool Can_CreateBrushAtmospherePreset(const UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId) const;
        bool Create_BrushAtmospherePreset(UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId) const;
    };
}
