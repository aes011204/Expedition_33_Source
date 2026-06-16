#include "pch.h"
#include "UI_Vfx_View.h"

#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"

#include <commdlg.h>

namespace UIEditor
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr wchar_t kDefaultVfxFileName[] = L"NewUISpriteVfx.uivfx.json";

        fs::path Get_ModuleDirectory()
        {
            wchar_t modulePath[MAX_PATH] = {};
            if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
                return {};

            return fs::path(modulePath).parent_path();
        }

        bool Try_CreateDirectory(const fs::path& directory)
        {
            if (directory.empty())
                return false;

            try
            {
                if (fs::exists(directory))
                    return fs::is_directory(directory);

                return fs::create_directories(directory);
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI VFX directory create failed: {} ({})", exception.what(), String::ToString(directory.wstring()));
                return false;
            }
        }

        bool Try_FindResourceRootFromStart(const fs::path& startPath, fs::path& outResourceRoot)
        {
            if (startPath.empty())
                return false;

            try
            {
                fs::path start = fs::absolute(startPath).lexically_normal();
                for (fs::path currentPath = start; !currentPath.empty(); currentPath = currentPath.parent_path())
                {
                    const fs::path candidate = (currentPath / L"Client" / L"Bin" / L"Resources").lexically_normal();
                    if (fs::exists(candidate) && fs::is_directory(candidate))
                    {
                        outResourceRoot = candidate;
                        return true;
                    }

                    if (currentPath == currentPath.parent_path())
                        break;
                }
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI VFX resource root search failed: {}", exception.what());
            }

            return false;
        }

        void Draw_RotatedParticle(
            ImDrawList* drawList,
            ImTextureID textureId,
            const ImVec2& center,
            const ImVec2& size,
            float rotationRadians,
            ImU32 color)
        {
            const float cosAngle = cosf(rotationRadians);
            const float sinAngle = sinf(rotationRadians);
            const ImVec2 halfSize(size.x * 0.5f, size.y * 0.5f);
            const ImVec2 corners[4] = {
                ImVec2(-halfSize.x, -halfSize.y),
                ImVec2(halfSize.x, -halfSize.y),
                ImVec2(halfSize.x, halfSize.y),
                ImVec2(-halfSize.x, halfSize.y),
            };

            ImVec2 points[4] = {};
            for (int32 index = 0; index < 4; ++index)
            {
                points[index] = ImVec2(
                    center.x + corners[index].x * cosAngle - corners[index].y * sinAngle,
                    center.y + corners[index].x * sinAngle + corners[index].y * cosAngle);
            }

            if (textureId != ImTextureID{})
            {
                drawList->AddImageQuad(
                    textureId,
                    points[0],
                    points[1],
                    points[2],
                    points[3],
                    ImVec2(0.f, 0.f),
                    ImVec2(1.f, 0.f),
                    ImVec2(1.f, 1.f),
                    ImVec2(0.f, 1.f),
                    color);
            }
            else
            {
                drawList->AddQuadFilled(points[0], points[1], points[2], points[3], color);
            }
        }
    }

    UI_Vfx_View::UI_Vfx_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"UI VFX" }
        , _editorManager{ editorManager }
    {
    }

    void UI_Vfx_View::Render()
    {
        if (!Is_Open())
            return;

        bool isOpen = Is_Open();
        if (ImGui::Begin("UI VFX", &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

            Render_Toolbar();
            ImGui::Separator();

            if (ImGui::BeginChild("##UIVfxLeft", ImVec2(380.f, 0.f), true))
            {
                Render_AssetControls();
                Render_TextureControls();
                Render_SpawnControls();
                Render_MotionControls();
                Render_AppearanceControls();
            }
            ImGui::EndChild();

            ImGui::SameLine();
            Render_Preview();
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    void UI_Vfx_View::Render_Toolbar()
    {
        if (ImGui::Button("New"))
        {
            _asset = Client::UISpriteVfxAsset{};
            _filePath.clear();
            _previewTime = 0.f;
            _isDirty = false;
            _isPlaying = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Open"))
            Load_Asset();

        ImGui::SameLine();
        if (ImGui::Button("Save"))
            Save_Asset(false);

        ImGui::SameLine();
        if (ImGui::Button("Save As"))
            Save_Asset(true);

        ImGui::SameLine();
        if (ImGui::Button(_isPlaying ? "Pause" : "Play"))
            _isPlaying = !_isPlaying;

        ImGui::SameLine();
        if (ImGui::Button("Restart"))
        {
            _previewTime = 0.f;
            _isPlaying = true;
        }

        ImGui::SameLine();
        const UI_Object* selectedObject = _editorManager.Get_SelectedObject();
        const bool canApplyToSelected =
            selectedObject != nullptr &&
            selectedObject->type == UI_ObjectType::UIVfx &&
            !_filePath.empty();
        ImGui::BeginDisabled(!canApplyToSelected);
        if (ImGui::Button("Apply To Selected"))
            _editorManager.Set_UIObjectVfxPath(selectedObject->id, Make_RuntimePath(_filePath));
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("%s", _isDirty ? "Dirty" : "Saved");
    }

    void UI_Vfx_View::Render_AssetControls()
    {
        if (ImGui::CollapsingHeader("Asset", ImGuiTreeNodeFlags_DefaultOpen))
        {
            strncpy_s(_nameBuffer, _asset.name.c_str(), _TRUNCATE);
            if (ImGui::InputText("Name", _nameBuffer, size(_nameBuffer)))
            {
                _asset.name = _nameBuffer;
                _isDirty = true;
            }

            ImGui::TextWrapped("Path: %s", _filePath.empty() ? "(unsaved)" : String::ToString(_filePath).c_str());
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::SliderFloat("Preview Time", &_previewTime, 0.f, max(0.001f, _asset.duration), "%.3f"))
                _isPlaying = false;
        }
    }

    void UI_Vfx_View::Render_TextureControls()
    {
        if (!ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        strncpy_s(_textureGuidBuffer, _asset.textureGuid.c_str(), _TRUNCATE);
        if (ImGui::InputText("Texture Guid", _textureGuidBuffer, size(_textureGuidBuffer)))
        {
            _asset.textureGuid = _textureGuidBuffer;
            _isDirty = true;
        }

        strncpy_s(_texturePathBuffer, _asset.texturePath.c_str(), _TRUNCATE);
        if (ImGui::InputText("Texture Path", _texturePathBuffer, size(_texturePathBuffer)))
        {
            _asset.texturePath = _texturePathBuffer;
            _isDirty = true;
        }

        strncpy_s(_maskTextureGuidBuffer, _asset.maskTextureGuid.c_str(), _TRUNCATE);
        if (ImGui::InputText("Mask Guid", _maskTextureGuidBuffer, size(_maskTextureGuidBuffer)))
        {
            _asset.maskTextureGuid = _maskTextureGuidBuffer;
            _isDirty = true;
        }

        strncpy_s(_maskTexturePathBuffer, _asset.maskTexturePath.c_str(), _TRUNCATE);
        if (ImGui::InputText("Mask Path", _maskTexturePathBuffer, size(_maskTexturePathBuffer)))
        {
            _asset.maskTexturePath = _maskTexturePathBuffer;
            _isDirty = true;
        }

        strncpy_s(_noiseTextureGuidBuffer, _asset.noiseTextureGuid.c_str(), _TRUNCATE);
        if (ImGui::InputText("Noise Guid", _noiseTextureGuidBuffer, size(_noiseTextureGuidBuffer)))
        {
            _asset.noiseTextureGuid = _noiseTextureGuidBuffer;
            _isDirty = true;
        }

        strncpy_s(_noiseTexturePathBuffer, _asset.noiseTexturePath.c_str(), _TRUNCATE);
        if (ImGui::InputText("Noise Path", _noiseTexturePathBuffer, size(_noiseTexturePathBuffer)))
        {
            _asset.noiseTexturePath = _noiseTexturePathBuffer;
            _isDirty = true;
        }
    }

    void UI_Vfx_View::Render_SpawnControls()
    {
        if (!ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        int32 maxParticles = static_cast<int32>(_asset.maxParticles);
        if (ImGui::DragInt("Max Particles", &maxParticles, 1.f, 1, 4096))
        {
            _asset.maxParticles = static_cast<uint32>(max(1, maxParticles));
            _asset.burstCount = min(_asset.burstCount, _asset.maxParticles);
            _isDirty = true;
        }

        if (ImGui::DragFloat("Duration", &_asset.duration, 0.01f, 0.001f, 30.f, "%.3f"))
        {
            _asset.duration = max(0.001f, _asset.duration);
            _previewTime = min(_previewTime, _asset.duration);
            _isDirty = true;
        }

        if (ImGui::Checkbox("Loop", &_asset.loop))
            _isDirty = true;

        int32 burstCount = static_cast<int32>(_asset.burstCount);
        if (ImGui::DragInt("Burst Count", &burstCount, 1.f, 0, static_cast<int32>(_asset.maxParticles)))
        {
            _asset.burstCount = static_cast<uint32>(min(max(0, burstCount), static_cast<int32>(_asset.maxParticles)));
            _isDirty = true;
        }

        if (ImGui::DragFloat("Spawn Rate", &_asset.spawnRate, 1.f, 0.f, 2000.f, "%.1f"))
        {
            _asset.spawnRate = max(0.f, _asset.spawnRate);
            _isDirty = true;
        }

        if (ImGui::DragFloat2("Lifetime", &_asset.lifetime.x, 0.01f, 0.001f, 30.f, "%.3f"))
        {
            _asset.lifetime.x = max(0.001f, _asset.lifetime.x);
            _asset.lifetime.y = max(_asset.lifetime.x, _asset.lifetime.y);
            _isDirty = true;
        }

        if (ImGui::DragFloat2("Spawn Range", &_asset.spawnRange.x, 1.f, 0.f, 4000.f, "%.1f"))
        {
            _asset.spawnRange.x = max(0.f, _asset.spawnRange.x);
            _asset.spawnRange.y = max(0.f, _asset.spawnRange.y);
            _isDirty = true;
        }
    }

    void UI_Vfx_View::Render_MotionControls()
    {
        if (!ImGui::CollapsingHeader("Motion", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        if (ImGui::DragFloat2("Velocity Min", &_asset.velocityMin.x, 1.f, -4000.f, 4000.f, "%.1f"))
            _isDirty = true;
        if (ImGui::DragFloat2("Velocity Max", &_asset.velocityMax.x, 1.f, -4000.f, 4000.f, "%.1f"))
            _isDirty = true;
        if (ImGui::DragFloat("Radial Velocity Min", &_asset.radialVelocityMin, 1.f, -4000.f, 4000.f, "%.1f"))
            _isDirty = true;
        if (ImGui::DragFloat("Radial Velocity Max", &_asset.radialVelocityMax, 1.f, -4000.f, 4000.f, "%.1f"))
            _isDirty = true;
    }

    void UI_Vfx_View::Render_AppearanceControls()
    {
        if (!ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        if (ImGui::DragFloat2("Size Start", &_asset.sizeStart.x, 1.f, 0.01f, 2000.f, "%.1f"))
        {
            _asset.sizeStart.x = max(0.01f, _asset.sizeStart.x);
            _asset.sizeStart.y = max(0.01f, _asset.sizeStart.y);
            _isDirty = true;
        }
        if (ImGui::DragFloat2("Size End", &_asset.sizeEnd.x, 1.f, 0.01f, 2000.f, "%.1f"))
        {
            _asset.sizeEnd.x = max(0.01f, _asset.sizeEnd.x);
            _asset.sizeEnd.y = max(0.01f, _asset.sizeEnd.y);
            _isDirty = true;
        }

        if (ImGui::DragFloat("Rotation Min", &_asset.rotationDegreesMin, 1.f, -360.f, 360.f, "%.1f"))
            _isDirty = true;
        if (ImGui::DragFloat("Rotation Max", &_asset.rotationDegreesMax, 1.f, -360.f, 360.f, "%.1f"))
            _isDirty = true;
        if (ImGui::DragFloat("Rotation Speed Min", &_asset.rotationSpeedDegreesMin, 1.f, -2000.f, 2000.f, "%.1f"))
            _isDirty = true;
        if (ImGui::DragFloat("Rotation Speed Max", &_asset.rotationSpeedDegreesMax, 1.f, -2000.f, 2000.f, "%.1f"))
            _isDirty = true;

        if (ImGui::ColorEdit4("Color Start", &_asset.colorStart.x))
            _isDirty = true;
        if (ImGui::ColorEdit4("Color End", &_asset.colorEnd.x))
            _isDirty = true;

        if (ImGui::DragFloat2("UV Scroll", &_asset.uvScrollSpeed.x, 0.01f, -20.f, 20.f, "%.3f"))
            _isDirty = true;
        if (ImGui::SliderFloat("Noise Strength", &_asset.noiseStrength, 0.f, 4.f, "%.3f"))
            _isDirty = true;
        if (ImGui::SliderFloat("Alpha Erosion", &_asset.alphaErosion, 0.f, 1.f, "%.3f"))
            _isDirty = true;

        const char* blendItems[] = { "AlphaBlend", "Additive" };
        int32 blendIndex = static_cast<int32>(_asset.blendMode);
        blendIndex = min(max(blendIndex, 0), 1);
        if (ImGui::Combo("Blend", &blendIndex, blendItems, 2))
        {
            _asset.blendMode = static_cast<Client::UISpriteVfxBlendMode>(blendIndex);
            _isDirty = true;
        }

        if (ImGui::DragInt("Sort Order", &_asset.sortOrder, 1.f))
            _isDirty = true;
    }

    void UI_Vfx_View::Render_Preview()
    {
        if (_isPlaying)
        {
            _previewTime += ImGui::GetIO().DeltaTime;
            if (_asset.loop)
                _previewTime = fmodf(_previewTime, max(0.001f, _asset.duration));
            else
                _previewTime = min(_previewTime, max(0.001f, _asset.duration));
        }

        ImGui::BeginChild("##UIVfxPreview", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 previewMin = ImGui::GetCursorScreenPos();
        const ImVec2 previewSize = ImGui::GetContentRegionAvail();
        const ImVec2 previewMax(previewMin.x + previewSize.x, previewMin.y + previewSize.y);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(previewMin, previewMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(previewMin, previewMax, IM_COL32(90, 96, 108, 255));

        const ImVec2 center(previewMin.x + previewSize.x * 0.5f, previewMin.y + previewSize.y * 0.5f);
        drawList->AddLine(ImVec2(center.x - 8.f, center.y), ImVec2(center.x + 8.f, center.y), IM_COL32(255, 220, 120, 180), 1.f);
        drawList->AddLine(ImVec2(center.x, center.y - 8.f), ImVec2(center.x, center.y + 8.f), IM_COL32(255, 220, 120, 180), 1.f);

        ID3D11ShaderResourceView* textureSRV = _editorManager.Get_TexturePreviewSRV(_asset.textureGuid, _asset.texturePath);
        ImTextureID textureId = textureSRV != nullptr ? reinterpret_cast<ImTextureID>(textureSRV) : ImTextureID{};

        const vector<Client::UISpriteVfxParticlePreview> particles =
            Client::UISpriteVfxPreviewSampler::Sample(
                _asset,
                _previewTime,
                Vec2(center.x, center.y),
                false);

        drawList->PushClipRect(previewMin, previewMax, true);
        for (const Client::UISpriteVfxParticlePreview& particle : particles)
        {
            const ImVec2 particleCenter(particle.position.x, particle.position.y);
            const ImVec2 particleSize(particle.size.x, particle.size.y);
            const ImVec4 color(
                min(max(particle.color.x, 0.f), 1.f),
                min(max(particle.color.y, 0.f), 1.f),
                min(max(particle.color.z, 0.f), 1.f),
                min(max(particle.color.w, 0.f), 1.f));
            Draw_RotatedParticle(
                drawList,
                textureId,
                particleCenter,
                particleSize,
                particle.rotationRadians,
                ImGui::ColorConvertFloat4ToU32(color));
        }
        drawList->PopClipRect();

        ImGui::Dummy(previewSize);
        ImGui::EndChild();
    }

    bool UI_Vfx_View::Save_Asset(bool forceSaveAs)
    {
        wstring path = _filePath;
        if (path.empty() || forceSaveAs)
        {
            if (!Show_SaveDialog(path))
                return false;
        }

        Ensure_UivfxExtension(path);
        if (!Client::UISpriteVfxSerializer::Save_ToFile(_asset, path))
            return false;

        _filePath = path;
        _isDirty = false;
        return true;
    }

    bool UI_Vfx_View::Load_Asset()
    {
        wstring path{};
        if (!Show_LoadDialog(path))
            return false;

        Client::UISpriteVfxAsset loadedAsset{};
        if (!Client::UISpriteVfxSerializer::Load_FromFile(path, loadedAsset))
            return false;

        _asset = move(loadedAsset);
        _filePath = path;
        _previewTime = 0.f;
        _isPlaying = true;
        _isDirty = false;
        return true;
    }

    bool UI_Vfx_View::Show_SaveDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        if (!outFilePath.empty())
            wcsncpy_s(filePathBuffer, outFilePath.c_str(), _TRUNCATE);
        else
            wcsncpy_s(filePathBuffer, kDefaultVfxFileName, _TRUNCATE);

        const wstring initialDirectory = Resolve_DefaultVfxDirectory();
        OPENFILENAMEW openFileName{};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI VFX Files\0*.uivfx.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"json";
        openFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (FALSE == GetSaveFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        Ensure_UivfxExtension(outFilePath);
        return !outFilePath.empty();
    }

    bool UI_Vfx_View::Show_LoadDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        const wstring initialDirectory = Resolve_DefaultVfxDirectory();
        OPENFILENAMEW openFileName{};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI VFX Files\0*.uivfx.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"json";
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (FALSE == GetOpenFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        return !outFilePath.empty();
    }

    wstring UI_Vfx_View::Resolve_DefaultVfxDirectory() const
    {
        vector<fs::path> startPaths{};
        const fs::path moduleDirectory = Get_ModuleDirectory();
        if (!moduleDirectory.empty())
            startPaths.push_back(moduleDirectory);

        try
        {
            startPaths.push_back(fs::current_path());
        }
        catch (...)
        {
        }

        for (const fs::path& startPath : startPaths)
        {
            fs::path resourceRoot{};
            if (Try_FindResourceRootFromStart(startPath, resourceRoot))
            {
                const fs::path vfxDirectory = (resourceRoot / L"UI" / L"UI_VFX").lexically_normal();
                if (Try_CreateDirectory(vfxDirectory))
                    return vfxDirectory.wstring();
            }
        }

        return {};
    }

    void UI_Vfx_View::Ensure_UivfxExtension(wstring& filePath) const
    {
        if (filePath.empty())
            return;

        fs::path path{ filePath };
        const wstring extension = path.extension().wstring();
        if (extension == L".json")
        {
            const wstring fileName = path.filename().wstring();
            if (fileName.ends_with(L".uivfx.json"))
                return;
        }

        if (!path.filename().wstring().ends_with(L".uivfx.json"))
            path.replace_extension(L".uivfx.json");

        filePath = path.wstring();
    }

    string UI_Vfx_View::Make_RuntimePath(const wstring& filePath) const
    {
        if (filePath.empty())
            return {};

        try
        {
            const fs::path absoluteFilePath = fs::absolute(fs::path(filePath)).lexically_normal();
            const wstring resourceRoot = Resolve_UIEditorClientResourceRoot();
            if (!resourceRoot.empty())
            {
                const fs::path absoluteResourceRoot = fs::absolute(fs::path(resourceRoot)).lexically_normal();
                const fs::path relativePath = fs::relative(absoluteFilePath, absoluteResourceRoot);
                if (!relativePath.empty() && relativePath.native().find(L"..") != 0)
                {
                    string runtimePath = "../Bin/Resources/" + String::ToString(relativePath.generic_wstring());
                    replace(runtimePath.begin(), runtimePath.end(), '\\', '/');
                    return runtimePath;
                }
            }
        }
        catch (...)
        {
        }

        return String::ToString(fs::path(filePath).lexically_normal().wstring());
    }

    Shared<UI_Vfx_View> UI_Vfx_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Vfx_View>(editorManager);
    }
}
