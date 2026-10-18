#pragma once

#include "Engine_Define.h"
#include "UI_Editor_ImGuiInclude.h"
#include "UI_Editor_Enum.h"
#include "UI_AnimationTypes.h"

NS_BEGIN(UIEditor)

using ObjectId = uint32;

inline constexpr float kPatternUvOffsetMin = -1.f;
inline constexpr float kPatternUvOffsetMax = 1.f;
inline constexpr float kPatternUvScaleMin = 0.1f;
inline constexpr float kPatternUvScaleMax = 8.f;
inline constexpr float kFillRatioMin = 0.f;
inline constexpr float kFillRatioMax = 1.f;
inline constexpr float kMaterialVfxGlowMin = 0.f;
inline constexpr float kMaterialVfxGlowMax = 12.f;
inline constexpr float kMaterialVfxAlphaMin = 0.f;
inline constexpr float kMaterialVfxAlphaMax = 2.f;
inline constexpr float kMaterialVfxNoiseStrengthMin = 0.f;
inline constexpr float kMaterialVfxNoiseStrengthMax = 1.f;
inline constexpr float kMaterialVfxFlowSpeedMin = 0.f;
inline constexpr float kMaterialVfxFlowSpeedMax = 4.f;
inline constexpr float kMaterialVfxPreserveColorMin = 0.f;
inline constexpr float kMaterialVfxPreserveColorMax = 1.f;
inline constexpr float kMaterialVfxEdgeWidthMin = 0.001f;
inline constexpr float kMaterialVfxEdgeWidthMax = 0.5f;
inline constexpr float kMaterialVfxEdgeSoftnessMin = 0.001f;
inline constexpr float kMaterialVfxEdgeSoftnessMax = 0.5f;
inline constexpr float kMaterialVfxErosionMin = 0.f;
inline constexpr float kMaterialVfxErosionMax = 1.f;
inline constexpr float kMaterialVfxFlickerMin = 0.f;
inline constexpr float kMaterialVfxFlickerMax = 1.f;
inline constexpr float kMaterialVfxDistortionMin = 0.f;
inline constexpr float kMaterialVfxDistortionMax = 0.1f;
inline constexpr float kMaterialVfxEdgeWobbleSpeedMin = 0.f;
inline constexpr float kMaterialVfxEdgeWobbleSpeedMax = 4.f;
inline constexpr float kMaterialVfxCoreStrengthMin = 0.f;
inline constexpr float kMaterialVfxCoreStrengthMax = 8.f;
inline constexpr float kMaterialVfxCoreWidthMin = 0.001f;
inline constexpr float kMaterialVfxCoreWidthMax = 0.5f;
inline constexpr float kMaterialVfxCoreSoftnessMin = 0.001f;
inline constexpr float kMaterialVfxCoreSoftnessMax = 0.5f;
inline constexpr float kButtonSheenSpeedMin = 0.f;
inline constexpr float kButtonSheenSpeedMax = 4.f;
inline constexpr float kButtonSheenWidthMin = 0.001f;
inline constexpr float kButtonSheenWidthMax = 1.f;
inline constexpr float kButtonSheenSoftnessMin = 0.001f;
inline constexpr float kButtonSheenSoftnessMax = 1.f;
inline constexpr float kButtonSheenAngleMin = -2.f;
inline constexpr float kButtonSheenAngleMax = 2.f;
inline constexpr float kButtonSheenNoiseStrengthMin = 0.f;
inline constexpr float kButtonSheenNoiseStrengthMax = 1.f;

inline ImVec2 Get_DefaultPatternUvOffset()
{
    return ImVec2(0.f, 0.f);
}

inline ImVec2 Get_DefaultPatternUvScale()
{
    return ImVec2(1.f, 1.f);
}

inline float Get_DefaultFillRatio()
{
    return 1.f;
}

inline UI_FillOrigin Get_DefaultFillOrigin()
{
    return UI_FillOrigin::LeftToRight;
}

struct UI_MaterialVfxDesc
{
    UI_MaterialVfxMode mode = UI_MaterialVfxMode::Energy;
    string noiseTextureGuid = "";
    string noiseTexturePath = "";
    ImVec4 color = ImVec4(1.f, 0.12f, 0.95f, 1.f);
    ImVec4 coreColor = ImVec4(1.f, 0.92f, 1.f, 1.f);
    ImVec4 flowColor = ImVec4(1.f, 1.f, 1.f, 0.f);
    float glow = 4.f;
    float alpha = 1.f;
    float coreStrength = 2.2f;
    float coreWidth = 0.13f;
    float coreSoftness = 0.05f;
    float noiseStrength = 0.35f;
    ImVec2 uvScroll = ImVec2(0.08f, -0.18f);
    float flowSpeed = 1.f;
    float preserveColor = 0.f;
    float edgeWidth = 0.18f;
    float edgeSoftness = 0.08f;
    float erosion = 0.18f;
    float flicker = 0.22f;
    float distortion = 0.015f;
    float edgeWobbleSpeed = 1.f;
};

inline UI_MaterialVfxDesc Get_DefaultMaterialVfxDesc()
{
    return UI_MaterialVfxDesc{};
}

struct UI_ButtonSheenDesc
{
    ImVec4 color = ImVec4(1.f, 0.960784f, 0.882353f, 0.588235f);
    float speed = 0.75f;
    float width = 0.18f;
    float softness = 0.12f;
    float angle = -0.08f;
    float noiseStrength = 0.35f;
};

inline UI_ButtonSheenDesc Get_DefaultButtonSheenDesc()
{
    return UI_ButtonSheenDesc{};
}

inline ImVec2 Clamp_PatternUvOffset(const ImVec2& value)
{
    return ImVec2(
        min(max(value.x, kPatternUvOffsetMin), kPatternUvOffsetMax),
        min(max(value.y, kPatternUvOffsetMin), kPatternUvOffsetMax));
}

inline ImVec2 Clamp_PatternUvScale(const ImVec2& value)
{
    return ImVec2(
        min(max(value.x, kPatternUvScaleMin), kPatternUvScaleMax),
        min(max(value.y, kPatternUvScaleMin), kPatternUvScaleMax));
}

inline float Clamp_FillRatio(float value)
{
    return min(max(value, kFillRatioMin), kFillRatioMax);
}

inline float Clamp_MaterialVfxGlow(float value)
{
    return min(max(value, kMaterialVfxGlowMin), kMaterialVfxGlowMax);
}

inline float Clamp_MaterialVfxAlpha(float value)
{
    return min(max(value, kMaterialVfxAlphaMin), kMaterialVfxAlphaMax);
}

inline float Clamp_MaterialVfxNoiseStrength(float value)
{
    return min(max(value, kMaterialVfxNoiseStrengthMin), kMaterialVfxNoiseStrengthMax);
}

inline float Clamp_MaterialVfxFlowSpeed(float value)
{
    return min(max(value, kMaterialVfxFlowSpeedMin), kMaterialVfxFlowSpeedMax);
}

inline float Clamp_MaterialVfxPreserveColor(float value)
{
    return min(max(value, kMaterialVfxPreserveColorMin), kMaterialVfxPreserveColorMax);
}

inline float Clamp_MaterialVfxEdgeWidth(float value)
{
    return min(max(value, kMaterialVfxEdgeWidthMin), kMaterialVfxEdgeWidthMax);
}

inline float Clamp_MaterialVfxEdgeSoftness(float value)
{
    return min(max(value, kMaterialVfxEdgeSoftnessMin), kMaterialVfxEdgeSoftnessMax);
}

inline float Clamp_MaterialVfxErosion(float value)
{
    return min(max(value, kMaterialVfxErosionMin), kMaterialVfxErosionMax);
}

inline float Clamp_MaterialVfxFlicker(float value)
{
    return min(max(value, kMaterialVfxFlickerMin), kMaterialVfxFlickerMax);
}

inline float Clamp_MaterialVfxDistortion(float value)
{
    return min(max(value, kMaterialVfxDistortionMin), kMaterialVfxDistortionMax);
}

inline float Clamp_MaterialVfxEdgeWobbleSpeed(float value)
{
    return min(max(value, kMaterialVfxEdgeWobbleSpeedMin), kMaterialVfxEdgeWobbleSpeedMax);
}

inline float Clamp_MaterialVfxCoreStrength(float value)
{
    return min(max(value, kMaterialVfxCoreStrengthMin), kMaterialVfxCoreStrengthMax);
}

inline float Clamp_MaterialVfxCoreWidth(float value)
{
    return min(max(value, kMaterialVfxCoreWidthMin), kMaterialVfxCoreWidthMax);
}

inline float Clamp_MaterialVfxCoreSoftness(float value)
{
    return min(max(value, kMaterialVfxCoreSoftnessMin), kMaterialVfxCoreSoftnessMax);
}

inline float Clamp_ButtonSheenSpeed(float value)
{
    return min(max(value, kButtonSheenSpeedMin), kButtonSheenSpeedMax);
}

inline float Clamp_ButtonSheenWidth(float value)
{
    return min(max(value, kButtonSheenWidthMin), kButtonSheenWidthMax);
}

inline float Clamp_ButtonSheenSoftness(float value)
{
    return min(max(value, kButtonSheenSoftnessMin), kButtonSheenSoftnessMax);
}

inline float Clamp_ButtonSheenAngle(float value)
{
    return min(max(value, kButtonSheenAngleMin), kButtonSheenAngleMax);
}

inline float Clamp_ButtonSheenNoiseStrength(float value)
{
    return min(max(value, kButtonSheenNoiseStrengthMin), kButtonSheenNoiseStrengthMax);
}

struct UI_EDITOR_DESC
{
    HWND hWnd = nullptr;
    WinMode winMode = WinMode::Win;
    uint32 viewportWidth = 1600;
    uint32 viewportHeight = 900;
    bool assetManagerReady = false;
};

struct UI_Object
{
    ObjectId id = 0;
    ObjectId parentId = 0;
    string name = "";
    UI_ObjectType type = UI_ObjectType::Image;
    ImVec2 position = ImVec2(0.f, 0.f);
    ImVec2 size = ImVec2(100.f, 100.f);
    ImVec2 scale = ImVec2(1.f, 1.f);
    ImVec2 pivot = ImVec2(0.5f, 0.5f);
    ImVec2 anchorMin = ImVec2(0.f, 0.f);
    ImVec2 anchorMax = ImVec2(0.f, 0.f);
    float rotationRadians = 0.f;
    ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
    float alphaMultiplier = 1.f;
    bool isOpenOnCreate = true;
    bool visible = true;
    bool locked = false;
    int32 sortOrder = 0;
    UI_RenderGroup renderGroup = UI_RenderGroup::Foreground;
    string text = "";
    float fontSize = 32.f;
    UI_TextAlign textAlign = UI_TextAlign::Left;
    bool textWrap = false;
    string fontTag = "";
    string fontGuid = "";
    string fontPath = "";
    string textureGuid = "";
    string texturePath = "";
    UI_ImageRenderMode imageRenderMode = UI_ImageRenderMode::Normal;
    string maskTextureGuid = "";
    string maskTexturePath = "";
    UI_MaskTextureMode maskTextureMode = UI_MaskTextureMode::Alpha;
    string patternMaskTextureGuid = "";
    string patternMaskTexturePath = "";
    ImVec2 patternUvOffset = Get_DefaultPatternUvOffset();
    ImVec2 patternUvScale = Get_DefaultPatternUvScale();
    UI_MaterialVfxDesc materialVfx = Get_DefaultMaterialVfxDesc();
    UI_ButtonSheenDesc buttonSheen = Get_DefaultButtonSheenDesc();
    bool useFillRatio = false;
    float fillRatio = Get_DefaultFillRatio();
    UI_FillOrigin fillOrigin = Get_DefaultFillOrigin();
    string vfxPath = "";
    bool vfxPlayOnOpen = true;
    bool vfxLoop = false;
    bool vfxStopOnClose = true;
    string stateAnimationPath = "";
    vector<UI_AnimationClip> animationClips = {};
};

NS_END
