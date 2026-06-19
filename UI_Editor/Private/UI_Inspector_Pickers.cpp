#include "pch.h"
#include "UI_Inspector.h"

#include "GameInstance.h"
#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        constexpr const char* kBrushFlowPaintSlashTexturePath = "../Bin/Resources/Textures/UI/Common/Paint/T_Generic_PaintSlash_Text_Variation3.png";

        bool NearlyEqual(float lhs, float rhs)
        {
            return fabsf(lhs - rhs) <= 0.0001f;
        }

        bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs)
        {
            return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
        }

        bool Draw_PresetButton(const char* label, bool active, const char* tooltip)
        {
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.72f, 0.24f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 0.82f, 0.34f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.88f, 0.62f, 0.18f, 1.f));
            }

            const bool clicked = ImGui::Button(label, ImVec2(34.f, 24.f));

            if (active)
                ImGui::PopStyleColor(3);

            if (nullptr != tooltip && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltip);

            return clicked;
        }

        bool Is_UITextureFileExtension(const fs::path& path)
        {
            const string extension = String::ToLowerCopy(path.extension().string());
            return extension == ".png" ||
                extension == ".jpg" ||
                extension == ".jpeg" ||
                extension == ".dds" ||
                extension == ".tga";
        }

        bool Try_GetMissingMetaPathForTexture(const string& texturePath, fs::path& outMetaPath)
        {
            outMetaPath.clear();
            if (texturePath.empty())
                return false;

            const wstring resolvedTexturePath = Resolve_ClientRuntimeResourcePath(texturePath);
            if (resolvedTexturePath.empty())
                return false;

            try
            {
                const fs::path resolvedPath = fs::path(resolvedTexturePath).lexically_normal();
                if (!fs::exists(resolvedPath) || !fs::is_regular_file(resolvedPath) || !Is_UITextureFileExtension(resolvedPath))
                    return false;

                fs::path metaPath = resolvedPath;
                metaPath += L".meta";
                if (fs::exists(metaPath))
                    return false;

                outMetaPath = metaPath;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        wstring To_LowerW(wstring value)
        {
            transform(value.begin(), value.end(), value.begin(), [](wchar_t ch)
                {
                    return static_cast<wchar_t>(towlower(ch));
                });

            return value;
        }

        bool Starts_With(const wstring& value, const wstring& prefix)
        {
            return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
        }

        bool Ends_With(const wstring& value, const wstring& suffix)
        {
            return value.size() >= suffix.size() && equal(suffix.rbegin(), suffix.rend(), value.rbegin());
        }

        wstring Resolve_DefaultStateAnimationDirectory()
        {
            const wstring resourceRoot = Resolve_UIEditorClientResourceRoot();
            if (!resourceRoot.empty())
            {
                const fs::path stateAnimationDirectory = (fs::path(resourceRoot) / L"UI" / L"UI_StateAnimations").lexically_normal();
                try
                {
                    if (fs::exists(stateAnimationDirectory) && fs::is_directory(stateAnimationDirectory))
                        return stateAnimationDirectory.wstring();
                }
                catch (const exception& exception)
                {
                    LOG_WARN("UI inspector state animation directory check failed: {} ({})",
                        exception.what(),
                        String::ToString(stateAnimationDirectory.wstring()));
                }
            }

            return {};
        }

        bool Show_StateAnimationSelectDialog(wstring& outFilePath)
        {
            wchar_t filePathBuffer[MAX_PATH] = {};
            const wstring initialDirectory = Resolve_DefaultStateAnimationDirectory();

            OPENFILENAMEW openFileName = {};
            openFileName.lStructSize = sizeof(openFileName);
            openFileName.lpstrFilter = L"UI State Animation Files\0*.uistateanim.json\0JSON Files\0*.json\0All Files\0*.*\0";
            openFileName.lpstrFile = filePathBuffer;
            openFileName.nMaxFile = MAX_PATH;
            if (!initialDirectory.empty())
                openFileName.lpstrInitialDir = initialDirectory.c_str();
            openFileName.lpstrDefExt = L"uistateanim.json";
            openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (FALSE == GetOpenFileNameW(&openFileName))
                return false;

            outFilePath = filePathBuffer;
            return !outFilePath.empty();
        }

        bool Try_MakeStateAnimationResourcePath(const wstring& filePath, string& outResourcePath)
        {
            outResourcePath.clear();
            if (filePath.empty())
                return false;

            try
            {
                const wstring resourceRootValue = Resolve_UIEditorClientResourceRoot();
                if (resourceRootValue.empty())
                    return false;

                const fs::path selectedPath = fs::weakly_canonical(fs::path(filePath));
                const fs::path resourceRoot = fs::weakly_canonical(fs::path(resourceRootValue));
                fs::path relativePath = selectedPath.lexically_relative(resourceRoot);
                if (relativePath.empty())
                    return false;

                for (const fs::path& part : relativePath)
                {
                    if (part.native() == L"..")
                        return false;
                }

                const wstring lowerRelativePath = To_LowerW(relativePath.generic_wstring());
                if (!Starts_With(lowerRelativePath, L"ui/ui_stateanimations/"))
                    return false;

                constexpr wchar_t kStateAnimationExtension[] = L".uistateanim.json";
                const wstring lowerFileName = To_LowerW(relativePath.filename().wstring());
                if (!Ends_With(lowerFileName, kStateAnimationExtension))
                    return false;

                wstring fileNameWithoutExtension = relativePath.filename().wstring();
                fileNameWithoutExtension.resize(fileNameWithoutExtension.size() - wcslen(kStateAnimationExtension));
                relativePath.replace_filename(fileNameWithoutExtension);

                outResourcePath = String::ToString(relativePath.generic_wstring());
                return !outResourcePath.empty();
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI inspector state animation resource path convert failed: {} ({})",
                    exception.what(),
                    String::ToString(filePath));
                return false;
            }
        }
    }

    void UI_Inspector::Render_LayoutPresets(const UI_Object& object)
    {
        struct Preset
        {
            const char* label = "";
            const char* tooltip = "";
            ImVec2 value = ImVec2(0.f, 0.f);
        };

        const Preset presets[3][3] =
        {
            {
                { "TL", "Set to (0, 0)", ImVec2(0.f, 0.f) },
                { "T", "Set to (0.5, 0)", ImVec2(0.5f, 0.f) },
                { "TR", "Set to (1, 0)", ImVec2(1.f, 0.f) },
            },
            {
                { "L", "Set to (0, 0.5)", ImVec2(0.f, 0.5f) },
                { "C", "Set to (0.5, 0.5)", ImVec2(0.5f, 0.5f) },
                { "R", "Set to (1, 0.5)", ImVec2(1.f, 0.5f) },
            },
            {
                { "BL", "Set to (0, 1)", ImVec2(0.f, 1.f) },
                { "B", "Set to (0.5, 1)", ImVec2(0.5f, 1.f) },
                { "BR", "Set to (1, 1)", ImVec2(1.f, 1.f) },
            },
        };

        ImGui::Separator();
        ImGui::TextUnformatted("Pivot Presets (Keep Rect)");
        ImGui::PushID("PivotKeepRectPresets");
        for (int32 row = 0; row < 3; ++row)
        {
            for (int32 column = 0; column < 3; ++column)
            {
                const Preset& preset = presets[row][column];
                if (column > 0)
                    ImGui::SameLine();

                if (Draw_PresetButton(preset.label, NearlyEqual(object.pivot, preset.value), preset.tooltip))
                    _editorManager.Set_UIObjectPivotKeepWorldRect(object.id, preset.value);
            }
        }
        ImGui::PopID();

        ImGui::TextUnformatted("Anchor Presets (Keep Rect)");
        ImGui::PushID("AnchorKeepRectPresets");
        for (int32 row = 0; row < 3; ++row)
        {
            for (int32 column = 0; column < 3; ++column)
            {
                const Preset& preset = presets[row][column];
                if (column > 0)
                    ImGui::SameLine();

                const bool active = NearlyEqual(object.anchorMin, preset.value) && NearlyEqual(object.anchorMax, preset.value);
                if (Draw_PresetButton(preset.label, active, preset.tooltip))
                    _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, preset.value, preset.value);
            }
        }
        ImGui::PopID();

        const float anchorCenterX = min(max((object.anchorMin.x + object.anchorMax.x) * 0.5f, 0.f), 1.f);
        const float anchorCenterY = min(max((object.anchorMin.y + object.anchorMax.y) * 0.5f, 0.f), 1.f);
        const ImVec2 horizontalMin = ImVec2(0.f, anchorCenterY);
        const ImVec2 horizontalMax = ImVec2(1.f, anchorCenterY);
        const ImVec2 verticalMin = ImVec2(anchorCenterX, 0.f);
        const ImVec2 verticalMax = ImVec2(anchorCenterX, 1.f);
        const ImVec2 fullMin = ImVec2(0.f, 0.f);
        const ImVec2 fullMax = ImVec2(1.f, 1.f);

        ImGui::TextUnformatted("Anchor Stretch (Keep Rect)");
        ImGui::PushID("AnchorStretchKeepRect");
        if (Draw_PresetButton("H", NearlyEqual(object.anchorMin, horizontalMin) && NearlyEqual(object.anchorMax, horizontalMax), "Stretch horizontally, keep current Y anchor"))
            _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, horizontalMin, horizontalMax);
        ImGui::SameLine();
        if (Draw_PresetButton("V", NearlyEqual(object.anchorMin, verticalMin) && NearlyEqual(object.anchorMax, verticalMax), "Stretch vertically, keep current X anchor"))
            _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, verticalMin, verticalMax);
        ImGui::SameLine();
        if (Draw_PresetButton("HV", NearlyEqual(object.anchorMin, fullMin) && NearlyEqual(object.anchorMax, fullMax), "Stretch both axes"))
            _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, fullMin, fullMax);
        ImGui::PopID();
    }

    void UI_Inspector::Render_TexturePicker(const UI_Object& object, ObjectId& outCreatePatternOverlaySourceId)
    {
        ImGui::Separator();

        const char* renderModeItems[] = { "Normal", "Pattern Overlay", "Material VFX", "Legacy Button Sheen" };
        UI_ImageRenderMode effectiveRenderMode = object.imageRenderMode;
        int32 currentRenderMode = static_cast<int32>(effectiveRenderMode);
        currentRenderMode = min(max(currentRenderMode, 0), static_cast<int32>(size(renderModeItems)) - 1);
        if (ImGui::Combo("Image Render Mode", &currentRenderMode, renderModeItems, static_cast<int32>(size(renderModeItems))))
        {
            effectiveRenderMode = static_cast<UI_ImageRenderMode>(currentRenderMode);
            _editorManager.Set_UIObjectImageRenderMode(object.id, effectiveRenderMode);
        }

        if (effectiveRenderMode == UI_ImageRenderMode::Normal)
        {
            if (ImGui::Button("Create Pattern Overlay From Selected Paint"))
                outCreatePatternOverlaySourceId = object.id;
        }
        else if (effectiveRenderMode == UI_ImageRenderMode::MaterialVfx)
        {
            UI_MaterialVfxDesc preset = object.materialVfx;
            preset.mode = UI_MaterialVfxMode::Energy;
            preset.color = ImVec4(1.f, 0.12f, 0.95f, 1.f);
            preset.coreColor = ImVec4(1.f, 0.92f, 1.f, 1.f);
            preset.glow = 5.5f;
            preset.alpha = 1.2f;
            preset.coreStrength = 2.8f;
            preset.coreWidth = 0.12f;
            preset.coreSoftness = 0.04f;
            preset.noiseStrength = 0.42f;
            preset.uvScroll = ImVec2(0.08f, -0.22f);
            preset.edgeWidth = 0.16f;
            preset.edgeSoftness = 0.055f;
            preset.erosion = 0.16f;
            preset.flicker = 0.28f;
            preset.distortion = 0.018f;
            preset.edgeWobbleSpeed = 1.f;

            if (ImGui::Button("Purple Energy Preset"))
                _editorManager.Set_UIObjectMaterialVfx(object.id, preset);
        }
        else if (effectiveRenderMode == UI_ImageRenderMode::ButtonSheen)
        {
            UI_ButtonSheenDesc preset = Get_DefaultButtonSheenDesc();
            if (ImGui::Button("Legacy Button Sheen Preset"))
                _editorManager.Set_UIObjectButtonSheen(object.id, preset);
        }

        ImGui::Separator();

        const string popupId = "UITexturePicker##" + to_string(object.id);
        string nextTextureGuid = object.textureGuid;
        string nextTexturePath = object.texturePath;
        const char* textureLabel = "UI Texture / VFX Base";
        if (effectiveRenderMode == UI_ImageRenderMode::PatternOverlay)
            textureLabel = "Pattern Texture";
        else if (effectiveRenderMode == UI_ImageRenderMode::MaterialVfx && object.materialVfx.mode == UI_MaterialVfxMode::BrushFlow)
            textureLabel = "Flow Brush Texture";
        else if (effectiveRenderMode == UI_ImageRenderMode::ButtonSheen)
            textureLabel = "Legacy Sheen Texture";
        if (Draw_UITextureSlotPicker(
            textureLabel,
            popupId.c_str(),
            nextTextureGuid,
            nextTexturePath))
        {
            _editorManager.Set_UIObjectTextureBinding(object.id, nextTextureGuid, nextTexturePath);
        }

        ImGui::Separator();

        const string maskPopupId = "UIMaskTexturePicker##" + to_string(object.id);
        string nextMaskTextureGuid = object.maskTextureGuid;
        string nextMaskTexturePath = object.maskTexturePath;
        const char* maskLabel = "UI Mask Texture / VFX Mask";
        if (effectiveRenderMode == UI_ImageRenderMode::PatternOverlay)
            maskLabel = "Paint Mask";
        else if (effectiveRenderMode == UI_ImageRenderMode::MaterialVfx && object.materialVfx.mode == UI_MaterialVfxMode::BrushFlow)
            maskLabel = "Optional Button Mask";
        else if (effectiveRenderMode == UI_ImageRenderMode::ButtonSheen)
            maskLabel = "Legacy Button Mask";
        if (Draw_UITextureSlotPicker(
            maskLabel,
            maskPopupId.c_str(),
            nextMaskTextureGuid,
            nextMaskTexturePath))
        {
            _editorManager.Set_UIObjectMaskTextureBinding(object.id, nextMaskTextureGuid, nextMaskTexturePath);
        }

        if (effectiveRenderMode == UI_ImageRenderMode::Normal || effectiveRenderMode == UI_ImageRenderMode::MaterialVfx)
        {
            const char* maskTextureModeItems[] = { "Alpha", "BlackWhite" };
            int32 currentMaskTextureMode = static_cast<int32>(object.maskTextureMode);
            currentMaskTextureMode = min(max(currentMaskTextureMode, 0), static_cast<int32>(size(maskTextureModeItems)) - 1);
            if (ImGui::Combo("Mask Texture Mode", &currentMaskTextureMode, maskTextureModeItems, static_cast<int32>(size(maskTextureModeItems))))
                _editorManager.Set_UIObjectMaskTextureMode(object.id, static_cast<UI_MaskTextureMode>(currentMaskTextureMode));
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Fill Reveal");

        bool nextUseFillRatio = object.useFillRatio;
        if (ImGui::Checkbox("Use Fill Ratio", &nextUseFillRatio))
            _editorManager.Set_UIObjectUseFillRatio(object.id, nextUseFillRatio);

        ImGui::BeginDisabled(!nextUseFillRatio);
        float nextFillRatio = object.fillRatio;
        if (ImGui::SliderFloat("Fill Ratio", &nextFillRatio, kFillRatioMin, kFillRatioMax, "%.3f"))
            _editorManager.Set_UIObjectFillRatio(object.id, nextFillRatio);

        const char* fillOriginItems[] = { "Left To Right", "Right To Left", "Top To Bottom", "Bottom To Top" };
        int32 currentFillOrigin = static_cast<int32>(object.fillOrigin);
        currentFillOrigin = min(max(currentFillOrigin, 0), static_cast<int32>(size(fillOriginItems)) - 1);
        if (ImGui::Combo("Fill Origin", &currentFillOrigin, fillOriginItems, static_cast<int32>(size(fillOriginItems))))
            _editorManager.Set_UIObjectFillOrigin(object.id, static_cast<UI_FillOrigin>(currentFillOrigin));
        ImGui::EndDisabled();

        if (effectiveRenderMode == UI_ImageRenderMode::PatternOverlay)
        {
            ImGui::Separator();

            const string patternMaskPopupId = "UIPatternMaskTexturePicker##" + to_string(object.id);
            string nextPatternMaskTextureGuid = object.patternMaskTextureGuid;
            string nextPatternMaskTexturePath = object.patternMaskTexturePath;
            if (Draw_UITextureSlotPicker(
                "Pattern Mask",
                patternMaskPopupId.c_str(),
                nextPatternMaskTextureGuid,
                nextPatternMaskTexturePath))
            {
                _editorManager.Set_UIObjectPatternMaskTextureBinding(object.id, nextPatternMaskTextureGuid, nextPatternMaskTexturePath);
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Pattern UV");

            ImVec2 nextPatternUvOffset = object.patternUvOffset;
            if (ImGui::DragFloat2("Pattern UV Offset", &nextPatternUvOffset.x, 0.01f, kPatternUvOffsetMin, kPatternUvOffsetMax, "%.3f"))
                _editorManager.Set_UIObjectPatternUvOffset(object.id, nextPatternUvOffset);

            ImVec2 nextPatternUvScale = object.patternUvScale;
            if (ImGui::DragFloat2("Pattern UV Scale", &nextPatternUvScale.x, 0.01f, kPatternUvScaleMin, kPatternUvScaleMax, "%.3f"))
                _editorManager.Set_UIObjectPatternUvScale(object.id, nextPatternUvScale);

            if (ImGui::Button(("Reset Pattern UV##" + to_string(object.id)).c_str()))
            {
                _editorManager.Set_UIObjectPatternUv(object.id, Get_DefaultPatternUvOffset(), Get_DefaultPatternUvScale());
            }
        }

        if (effectiveRenderMode == UI_ImageRenderMode::MaterialVfx)
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Material VFX");

            UI_MaterialVfxDesc nextMaterialVfx = object.materialVfx;
            bool materialChanged = false;

            const char* materialVfxModeItems[] = { "Energy", "Brush Flow" };
            int32 currentMaterialVfxMode = static_cast<int32>(nextMaterialVfx.mode);
            currentMaterialVfxMode = min(max(currentMaterialVfxMode, 0), static_cast<int32>(size(materialVfxModeItems)) - 1);
            if (ImGui::Combo("Mode", &currentMaterialVfxMode, materialVfxModeItems, static_cast<int32>(size(materialVfxModeItems))))
            {
                nextMaterialVfx.mode = static_cast<UI_MaterialVfxMode>(currentMaterialVfxMode);
                materialChanged = true;
            }

            UI_MaterialVfxDesc brushFlowPreset = nextMaterialVfx;
            brushFlowPreset.mode = UI_MaterialVfxMode::BrushFlow;
            brushFlowPreset.color = ImVec4(0.025f, 0.021f, 0.018f, 0.92f);
            brushFlowPreset.coreColor = ImVec4(0.42f, 0.18f, 0.055f, 0.58f);
            brushFlowPreset.glow = 0.65f;
            brushFlowPreset.alpha = 1.f;
            brushFlowPreset.coreStrength = 0.9f;
            brushFlowPreset.coreWidth = 0.09f;
            brushFlowPreset.coreSoftness = 0.1f;
            brushFlowPreset.noiseStrength = 0.82f;
            brushFlowPreset.uvScroll = ImVec2(0.48f, 0.08f);
            brushFlowPreset.edgeWidth = 0.3f;
            brushFlowPreset.edgeSoftness = 0.18f;
            brushFlowPreset.erosion = 0.13f;
            brushFlowPreset.flicker = 0.28f;
            brushFlowPreset.distortion = 0.044f;
            brushFlowPreset.edgeWobbleSpeed = 2.85f;

            if (ImGui::Button("Button Ink Flow Preset"))
            {
                _editorManager.Set_UIObjectTextureBinding(object.id, "", kBrushFlowPaintSlashTexturePath);
                _editorManager.Set_UIObjectMaterialVfx(object.id, brushFlowPreset);
            }

            if (nextMaterialVfx.mode == UI_MaterialVfxMode::BrushFlow)
            {
                if (ImGui::Button("Boost Brush Flow Motion"))
                {
                    UI_MaterialVfxDesc dynamicBorder = nextMaterialVfx;
                    if (dynamicBorder.uvScroll.x < 0.f)
                        dynamicBorder.uvScroll.x = min(dynamicBorder.uvScroll.x, -0.58f);
                    else
                        dynamicBorder.uvScroll.x = max(dynamicBorder.uvScroll.x, 0.58f);
                    dynamicBorder.uvScroll.y = dynamicBorder.uvScroll.y < 0.f
                        ? min(dynamicBorder.uvScroll.y, -0.1f)
                        : max(dynamicBorder.uvScroll.y, 0.1f);
                    dynamicBorder.noiseStrength = max(dynamicBorder.noiseStrength, 0.88f);
                    dynamicBorder.edgeWidth = max(dynamicBorder.edgeWidth, 0.34f);
                    dynamicBorder.edgeSoftness = max(dynamicBorder.edgeSoftness, 0.2f);
                    dynamicBorder.flicker = max(dynamicBorder.flicker, 0.36f);
                    dynamicBorder.distortion = max(dynamicBorder.distortion, 0.058f);
                    dynamicBorder.edgeWobbleSpeed = max(dynamicBorder.edgeWobbleSpeed, 3.35f);
                    dynamicBorder.glow = max(dynamicBorder.glow, 0.9f);
                    dynamicBorder.coreStrength = max(dynamicBorder.coreStrength, 1.05f);
                    _editorManager.Set_UIObjectMaterialVfx(object.id, dynamicBorder);
                }
            }

            const string noisePopupId = "UIMaterialVfxNoisePicker##" + to_string(object.id);
            string nextNoiseTextureGuid = nextMaterialVfx.noiseTextureGuid;
            string nextNoiseTexturePath = nextMaterialVfx.noiseTexturePath;
            const char* noiseLabel = nextMaterialVfx.mode == UI_MaterialVfxMode::BrushFlow ? "Optional Noise Texture" : "Noise Texture";
            if (Draw_UITextureSlotPicker(noiseLabel, noisePopupId.c_str(), nextNoiseTextureGuid, nextNoiseTexturePath))
            {
                nextMaterialVfx.noiseTextureGuid = nextNoiseTextureGuid;
                nextMaterialVfx.noiseTexturePath = nextNoiseTexturePath;
                materialChanged = true;
            }

            ImVec4 nextVfxColor = nextMaterialVfx.color;
            if (ImGui::ColorEdit4("VFX Color", &nextVfxColor.x))
            {
                nextMaterialVfx.color = nextVfxColor;
                materialChanged = true;
            }

            ImVec4 nextCoreColor = nextMaterialVfx.coreColor;
            if (ImGui::ColorEdit4("Core Color", &nextCoreColor.x))
            {
                nextMaterialVfx.coreColor = nextCoreColor;
                materialChanged = true;
            }

            if (nextMaterialVfx.mode == UI_MaterialVfxMode::Energy)
            {
                ImVec4 nextFlowColor = nextMaterialVfx.flowColor;
                if (ImGui::ColorEdit4("Flow Color", &nextFlowColor.x))
                {
                    nextMaterialVfx.flowColor = nextFlowColor;
                    materialChanged = true;
                }
            }

            float nextGlow = nextMaterialVfx.glow;
            if (ImGui::SliderFloat("Glow", &nextGlow, kMaterialVfxGlowMin, kMaterialVfxGlowMax, "%.2f"))
            {
                nextMaterialVfx.glow = nextGlow;
                materialChanged = true;
            }

            float nextAlpha = nextMaterialVfx.alpha;
            if (ImGui::SliderFloat("Alpha", &nextAlpha, kMaterialVfxAlphaMin, kMaterialVfxAlphaMax, "%.2f"))
            {
                nextMaterialVfx.alpha = nextAlpha;
                materialChanged = true;
            }

            float nextCoreStrength = nextMaterialVfx.coreStrength;
            if (ImGui::SliderFloat("Core Strength", &nextCoreStrength, kMaterialVfxCoreStrengthMin, kMaterialVfxCoreStrengthMax, "%.2f"))
            {
                nextMaterialVfx.coreStrength = nextCoreStrength;
                materialChanged = true;
            }

            float nextCoreWidth = nextMaterialVfx.coreWidth;
            if (ImGui::SliderFloat("Core Width", &nextCoreWidth, kMaterialVfxCoreWidthMin, kMaterialVfxCoreWidthMax, "%.3f"))
            {
                nextMaterialVfx.coreWidth = nextCoreWidth;
                materialChanged = true;
            }

            float nextCoreSoftness = nextMaterialVfx.coreSoftness;
            if (ImGui::SliderFloat("Core Softness", &nextCoreSoftness, kMaterialVfxCoreSoftnessMin, kMaterialVfxCoreSoftnessMax, "%.3f"))
            {
                nextMaterialVfx.coreSoftness = nextCoreSoftness;
                materialChanged = true;
            }

            float nextNoiseStrength = nextMaterialVfx.noiseStrength;
            if (ImGui::SliderFloat("Noise Strength", &nextNoiseStrength, kMaterialVfxNoiseStrengthMin, kMaterialVfxNoiseStrengthMax, "%.2f"))
            {
                nextMaterialVfx.noiseStrength = nextNoiseStrength;
                materialChanged = true;
            }

            ImVec2 nextUvScroll = nextMaterialVfx.uvScroll;
            const char* uvScrollLabel = nextMaterialVfx.mode == UI_MaterialVfxMode::BrushFlow ? "Brush UV Scroll" : "Noise UV Scroll";
            if (ImGui::DragFloat2(uvScrollLabel, &nextUvScroll.x, 0.01f, -2.f, 2.f, "%.3f"))
            {
                nextMaterialVfx.uvScroll = nextUvScroll;
                materialChanged = true;
            }

            float nextEdgeWidth = nextMaterialVfx.edgeWidth;
            if (ImGui::SliderFloat("Edge Width", &nextEdgeWidth, kMaterialVfxEdgeWidthMin, kMaterialVfxEdgeWidthMax, "%.3f"))
            {
                nextMaterialVfx.edgeWidth = nextEdgeWidth;
                materialChanged = true;
            }

            float nextEdgeSoftness = nextMaterialVfx.edgeSoftness;
            if (ImGui::SliderFloat("Edge Softness", &nextEdgeSoftness, kMaterialVfxEdgeSoftnessMin, kMaterialVfxEdgeSoftnessMax, "%.3f"))
            {
                nextMaterialVfx.edgeSoftness = nextEdgeSoftness;
                materialChanged = true;
            }

            float nextErosion = nextMaterialVfx.erosion;
            if (ImGui::SliderFloat("Erosion", &nextErosion, kMaterialVfxErosionMin, kMaterialVfxErosionMax, "%.2f"))
            {
                nextMaterialVfx.erosion = nextErosion;
                materialChanged = true;
            }

            float nextFlicker = nextMaterialVfx.flicker;
            if (ImGui::SliderFloat("Flicker", &nextFlicker, kMaterialVfxFlickerMin, kMaterialVfxFlickerMax, "%.2f"))
            {
                nextMaterialVfx.flicker = nextFlicker;
                materialChanged = true;
            }

            float nextDistortion = nextMaterialVfx.distortion;
            if (ImGui::SliderFloat("Distortion", &nextDistortion, kMaterialVfxDistortionMin, kMaterialVfxDistortionMax, "%.3f"))
            {
                nextMaterialVfx.distortion = nextDistortion;
                materialChanged = true;
            }

            float nextEdgeWobbleSpeed = nextMaterialVfx.edgeWobbleSpeed;
            if (ImGui::SliderFloat("Edge Wobble Speed", &nextEdgeWobbleSpeed, kMaterialVfxEdgeWobbleSpeedMin, kMaterialVfxEdgeWobbleSpeedMax, "%.2f"))
            {
                nextMaterialVfx.edgeWobbleSpeed = nextEdgeWobbleSpeed;
                materialChanged = true;
            }

            if (materialChanged)
                _editorManager.Set_UIObjectMaterialVfx(object.id, nextMaterialVfx);
        }

        if (effectiveRenderMode == UI_ImageRenderMode::ButtonSheen)
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Legacy Button Sheen");

            UI_ButtonSheenDesc nextButtonSheen = object.buttonSheen;
            bool buttonSheenChanged = false;

            ImVec4 nextSheenColor = nextButtonSheen.color;
            if (ImGui::ColorEdit4("Sheen Color", &nextSheenColor.x))
            {
                nextButtonSheen.color = nextSheenColor;
                buttonSheenChanged = true;
            }

            float nextSheenSpeed = nextButtonSheen.speed;
            if (ImGui::SliderFloat("Sheen Speed", &nextSheenSpeed, kButtonSheenSpeedMin, kButtonSheenSpeedMax, "%.2f"))
            {
                nextButtonSheen.speed = nextSheenSpeed;
                buttonSheenChanged = true;
            }

            float nextSheenWidth = nextButtonSheen.width;
            if (ImGui::SliderFloat("Sheen Width", &nextSheenWidth, kButtonSheenWidthMin, kButtonSheenWidthMax, "%.3f"))
            {
                nextButtonSheen.width = nextSheenWidth;
                buttonSheenChanged = true;
            }

            float nextSheenSoftness = nextButtonSheen.softness;
            if (ImGui::SliderFloat("Sheen Softness", &nextSheenSoftness, kButtonSheenSoftnessMin, kButtonSheenSoftnessMax, "%.3f"))
            {
                nextButtonSheen.softness = nextSheenSoftness;
                buttonSheenChanged = true;
            }

            float nextSheenAngle = nextButtonSheen.angle;
            if (ImGui::SliderFloat("Sheen Angle", &nextSheenAngle, kButtonSheenAngleMin, kButtonSheenAngleMax, "%.3f"))
            {
                nextButtonSheen.angle = nextSheenAngle;
                buttonSheenChanged = true;
            }

            float nextSheenNoiseStrength = nextButtonSheen.noiseStrength;
            if (ImGui::SliderFloat("Sheen Noise Strength", &nextSheenNoiseStrength, kButtonSheenNoiseStrengthMin, kButtonSheenNoiseStrengthMax, "%.2f"))
            {
                nextButtonSheen.noiseStrength = nextSheenNoiseStrength;
                buttonSheenChanged = true;
            }

            if (buttonSheenChanged)
                _editorManager.Set_UIObjectButtonSheen(object.id, nextButtonSheen);
        }
    }

    void UI_Inspector::Render_StateAnimationPicker(const UI_Object& object)
    {
        ImGui::Separator();
        ImGui::TextUnformatted("State Animation");

        char pathBuffer[260] = {};
        strncpy_s(pathBuffer, object.stateAnimationPath.c_str(), _TRUNCATE);
        if (ImGui::InputText("State Animation Path", pathBuffer, size(pathBuffer)))
            _editorManager.Set_UIObjectStateAnimationPath(object.id, pathBuffer);

        if (ImGui::Button(("Select##StateAnimation" + to_string(object.id)).c_str()))
        {
            wstring filePath = {};
            if (Show_StateAnimationSelectDialog(filePath))
            {
                string stateAnimationPath = {};
                if (Try_MakeStateAnimationResourcePath(filePath, stateAnimationPath))
                    _editorManager.Set_UIObjectStateAnimationPath(object.id, stateAnimationPath);
                else
                    LOG_WARN("UI inspector state animation must be under Resources/UI/UI_StateAnimations: {}", String::ToString(filePath));
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(("Clear State Animation##" + to_string(object.id)).c_str()))
            _editorManager.Set_UIObjectStateAnimationPath(object.id, "");
    }

    void UI_Inspector::Render_FontPicker(const UI_Object& object)
    {
        ImGui::Separator();

        const string popupId = "UIFontPicker##" + to_string(object.id);
        Draw_FontSlotPicker("UI Font", popupId.c_str(), object);
    }

    bool UI_Inspector::Draw_UITextureSlotPicker(const char* label, const char* popupId, string& textureGuid, string& texturePath)
    {
        bool changed = false;

        ImGui::TextUnformatted(label);

        ID3D11ShaderResourceView* previewSRV = _editorManager.Get_TexturePreviewSRV(textureGuid, texturePath);
        if (nullptr != previewSRV)
        {
            ImGui::Image(To_ImTextureID(previewSRV), ImVec2(64.f, 64.f));
        }
        else
        {
            const ImVec2 previewMin = ImGui::GetCursorScreenPos();
            const ImVec2 previewMax = ImVec2(previewMin.x + 64.f, previewMin.y + 64.f);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(previewMin, previewMax, IM_COL32(36, 39, 46, 255));
            drawList->AddRect(previewMin, previewMax, IM_COL32(88, 96, 108, 255));
            drawList->AddText(ImVec2(previewMin.x + 13.f, previewMin.y + 24.f), IM_COL32(170, 176, 186, 255), "Empty");
            ImGui::Dummy(ImVec2(64.f, 64.f));
        }

        ImGui::SameLine();
        ImGui::BeginGroup();

        if (textureGuid.empty())
        {
            if (texturePath.empty())
            {
                ImGui::TextDisabled("No texture selected");
            }
            else
            {
                ImGui::TextDisabled("Texture path is set without GUID");
                ImGui::TextWrapped("%s", texturePath.c_str());
            }
        }
        else
        {
            const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
            if (nullptr != assetMeta)
            {
                ImGui::Text("%s", Get_AssetFileName(*assetMeta).c_str());
                ImGui::TextWrapped("%s", Make_ClientRuntimeResourcePath(*assetMeta).c_str());
            }
            else
            {
                ImGui::TextDisabled("Missing texture asset");
                if (!texturePath.empty())
                    ImGui::TextWrapped("%s", texturePath.c_str());
            }
        }

        if (ImGui::Button(("Select##Select" + string(popupId)).c_str()))
            ImGui::OpenPopup(popupId);

        ImGui::SameLine();
        if (ImGui::Button(("Clear##Clear" + string(popupId)).c_str()))
        {
            textureGuid.clear();
            texturePath.clear();
            changed = true;
        }

        fs::path missingMetaPath = {};
        if (textureGuid.empty() && Try_GetMissingMetaPathForTexture(texturePath, missingMetaPath))
        {
            if (!_editorManager.Is_AssetManagerReady())
            {
                ImGui::TextDisabled("Texture meta is missing. Asset registry is not ready.");
            }
            else if (ImGui::Button(("Generate Texture Meta##Meta" + string(popupId)).c_str()))
            {
                const wstring resolvedTexturePath = Resolve_ClientRuntimeResourcePath(texturePath);
                const string generatedGuid = GAME->Ensure_AssetGUID(resolvedTexturePath, "Texture");
                if (!generatedGuid.empty())
                {
                    textureGuid = generatedGuid;
                    _uiTextureAssetCacheDirty = true;
                    _filteredUITextureAssetsDirty = true;
                    changed = true;
                }
                else
                {
                    LOG_WARN("UI texture meta generation failed: {}", String::ToString(resolvedTexturePath));
                }
            }
        }

        ImGui::EndGroup();

        if (ImGui::BeginPopup(popupId))
        {
            if (!_editorManager.Is_AssetManagerReady())
                ImGui::TextDisabled("Asset registry is not ready.");

            ImGui::SetNextItemWidth(360.f);
            ImGui::InputText(("Search##" + string(popupId)).c_str(), _textureSearchBuffer, size(_textureSearchBuffer));
            ImGui::Separator();

            if (_uiTextureAssetCacheDirty)
                Refresh_UITextureAssetCache();

            const string searchText = String::ToLowerCopy(_textureSearchBuffer);
            Refresh_FilteredUITextureAssets(searchText);

            if (_filteredUITextureAssets.empty())
            {
                ImGui::TextDisabled("No UI textures found under ../Bin/Resources/Textures/UI.");
            }
            else
            {
                const float listHeight = 360.f;
                if (ImGui::BeginChild(("TextureList##" + string(popupId)).c_str(), ImVec2(520.f, listHeight), true))
                {
                    auto draw_texture_row = [&](const AssetMeta& assetMeta)
                    {
                        const string rowId = assetMeta.guid.empty() ? Make_ClientRuntimeResourcePath(assetMeta) : assetMeta.guid;
                        ImGui::PushID(rowId.c_str());

                        const string assetTexturePath = Make_ClientRuntimeResourcePath(assetMeta);
                        ID3D11ShaderResourceView* rowSRV = _editorManager.Get_TexturePreviewSRV(assetMeta.guid, assetTexturePath);
                        if (nullptr != rowSRV)
                        {
                            ImGui::Image(To_ImTextureID(rowSRV), ImVec2(48.f, 48.f));
                        }
                        else
                        {
                            const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
                            const ImVec2 thumbMax = ImVec2(thumbMin.x + 48.f, thumbMin.y + 48.f);
                            ImDrawList* drawList = ImGui::GetWindowDrawList();

                            drawList->AddRectFilled(thumbMin, thumbMax, IM_COL32(36, 39, 46, 255));
                            drawList->AddRect(thumbMin, thumbMax, IM_COL32(88, 96, 108, 255));
                            ImGui::Dummy(ImVec2(48.f, 48.f));
                        }

                        ImGui::SameLine();
                        ImGui::BeginGroup();

                        const bool isSelected =
                            (!assetMeta.guid.empty() && textureGuid == assetMeta.guid) ||
                            (assetMeta.guid.empty() && texturePath == assetTexturePath);
                        if (ImGui::Selectable(Get_AssetFileName(assetMeta).c_str(), isSelected))
                        {
                            textureGuid = assetMeta.guid;
                            texturePath = Make_ClientRuntimeResourcePath(assetMeta);
                            changed = true;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::TextDisabled("%s", assetTexturePath.c_str());
                        ImGui::EndGroup();
                        ImGui::Separator();
                        ImGui::PopID();
                    };

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int32>(_filteredUITextureAssets.size()));
                    while (clipper.Step())
                    {
                        for (int32 rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex)
                        {
                            if (nullptr != _filteredUITextureAssets[rowIndex])
                                draw_texture_row(*_filteredUITextureAssets[rowIndex]);
                        }
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndPopup();
        }

        return changed;
    }

    void UI_Inspector::Refresh_UITextureAssetCache()
    {
        _uiTextureAssets.clear();
        _filteredUITextureAssets.clear();
        _filteredUITextureAssetsDirty = true;

        if (_editorManager.Is_AssetManagerReady())
        {
            for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
            {
                if (Is_UITextureAsset(assetMeta))
                    _uiTextureAssets.push_back(assetMeta);
            }
        }

        if (_uiTextureAssets.empty())
        {
            const wstring resourceRoot = Resolve_UIEditorClientResourceRoot();
            const fs::path uiTextureRoot = fs::path(resourceRoot) / L"Textures" / L"UI";

            try
            {
                if (!resourceRoot.empty() && fs::exists(uiTextureRoot) && fs::is_directory(uiTextureRoot))
                {
                    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(uiTextureRoot))
                    {
                        if (!entry.is_regular_file() || !Is_UITextureFileExtension(entry.path()))
                            continue;

                        AssetMeta textureAsset = {};
                        textureAsset.type = "Texture";
                        textureAsset.fullPath = fs::absolute(entry.path()).lexically_normal().wstring();
                        textureAsset.relativePath = fs::relative(entry.path(), fs::path(resourceRoot)).lexically_normal().wstring();

                        _uiTextureAssets.push_back(textureAsset);
                    }
                }
            }
            catch (...)
            {
            }
        }

        _uiTextureAssetCacheDirty = false;
    }

    void UI_Inspector::Refresh_FilteredUITextureAssets(const string& searchText)
    {
        if (!_filteredUITextureAssetsDirty && _lastTextureSearchText == searchText)
            return;

        _filteredUITextureAssets.clear();
        _filteredUITextureAssets.reserve(_uiTextureAssets.size());

        for (const AssetMeta& assetMeta : _uiTextureAssets)
        {
            if (Matches_Search(assetMeta, searchText))
                _filteredUITextureAssets.push_back(&assetMeta);
        }

        _lastTextureSearchText = searchText;
        _filteredUITextureAssetsDirty = false;
    }

    void UI_Inspector::Refresh_UIFontAssetCache()
    {
        _uiFontAssets = Collect_UISpriteFontAssets("");
        _filteredUIFontAssets.clear();
        _lastFontSearchText.clear();
        _uiFontAssetCacheDirty = false;
        _filteredUIFontAssetsDirty = true;
    }

    void UI_Inspector::Refresh_FilteredUIFontAssets(const string& searchText)
    {
        if (!_filteredUIFontAssetsDirty && _lastFontSearchText == searchText)
            return;

        _filteredUIFontAssets.clear();
        _filteredUIFontAssets.reserve(_uiFontAssets.size());

        for (const AssetMeta& assetMeta : _uiFontAssets)
        {
            if (Matches_Search(assetMeta, searchText))
                _filteredUIFontAssets.push_back(&assetMeta);
        }

        _lastFontSearchText = searchText;
        _filteredUIFontAssetsDirty = false;
    }

    void UI_Inspector::Draw_FontSlotPicker(const char* label, const char* popupId, const UI_Object& object)
    {
        ImGui::TextUnformatted(label);

        const AssetMeta* selectedAsset = nullptr;
        if (_editorManager.Is_AssetManagerReady() && !object.fontGuid.empty())
            selectedAsset = GAME->Find_AssetByGUID(object.fontGuid);

        if (nullptr != selectedAsset)
        {
            ImGui::Text("%s", Get_AssetFileName(*selectedAsset).c_str());
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.35f, 1.f), "Applied: %s", object.fontTag.c_str());
            ImGui::TextWrapped("%s", Make_ClientRuntimeResourcePath(*selectedAsset).c_str());
        }
        else if (!object.fontPath.empty())
        {
            ImGui::TextDisabled("Font path is set without GUID");
            if (!object.fontTag.empty())
                ImGui::TextColored(ImVec4(1.f, 0.85f, 0.35f, 1.f), "Applied: %s", object.fontTag.c_str());
            ImGui::TextWrapped("%s", object.fontPath.c_str());
        }
        else if (!object.fontTag.empty())
        {
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.35f, 1.f), "Applied: %s", object.fontTag.c_str());
            ImGui::TextDisabled("No font asset selected");
        }
        else
        {
            ImGui::TextDisabled("No font selected");
        }

        if (ImGui::Button(("Select##Select" + string(popupId)).c_str()))
            ImGui::OpenPopup(popupId);

        ImGui::SameLine();
        if (ImGui::Button(("Clear##Clear" + string(popupId)).c_str()))
            _editorManager.Set_UIObjectFontBinding(object.id, "", "", "");

        if (ImGui::BeginPopup(popupId))
        {
            if (!_editorManager.Is_AssetManagerReady())
                ImGui::TextDisabled("Asset registry is not ready.");

            ImGui::SetNextItemWidth(360.f);
            ImGui::InputText(("Search##" + string(popupId)).c_str(), _fontSearchBuffer, size(_fontSearchBuffer));
            ImGui::Separator();

            if (_uiFontAssetCacheDirty)
                Refresh_UIFontAssetCache();

            const string searchText = String::ToLowerCopy(_fontSearchBuffer);
            Refresh_FilteredUIFontAssets(searchText);

            if (_filteredUIFontAssets.empty())
            {
                ImGui::TextDisabled("No UI fonts found under ../Bin/Resources/Fonts.");
            }
            else
            {
                const float listHeight = 320.f;
                if (ImGui::BeginChild(("FontList##" + string(popupId)).c_str(), ImVec2(520.f, listHeight), true))
                {
                    auto draw_font_row = [&](const AssetMeta& assetMeta)
                    {
                        const string stableId = Get_FontAssetStableId(assetMeta);
                        ImGui::PushID(stableId.c_str());

                        const bool isSelected = Is_FontAssetApplied(object, assetMeta);
                        if (ImGui::Selectable(Get_AssetFileName(assetMeta).c_str(), isSelected))
                        {
                            _editorManager.Set_UIObjectFontBinding(
                                object.id,
                                assetMeta.guid,
                                Make_ClientRuntimeResourcePath(assetMeta),
                                Make_FontTag(assetMeta));
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::TextDisabled("%s", Make_ClientRuntimeResourcePath(assetMeta).c_str());
                        ImGui::TextDisabled("Tag: %s", Make_FontTag(assetMeta).c_str());
                        ImGui::Separator();
                        ImGui::PopID();
                    };

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int32>(_filteredUIFontAssets.size()));
                    while (clipper.Step())
                    {
                        for (int32 rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex)
                        {
                            if (nullptr != _filteredUIFontAssets[rowIndex])
                                draw_font_row(*_filteredUIFontAssets[rowIndex]);
                        }
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndPopup();
        }
    }
}
