#include "pch.h"
#include "UI_BrushPresetService.h"

#include "UI_DocumentInternalAccess.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        constexpr const char* kHoverBrushFlowName = "Hover_PaintSlashFlow";
        constexpr const char* kHoverBrushFlowTexturePath = "../Bin/Resources/Textures/UI/Common/Paint/T_Generic_PaintSlash_Text_Variation3.png";
        constexpr const char* kBrushAtmosphereSlashShadowName = "BrushAtmosphere_SlashShadow";
        constexpr const char* kBrushAtmosphereInkWashName = "BrushAtmosphere_InkWash";
        constexpr const char* kBrushAtmosphereInkFlowName = "BrushAtmosphere_InkFlow";
        constexpr const char* kBrushAtmosphereParticlesName = "BrushAtmosphere_ShinyParticles";
        constexpr const char* kBrushAtmosphereSlashTexturePath = "../Bin/Resources/Textures/UI/Battle/EndBattle/T_HUD_SlashGrunge.png";
        constexpr const char* kBrushAtmosphereInkWashTexturePath = "../Bin/Resources/Textures/UI/UI_VFX/T_PaintBrush_01_White.png";
        constexpr const char* kBrushAtmosphereInkFlowTexturePath = "../Bin/Resources/Textures/UI/Common/Paint/T_Generic_PaintSlash_Text_Variation3.png";
        constexpr const char* kBrushAtmosphereParticlesTexturePath = "../Bin/Resources/Textures/UI/Common/Unclassified/DoNotShip/T_Particules_ShinyNumber_PlaceHolder.png";

        struct BrushAtmosphereLayerSpec
        {
            const char* name = "";
            const char* texturePath = "";
            ImVec2 sizeScale = ImVec2(1.f, 1.f);
            ImVec2 sizePadding = ImVec2(0.f, 0.f);
            ImVec2 offsetScale = ImVec2(0.f, 0.f);
            float rotationOffsetRadians = 0.f;
            ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
            bool materialVfx = false;
            int32 sortOffset = 0;
        };

        bool Starts_With(const string& value, const string& prefix)
        {
            return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
        }

        ObjectId Find_GeneratedChildByName(const UI_Document& document, ObjectId parentId, const string& baseName)
        {
            const string generatedPrefix = baseName + "_";
            for (const UI_Object& object : document.Get_Objects())
            {
                if (object.parentId != parentId)
                    continue;

                if (object.name == baseName || Starts_With(object.name, generatedPrefix))
                    return object.id;
            }

            return 0;
        }

        string Make_BrushAtmosphereLayerBaseName(const UI_Object& targetObject, const char* layerName)
        {
            if (targetObject.name.empty())
                return string(layerName);

            return targetObject.name + "_" + layerName;
        }

        const BrushAtmosphereLayerSpec* Get_BrushAtmosphereLayerSpecs(size_t& outCount)
        {
            static const BrushAtmosphereLayerSpec layerSpecs[] =
            {
                {
                    kBrushAtmosphereSlashShadowName,
                    kBrushAtmosphereSlashTexturePath,
                    ImVec2(1.55f, 1.05f),
                    ImVec2(140.f, 36.f),
                    ImVec2(-0.03f, 0.02f),
                    -0.035f,
                    ImVec4(0.014f, 0.011f, 0.009f, 0.68f),
                    false,
                    -4,
                },
                {
                    kBrushAtmosphereInkWashName,
                    kBrushAtmosphereInkWashTexturePath,
                    ImVec2(1.28f, 1.78f),
                    ImVec2(96.f, 64.f),
                    ImVec2(0.08f, -0.05f),
                    0.16f,
                    ImVec4(0.026f, 0.020f, 0.016f, 0.28f),
                    false,
                    -3,
                },
                {
                    kBrushAtmosphereInkFlowName,
                    kBrushAtmosphereInkFlowTexturePath,
                    ImVec2(1.34f, 1.38f),
                    ImVec2(104.f, 38.f),
                    ImVec2(-0.06f, 0.00f),
                    0.02f,
                    ImVec4(1.f, 1.f, 1.f, 1.f),
                    true,
                    -2,
                },
                {
                    kBrushAtmosphereParticlesName,
                    kBrushAtmosphereParticlesTexturePath,
                    ImVec2(1.20f, 1.22f),
                    ImVec2(78.f, 58.f),
                    ImVec2(0.08f, -0.10f),
                    -0.10f,
                    ImVec4(1.f, 0.70f, 0.32f, 0.46f),
                    false,
                    1,
                },
            };

            outCount = size(layerSpecs);
            return layerSpecs;
        }
    }

    bool UI_BrushPresetService::Can_CreatePatternOverlayFromImage(const UI_Document& document, const UI_EditorReadModel& readModel, ObjectId sourceObjectId) const
    {
        const UI_Object* sourceObject = document.Find_Object(sourceObjectId);
        if (nullptr == sourceObject || sourceObject->type != UI_ObjectType::Image)
            return false;

        return !readModel.Is_EffectivelyLocked(sourceObjectId);
    }

    ObjectId UI_BrushPresetService::Create_PatternOverlayFromImage(UI_Document& document, const UI_EditorReadModel& readModel, ObjectId sourceObjectId) const
    {
        if (!Can_CreatePatternOverlayFromImage(document, readModel, sourceObjectId))
            return 0;

        const UI_Object source = *static_cast<const UI_Document&>(document).Find_Object(sourceObjectId);
        const int32 overlaySortOrder = source.sortOrder + 1;

        const ObjectId overlayObjectId = UI_DocumentInternalAccess::Create_Object(document, UI_ObjectType::Image, source.parentId);
        UI_Object* overlayObject = UI_DocumentInternalAccess::Find_Object(document, overlayObjectId);
        if (nullptr == overlayObject)
            return 0;

        for (UI_Object& object : UI_DocumentInternalAccess::Get_MutableObjects(document))
        {
            if (object.id != overlayObjectId && object.sortOrder >= overlaySortOrder)
                ++object.sortOrder;
        }

        UI_Object overlay = source;
        overlay.id = overlayObjectId;
        overlay.name = document.Generate_UniqueName(source.name.empty() ? "PatternOverlay" : source.name + "_PatternOverlay", overlayObjectId);
        overlay.parentId = source.parentId;
        overlay.imageRenderMode = UI_ImageRenderMode::PatternOverlay;
        overlay.textureGuid.clear();
        overlay.texturePath.clear();
        overlay.patternMaskTextureGuid.clear();
        overlay.patternMaskTexturePath.clear();
        overlay.patternUvOffset = Get_DefaultPatternUvOffset();
        overlay.patternUvScale = Get_DefaultPatternUvScale();
        overlay.color = ImVec4(0.74f, 0.58f, 0.25f, 0.35f);
        overlay.visible = true;
        overlay.locked = false;
        overlay.sortOrder = overlaySortOrder;
        overlay.animationClips.clear();

        *overlayObject = overlay;
        return overlayObjectId;
    }

    bool UI_BrushPresetService::Can_CreateHoverBrushPreset(const UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId) const
    {
        const UI_Object* targetObject = static_cast<const UI_Document&>(document).Find_Object(targetObjectId);
        if (nullptr == targetObject || !Is_ContainerObjectType(targetObject->type))
            return false;

        if (readModel.Is_EffectivelyLocked(targetObjectId))
            return false;

        const ObjectId flowObjectId = Find_GeneratedChildByName(document, targetObjectId, kHoverBrushFlowName);
        return flowObjectId == 0 || !readModel.Is_EffectivelyLocked(flowObjectId);
    }

    bool UI_BrushPresetService::Create_HoverBrushPreset(UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId, bool allowStateAnimationOverwrite) const
    {
        UNREFERENCED_PARAMETER(allowStateAnimationOverwrite);

        if (!Can_CreateHoverBrushPreset(document, readModel, targetObjectId))
            return false;

        const ObjectId flowObjectId = Find_GeneratedChildByName(document, targetObjectId, kHoverBrushFlowName);
        const string flowName = flowObjectId != 0
            ? static_cast<const UI_Document&>(document).Find_Object(flowObjectId)->name
            : document.Generate_UniqueName(kHoverBrushFlowName);

        const UI_Object* targetObject = static_cast<const UI_Document&>(document).Find_Object(targetObjectId);
        if (nullptr == targetObject)
            return false;

        const ImVec2 targetSize = ImVec2(max(1.f, targetObject->size.x), max(1.f, targetObject->size.y));
        const ImVec2 targetCenter = ImVec2(targetSize.x * 0.5f, targetSize.y * 0.5f);
        const UI_RenderGroup targetRenderGroup = targetObject->renderGroup;

        int32 minChildSortOrder = numeric_limits<int32>::max();
        for (const UI_Object& object : document.Get_Objects())
        {
            if (object.parentId != targetObjectId)
                continue;

            minChildSortOrder = min(minChildSortOrder, object.sortOrder);
        }

        if (minChildSortOrder == numeric_limits<int32>::max())
            minChildSortOrder = targetObject->sortOrder;

        const int32 flowSortOrder = minChildSortOrder - 1;

        ObjectId resolvedObjectId = flowObjectId;
        if (resolvedObjectId == 0)
            resolvedObjectId = UI_DocumentInternalAccess::Create_Object(document, UI_ObjectType::Image, targetObjectId);

        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, resolvedObjectId);
        if (nullptr == object)
            return false;

        const ImVec2 flowSize = ImVec2(
            max(targetSize.x + 88.f, targetSize.x * 1.26f),
            max(targetSize.y + 28.f, targetSize.y * 1.5f));

        object->parentId = targetObjectId;
        object->name = flowName;
        object->type = UI_ObjectType::Image;
        object->position = targetCenter;
        object->size = flowSize;
        object->scale = ImVec2(1.f, 1.f);
        object->rotationRadians = 0.f;
        object->pivot = ImVec2(0.5f, 0.5f);
        object->anchorMin = ImVec2(0.f, 0.f);
        object->anchorMax = ImVec2(0.f, 0.f);
        object->color = ImVec4(1.f, 1.f, 1.f, 1.f);
        object->alphaMultiplier = 1.f;
        object->visible = true;
        object->locked = false;
        object->sortOrder = flowSortOrder;
        object->renderGroup = targetRenderGroup;
        object->textureGuid.clear();
        object->texturePath = kHoverBrushFlowTexturePath;
        object->imageRenderMode = UI_ImageRenderMode::MaterialVfx;
        object->maskTextureGuid.clear();
        object->maskTexturePath.clear();
        object->maskTextureMode = UI_MaskTextureMode::Alpha;
        object->patternMaskTextureGuid.clear();
        object->patternMaskTexturePath.clear();
        object->patternUvOffset = Get_DefaultPatternUvOffset();
        object->patternUvScale = Get_DefaultPatternUvScale();
        object->materialVfx = Get_DefaultMaterialVfxDesc();
        object->materialVfx.mode = UI_MaterialVfxMode::BrushFlow;
        object->materialVfx.color = ImVec4(0.025f, 0.021f, 0.018f, 0.92f);
        object->materialVfx.coreColor = ImVec4(0.42f, 0.18f, 0.055f, 0.58f);
        object->materialVfx.glow = 0.65f;
        object->materialVfx.alpha = 1.f;
        object->materialVfx.coreStrength = 0.9f;
        object->materialVfx.coreWidth = 0.09f;
        object->materialVfx.coreSoftness = 0.1f;
        object->materialVfx.noiseStrength = 0.82f;
        object->materialVfx.uvScroll = ImVec2(0.48f, 0.08f);
        object->materialVfx.edgeWidth = 0.3f;
        object->materialVfx.edgeSoftness = 0.18f;
        object->materialVfx.erosion = 0.13f;
        object->materialVfx.flicker = 0.28f;
        object->materialVfx.distortion = 0.044f;
        object->materialVfx.edgeWobbleSpeed = 2.85f;
        object->buttonSheen = Get_DefaultButtonSheenDesc();
        object->useFillRatio = false;
        object->fillRatio = Get_DefaultFillRatio();
        object->fillOrigin = Get_DefaultFillOrigin();
        object->vfxPath.clear();
        object->vfxPlayOnOpen = true;
        object->vfxLoop = false;
        object->vfxStopOnClose = true;
        object->stateAnimationPath.clear();
        object->animationClips.clear();

        return true;
    }

    bool UI_BrushPresetService::Can_CreateBrushAtmospherePreset(const UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId) const
    {
        const UI_Object* targetObject = static_cast<const UI_Document&>(document).Find_Object(targetObjectId);
        if (nullptr == targetObject)
            return false;

        const bool createAsChildren = Is_ContainerObjectType(targetObject->type);
        if (!createAsChildren && targetObject->type != UI_ObjectType::Image)
            return false;

        if (readModel.Is_EffectivelyLocked(targetObjectId))
            return false;

        const ObjectId layerParentId = createAsChildren ? targetObject->id : targetObject->parentId;
        size_t layerSpecCount = 0;
        const BrushAtmosphereLayerSpec* layerSpecs = Get_BrushAtmosphereLayerSpecs(layerSpecCount);
        for (size_t layerIndex = 0; layerIndex < layerSpecCount; ++layerIndex)
        {
            const string layerBaseName = Make_BrushAtmosphereLayerBaseName(*targetObject, layerSpecs[layerIndex].name);
            const ObjectId layerObjectId = Find_GeneratedChildByName(document, layerParentId, layerBaseName);
            if (layerObjectId != 0 && readModel.Is_EffectivelyLocked(layerObjectId))
                return false;
        }

        return true;
    }

    bool UI_BrushPresetService::Create_BrushAtmospherePreset(UI_Document& document, const UI_EditorReadModel& readModel, ObjectId targetObjectId) const
    {
        if (!Can_CreateBrushAtmospherePreset(document, readModel, targetObjectId))
            return false;

        const UI_Object* targetObject = static_cast<const UI_Document&>(document).Find_Object(targetObjectId);
        if (nullptr == targetObject)
            return false;

        const bool createAsChildren = Is_ContainerObjectType(targetObject->type);
        const UI_Object target = *targetObject;
        const ObjectId layerParentId = createAsChildren ? target.id : target.parentId;
        const ImVec2 targetSize = document.Get_ResolvedLocalSize(targetObjectId);
        const ImVec2 targetPosition = document.Get_ResolvedLocalPosition(targetObjectId);

        size_t layerSpecCount = 0;
        const BrushAtmosphereLayerSpec* layerSpecs = Get_BrushAtmosphereLayerSpecs(layerSpecCount);

        vector<string> layerBaseNames;
        vector<ObjectId> layerObjectIds;
        layerBaseNames.reserve(layerSpecCount);
        layerObjectIds.reserve(layerSpecCount);

        for (size_t layerIndex = 0; layerIndex < layerSpecCount; ++layerIndex)
        {
            const BrushAtmosphereLayerSpec& layerSpec = layerSpecs[layerIndex];
            const string layerBaseName = Make_BrushAtmosphereLayerBaseName(target, layerSpec.name);
            const ObjectId layerObjectId = Find_GeneratedChildByName(document, layerParentId, layerBaseName);
            if (layerObjectId != 0 && readModel.Is_EffectivelyLocked(layerObjectId))
                return false;

            layerBaseNames.push_back(layerBaseName);
            layerObjectIds.push_back(layerObjectId);
        }

        int32 minLayerSortOrder = target.sortOrder;
        int32 maxLayerSortOrder = target.sortOrder;
        if (createAsChildren)
        {
            minLayerSortOrder = numeric_limits<int32>::max();
            maxLayerSortOrder = numeric_limits<int32>::min();
            for (const UI_Object& object : document.Get_Objects())
            {
                if (object.parentId != target.id)
                    continue;

                minLayerSortOrder = min(minLayerSortOrder, object.sortOrder);
                maxLayerSortOrder = max(maxLayerSortOrder, object.sortOrder);
            }

            if (minLayerSortOrder == numeric_limits<int32>::max())
            {
                minLayerSortOrder = target.sortOrder;
                maxLayerSortOrder = target.sortOrder;
            }
        }

        auto resolveLayerSortOrder = [&](int32 sortOffset) -> int32
        {
            return sortOffset > 0
                ? maxLayerSortOrder + sortOffset
                : minLayerSortOrder + sortOffset;
        };

        for (size_t layerIndex = 0; layerIndex < layerSpecCount; ++layerIndex)
        {
            const BrushAtmosphereLayerSpec& layerSpec = layerSpecs[layerIndex];
            string layerName = document.Generate_UniqueName(layerBaseNames[layerIndex]);
            if (layerObjectIds[layerIndex] != 0)
            {
                const UI_Object* existingLayerObject = static_cast<const UI_Document&>(document).Find_Object(layerObjectIds[layerIndex]);
                if (nullptr == existingLayerObject)
                    return false;

                layerName = existingLayerObject->name;
            }

            ObjectId resolvedObjectId = layerObjectIds[layerIndex];
            if (resolvedObjectId == 0)
                resolvedObjectId = UI_DocumentInternalAccess::Create_Object(document, UI_ObjectType::Image, layerParentId);

            UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, resolvedObjectId);
            if (nullptr == object)
                return false;

            const ImVec2 layerSize = ImVec2(
                max(8.f, targetSize.x * layerSpec.sizeScale.x + layerSpec.sizePadding.x),
                max(8.f, targetSize.y * layerSpec.sizeScale.y + layerSpec.sizePadding.y));
            const ImVec2 layerOffset = ImVec2(
                targetSize.x * layerSpec.offsetScale.x,
                targetSize.y * layerSpec.offsetScale.y);
            const ImVec2 targetCenter = createAsChildren
                ? ImVec2(targetSize.x * 0.5f, targetSize.y * 0.5f)
                : ImVec2(
                    targetPosition.x + (0.5f - target.pivot.x) * targetSize.x,
                    targetPosition.y + (0.5f - target.pivot.y) * targetSize.y);

            object->parentId = layerParentId;
            object->name = layerName;
            object->type = UI_ObjectType::Image;
            object->position = ImVec2(targetCenter.x + layerOffset.x, targetCenter.y + layerOffset.y);
            object->size = layerSize;
            object->scale = createAsChildren ? ImVec2(1.f, 1.f) : target.scale;
            object->pivot = ImVec2(0.5f, 0.5f);
            object->anchorMin = createAsChildren ? ImVec2(0.f, 0.f) : target.anchorMin;
            object->anchorMax = createAsChildren ? ImVec2(0.f, 0.f) : target.anchorMax;
            object->rotationRadians = (createAsChildren ? 0.f : target.rotationRadians) + layerSpec.rotationOffsetRadians;
            object->color = layerSpec.color;
            object->alphaMultiplier = 1.f;
            object->isOpenOnCreate = true;
            object->visible = true;
            object->locked = false;
            object->sortOrder = resolveLayerSortOrder(layerSpec.sortOffset);
            object->renderGroup = target.renderGroup;
            object->textureGuid.clear();
            object->texturePath = layerSpec.texturePath;
            object->maskTextureGuid.clear();
            object->maskTexturePath.clear();
            object->maskTextureMode = UI_MaskTextureMode::Alpha;
            object->patternMaskTextureGuid.clear();
            object->patternMaskTexturePath.clear();
            object->patternUvOffset = Get_DefaultPatternUvOffset();
            object->patternUvScale = Get_DefaultPatternUvScale();
            object->buttonSheen = Get_DefaultButtonSheenDesc();
            object->useFillRatio = false;
            object->fillRatio = Get_DefaultFillRatio();
            object->fillOrigin = Get_DefaultFillOrigin();
            object->vfxPath.clear();
            object->vfxPlayOnOpen = true;
            object->vfxLoop = false;
            object->vfxStopOnClose = true;
            object->stateAnimationPath.clear();
            object->animationClips.clear();

            object->materialVfx = Get_DefaultMaterialVfxDesc();
            if (layerSpec.materialVfx)
            {
                object->imageRenderMode = UI_ImageRenderMode::MaterialVfx;
                object->materialVfx.mode = UI_MaterialVfxMode::BrushFlow;
                object->materialVfx.color = ImVec4(0.022f, 0.018f, 0.014f, 0.72f);
                object->materialVfx.coreColor = ImVec4(0.72f, 0.39f, 0.14f, 0.32f);
                object->materialVfx.flowColor = ImVec4(1.f, 0.66f, 0.22f, 0.18f);
                object->materialVfx.glow = 0.55f;
                object->materialVfx.alpha = 0.78f;
                object->materialVfx.coreStrength = 0.72f;
                object->materialVfx.coreWidth = 0.08f;
                object->materialVfx.coreSoftness = 0.12f;
                object->materialVfx.noiseStrength = 0.76f;
                object->materialVfx.uvScroll = ImVec2(0.42f, 0.08f);
                object->materialVfx.flowSpeed = 0.86f;
                object->materialVfx.preserveColor = 0.f;
                object->materialVfx.edgeWidth = 0.28f;
                object->materialVfx.edgeSoftness = 0.20f;
                object->materialVfx.erosion = 0.10f;
                object->materialVfx.flicker = 0.20f;
                object->materialVfx.distortion = 0.034f;
                object->materialVfx.edgeWobbleSpeed = 2.35f;
            }
            else
            {
                object->imageRenderMode = UI_ImageRenderMode::Normal;
            }
        }

        return true;
    }
}
