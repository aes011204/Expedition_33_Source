#include "pch.h"
#include "PanelModelOverride.h"
#include "SceneContext.h"
#include "MaterialBuilder.h"
#include "Asset_Manager.h"
#include "MEStaticMeshActor.h"
#include "MEInstancedMeshActor.h"
#include "PanelViewport.h"
#include "Mesh.h"

PanelModelOverride::PanelModelOverride(SceneContext* ctx, Shared<PanelViewport> viewport)
    : MEWindow{ L"Model Override" }, _ctx{ ctx }, _viewport{ viewport }
{
}

namespace
{
    struct SelectedModelContext
    {
        Shared<EditorObject> object = {};
        Shared<ModelCom> model = {};
        string modelPath = {};
        string filename = {};
        ShaderType shaderType = ShaderType::VtxMesh;
        MapEditor::EMapMaterialObjectKind materialObjectKind = MapEditor::EMapMaterialObjectKind::Unsupported;
    };

    string ToLowerAscii(string value)
    {
        transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(tolower(c));
            });
        return value;
    }

    string ToRelativePath(const string& path)
    {
        if (path.empty())
        {
            return path;
        }

        error_code ec;
        const fs::path abs = fs::weakly_canonical(fs::path(path), ec);
        const fs::path cwd = fs::weakly_canonical(fs::current_path(), ec);
        if (!ec && !cwd.empty())
        {
            const fs::path rel = abs.lexically_relative(cwd);
            if (!rel.empty())
            {
                return rel.generic_string();
            }
        }

        return path;
    }

    string GetMapName(const MapEditor::SceneContext* ctx)
    {
        if (!ctx || ctx->_mapName.empty())
        {
            return "Untitled";
        }
        return String::ToString(ctx->_mapName);
    }

    string BuildBaseBundleKey(const string& modelPath)
    {
        fs::path path(modelPath);
        string bundleKey = path.parent_path().filename().string();
        if (bundleKey.empty())
        {
            bundleKey = path.stem().string();
        }

        return ToLowerAscii(bundleKey);
    }

    fs::path BuildMaterialJsonPath(const string& mapName, const string& modelPath)
    {
        string stem = fs::path(modelPath).stem().string();
        return fs::path(MapEditor::_materialsPath) / mapName / (stem + ".material.json");
    }

    bool ResolveSelectedModel(MapEditor::SceneContext* ctx, SelectedModelContext& out)
    {
        if (!ctx || !ctx->_selected)
        {
            return false;
        }

        out.object = ctx->_selected;
        out.materialObjectKind = MapEditor::MapMaterialObjectKindFromObjectType(out.object->_type);

        if (!out.object->_modelPath.empty())
        {
            out.modelPath = String::ToString(out.object->_modelPath);
            out.filename = fs::path(out.modelPath).filename().string();
            out.shaderType = ShaderType::VtxMesh;
            out.model = out.object->_renderHandle ? out.object->_renderHandle->GetModel() : nullptr;
            return !out.filename.empty();
        }

        if (!out.object->_instancedModelPath.empty())
        {
            out.modelPath = String::ToString(out.object->_instancedModelPath);
            out.filename = fs::path(out.modelPath).filename().string();
            out.shaderType = ShaderType::VtxInstanceMeshExact;
            out.model = out.object->_instancedChunkHandles.empty() ? nullptr : out.object->_instancedChunkHandles[0]->GetModel();
            return !out.filename.empty();
        }

        return false;
    }

    const char* TextureBindingKey(MaterialTextureSlot slot)
    {
        switch (slot)
        {
        case MaterialTextureSlot::BaseColor:
            return "base_color";
        case MaterialTextureSlot::Normal:
            return "normal";
        case MaterialTextureSlot::ORM:
            return "orm";
        case MaterialTextureSlot::ORM2:
            return "orm2";
        case MaterialTextureSlot::ORM3:
            return "orm3";
        case MaterialTextureSlot::Emissive:
            return "emissive";
        case MaterialTextureSlot::OpacityMask:
            return "opacity_mask";
        case MaterialTextureSlot::HairDepth:
            return "hair_depth";
        case MaterialTextureSlot::HairID:
            return "hair_id";
        case MaterialTextureSlot::IrisHeight:
            return "iris_height";
        case MaterialTextureSlot::IrisMask:
            return "iris_mask";
        default:
            return "";
        }
    }

    const char* LegacyTextureKey(MaterialTextureSlot slot)
    {
        switch (slot)
        {
        case MaterialTextureSlot::BaseColor:
            return "baseColor";
        case MaterialTextureSlot::Normal:
            return "normal";
        case MaterialTextureSlot::ORM:
            return "orm";
        case MaterialTextureSlot::ORM2:
            return "orm2";
        case MaterialTextureSlot::ORM3:
            return "orm3";
        case MaterialTextureSlot::Emissive:
            return "emissive";
        case MaterialTextureSlot::OpacityMask:
            return "opacityMask";
        case MaterialTextureSlot::HairDepth:
            return "hairDepth";
        case MaterialTextureSlot::HairID:
            return "hairID";
        case MaterialTextureSlot::IrisHeight:
            return "irisHeight";
        case MaterialTextureSlot::IrisMask:
            return "irisMask";
        default:
            return "";
        }
    }

    constexpr const char* kMaterialChannelSourceLabels[] =
    {
        "Const0",
        "Const1",
        "R",
        "G",
        "B",
        "A",
        "MaxRGB",
        "Luma",
    };
    constexpr int32 kMaterialChannelSourceLabelCount =
        static_cast<int32>(sizeof(kMaterialChannelSourceLabels) / sizeof(kMaterialChannelSourceLabels[0]));

    int32 ClampMaterialChannelSourceIndex(uint32 value)
    {
        return std::clamp(static_cast<int32>(value), 0, kMaterialChannelSourceLabelCount - 1);
    }

    bool InputTextString(const char* label, string& value, float width = 240.f)
    {
        char buffer[MAX_PATH] = {};
        strncpy_s(buffer, value.c_str(), sizeof(buffer) - 1);
        ImGui::SetNextItemWidth(width);
        if (!ImGui::InputText(label, buffer, sizeof(buffer)))
        {
            return false;
        }

        value = buffer;
        return true;
    }

    bool TryPickTextureFile(string& outPath, string& outGuid)
    {
        WCHAR buffer[MAX_PATH] = {};

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"Texture Files\0*.dds;*.png;*.jpg;*.jpeg;*.tga;*.bmp\0All Files\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = L"../../../Client/Bin/Resources/Models/";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&ofn))
        {
            return false;
        }

        const fs::path picked = fs::path(buffer).lexically_normal();
        error_code ec;
        const fs::path cwd = fs::weakly_canonical(fs::current_path(), ec);
        if (!ec && !cwd.empty())
        {
            const fs::path rel = picked.lexically_relative(cwd);
            outPath = rel.empty() ? String::ToString(picked.wstring()) : rel.generic_string();
        }
        else
        {
            outPath = String::ToString(picked.wstring());
        }

        outGuid.clear();

        const string guid = GAME->Ensure_AssetGUID(picked.wstring(), "Texture");
        if (!guid.empty())
        {
            outGuid = guid;
        }

        return true;
    }

    string ReadTextureOverrideDisplayPath(const json& instJson, const char* key)
    {
        if (!instJson.contains("textures") ||
            !instJson["textures"].is_object() ||
            !instJson["textures"].contains(key) ||
            !instJson["textures"][key].is_object())
        {
            return {};
        }

        const json& tex = instJson["textures"][key];
        const string path = tex.value("texture_path", string{});
        if (!path.empty())
        {
            return path;
        }

        const string guid = tex.value("texture_guid", string{});
        if (!guid.empty())
        {
            const wstring resolved = GAME->Resolve_AssetPath(guid);
            if (!resolved.empty())
            {
                return String::ToString(resolved);
            }
        }

        return {};
    }

    string BuildTextureDisplayName(const string& texturePath)
    {
        if (texturePath.empty())
        {
            return {};
        }

        const string filename = fs::path(texturePath).filename().string();
        return filename.empty() ? texturePath : filename;
    }

    void SetTextureOverrideFromFile(json& instJson, const char* key, const string& texturePath, const string& textureGuid)
    {
        instJson["textures"][key]["enabled"] = true;
        instJson["textures"][key]["texture_path"] = texturePath;

        if (!textureGuid.empty())
        {
            instJson["textures"][key]["texture_guid"] = textureGuid;
        }
        else
        {
            instJson["textures"][key].erase("texture_guid");
        }
    }

    void ClearTextureOverride(json& instJson, const char* key)
    {
        if (!instJson.contains("textures") || !instJson["textures"].is_object())
        {
            return;
        }

        instJson["textures"].erase(key);
        if (instJson["textures"].empty())
        {
            instJson.erase("textures");
        }
    }

    json* FindBinding(json& bindings, uint32 materialIndex)
    {
        if (!bindings.is_array())
        {
            return nullptr;
        }

        for (json& binding : bindings)
        {
            if (binding.is_object() && binding.value("material_index", UINT32_MAX) == materialIndex)
            {
                return &binding;
            }
        }

        return nullptr;
    }

    json& EnsureBinding(MapEditor::ModelOverrideEntry& entry, uint32 materialIndex, const SelectedModelContext& target)
    {
        entry.hasMaterialBindings = true;

        if (!entry.materialBindingsJson.is_array())
        {
            entry.materialBindingsJson = json::array();
        }

        if (json* binding = FindBinding(entry.materialBindingsJson, materialIndex))
        {
            return *binding;
        }

        json binding = json::object();
        binding["material_index"] = materialIndex;
        binding["base_bundle_key"] = entry.baseBundleKey.empty() ? BuildBaseBundleKey(target.modelPath) : entry.baseBundleKey;
        binding["instance_override"] = json::object();
        entry.materialBindingsJson.push_back(std::move(binding));
        return entry.materialBindingsJson.back();
    }

    void RemoveBinding(MapEditor::ModelOverrideEntry& entry, uint32 materialIndex)
    {
        if (!entry.materialBindingsJson.is_array())
        {
            return;
        }

        for (auto it = entry.materialBindingsJson.begin(); it != entry.materialBindingsJson.end(); ++it)
        {
            if (it->is_object() && it->value("material_index", UINT32_MAX) == materialIndex)
            {
                entry.materialBindingsJson.erase(it);
                break;
            }
        }

        entry.hasMaterialBindings = !entry.materialBindingsJson.empty();
    }

    json& EnsureInstanceOverride(json& binding)
    {
        if (!binding.contains("instance_override") || !binding["instance_override"].is_object())
        {
            binding["instance_override"] = json::object();
        }
        return binding["instance_override"];
    }

    void SetCBOverride(json& instJson, const char* key, bool enabled)
    {
        if (enabled)
        {
            instJson["cb_overrides"][key] = true;
            return;
        }

        if (instJson.contains("cb_overrides") && instJson["cb_overrides"].is_object())
        {
            instJson["cb_overrides"].erase(key);
            if (instJson["cb_overrides"].empty())
            {
                instJson.erase("cb_overrides");
            }
        }
    }

    bool ReadVec4(const json& parent, const char* key, Vec4& out)
    {
        if (!parent.contains(key) || !parent[key].is_array() || parent[key].size() < 4)
        {
            return false;
        }

        out = Vec4(parent[key][0].get<float>(), parent[key][1].get<float>(), parent[key][2].get<float>(), parent[key][3].get<float>());
        return true;
    }

    bool ReadVec2(const json& parent, const char* key, Vec2& out)
    {
        if (!parent.contains(key) || !parent[key].is_array() || parent[key].size() < 2)
        {
            return false;
        }

        out = Vec2(parent[key][0].get<float>(), parent[key][1].get<float>());
        return true;
    }

    bool IsTextureOverrideMeaningful(const json& tex)
    {
        return tex.is_object() &&
            tex.value("enabled", false);
    }

    bool IsInstanceOverrideMeaningful(const json& instJson)
    {
        if (!instJson.is_object())
        {
            return false;
        }

        if (instJson.value("force_pass_index", -1) >= 0 ||
            instJson.value("forcePassIndex", -1) >= 0 ||
            instJson.value("override_template_enabled", false) ||
            instJson.value("override_render_state_enabled", false))
        {
            return true;
        }

        if (instJson.contains("cb_overrides") &&
            instJson["cb_overrides"].is_object() &&
            !instJson["cb_overrides"].empty())
        {
            return true;
        }

        if (instJson.contains("textures") && instJson["textures"].is_object())
        {
            for (const auto& item : instJson["textures"].items())
            {
                if (IsTextureOverrideMeaningful(item.value()))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool IsBindingMeaningful(const json& binding)
    {
        if (!binding.is_object())
        {
            return false;
        }

        if (!binding.value("base_bundle_key", string{}).empty())
        {
            return true;
        }

        return binding.contains("instance_override") &&
            IsInstanceOverrideMeaningful(binding["instance_override"]);
    }

    void ApplySelectedEntryToModel(MapEditor::SceneContext* ctx, const SelectedModelContext& target, const MapEditor::ModelOverrideEntry& entry)
    {
        if (ctx)
        {
            ctx->_dirtyPassOverrides = true;
        }

        if (!target.model || !entry.HasMaterialBindingsOverride())
        {
            return;
        }

        target.model->From_Json(entry.BuildMaterialJson(target.modelPath, GetMapName(ctx)));

        if (target.object)
        {
            for (auto& chunk : target.object->_instancedChunkHandles)
            {
                if (chunk)
                {
                    chunk->Refresh_HasGlow();
                }
            }
        }
    }

    void DrawInfoRow(const char* label, const string& value)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(120.f);
        ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
    }

    void DrawInfoRow(const char* label, const char* value)
    {
        DrawInfoRow(label, string(value ? value : ""));
    }

    void DrawLivePreview(const Shared<MapEditor::PanelViewport>& viewport, const SelectedModelContext& target)
    {
        ImGui::TextDisabled("Current Applied Object");

        const float width = ImGui::GetContentRegionAvail().x;
        const float height = max(180.f, width * 0.62f);
        const ImVec2 size(width, height);
        const ImVec2 cursor = ImGui::GetCursorScreenPos();

        ID3D11ShaderResourceView* srv = viewport ? viewport->Get_SceneSRV() : nullptr;
        if (srv)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(srv), size);
        }
        else
        {
            ImGui::Dummy(size);
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = cursor;
        const ImVec2 max(cursor.x + size.x, cursor.y + size.y);
        drawList->AddRectFilled(min, max, IM_COL32(18, 18, 18, srv ? 24 : 255));
        drawList->AddRect(min, max, IM_COL32(95, 95, 95, 255), 4.f, 0, 1.5f);

        const string label = target.filename.empty() ? "No selected model" : target.filename;
        const ImVec2 labelPos(min.x + 10.f, min.y + 10.f);
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        drawList->AddRectFilled(
            ImVec2(labelPos.x - 6.f, labelPos.y - 4.f),
            ImVec2(labelPos.x + labelSize.x + 8.f, labelPos.y + labelSize.y + 6.f),
            IM_COL32(0, 0, 0, 150),
            4.f);
        drawList->AddText(labelPos, IM_COL32(245, 245, 245, 255), label.c_str());

        if (!srv)
        {
            const char* missing = "Viewport preview is not ready";
            const ImVec2 textSize = ImGui::CalcTextSize(missing);
            drawList->AddText(
                ImVec2(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f),
                IM_COL32(180, 180, 180, 255),
                missing);
        }
    }

    // 좌측 영역: 실시간 씬 프리뷰와 선택 오브젝트/저장 경로 요약.
    void DrawSelectedMaterialHeaderPane(
        const Shared<MapEditor::PanelViewport>& viewport,
        const SelectedModelContext& target,
        const string& mapName,
        const fs::path& materialPath,
        const function<void()>& createEntry,
        const function<void()>& loadMapMaterial)
    {
        DrawLivePreview(viewport, target);

        ImGui::SeparatorText("Header");
        DrawInfoRow("Map", mapName);
        DrawInfoRow("Object", String::ToString(target.object->_name));
        DrawInfoRow("Kind", MapMaterialObjectKindName(target.materialObjectKind));
        DrawInfoRow("Model", target.filename);
        DrawInfoRow("Materials", to_string(target.model ? target.model->Get_NumMaterials() : 0));
        DrawInfoRow("Material Json", materialPath.string());

        ImGui::Spacing();
        if (ImGui::Button("+ Override Entry"))
        {
            createEntry();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Map Material"))
        {
            loadMapMaterial();
        }
    }

    // 우측 상단: 선택 모델에 대한 즉시 적용/저장/초기화 명령.
    void DrawMaterialCommandBar(
        MapEditor::SceneContext* ctx,
        const SelectedModelContext& target,
        MapEditor::ModelOverrideEntry& entry,
        const string& mapName)
    {
        if (ImGui::Button("Apply To Selected"))
        {
            ApplySelectedEntryToModel(ctx, target, entry);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Map Material") && entry.HasAnyMaterialOverride())
        {
            entry.ExportToNativeMaterial(target.modelPath, mapName);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear V2"))
        {
            entry.hasMaterialBindings = false;
            entry.materialBindingsJson = json::array();
            entry.materialMetaJson = json::object();
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }
    }

    string BuildMaterialSlotLabel(const Shared<ModelCom>& model, int32 materialIndex)
    {
        string label = "[" + to_string(materialIndex) + "] ";
        if (!model)
        {
            return label + "<null>";
        }

        const Shared<Material> material = model->Get_Material(static_cast<uint32>(materialIndex));
        return label + (material ? material->Get_MaterialName() : string("<null>"));
    }

    uint32 GetMaterialSlotCountForUi(const Shared<ModelCom>& model)
    {
        if (!model)
        {
            return 1;
        }

        uint32 slotCount = static_cast<uint32>(max<size_t>(1, model->Get_NumMaterials()));
        const auto& meshes = model->Get_Meshes();
        for (const Shared<Mesh>& mesh : meshes)
        {
            if (!mesh)
            {
                continue;
            }

            slotCount = max(slotCount, mesh->Get_MaterialIndex() + 1u);
        }

        return max(1u, slotCount);
    }

    void DrawSelectedModelMeshMap(const Shared<ModelCom>& model)
    {
        if (!model || !ImGui::CollapsingHeader("Mesh -> Material Slot", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        const auto& meshes = model->Get_Meshes();
        for (uint32 i = 0; i < static_cast<uint32>(meshes.size()); ++i)
        {
            const Shared<Mesh>& mesh = meshes[i];
            if (!mesh)
            {
                continue;
            }

            const uint32 materialIndex = mesh->Get_MaterialIndex();
            const Shared<Material> material = model->Get_Material(materialIndex);
            ImGui::BulletText("Mesh[%u] %s  ->  Material[%u] : %s",
                i,
                mesh->Get_Name().c_str(),
                materialIndex,
                material ? material->Get_MaterialName().c_str() : "<null>");
        }
    }

    // 현재 모델의 material index를 고르는 compact combo.
    void DrawMaterialSlotPicker(int32& selectedMaterialIndex, const Shared<ModelCom>& model)
    {
        const int32 materialCount = static_cast<int32>(GetMaterialSlotCountForUi(model));
        const int32 clampedMaterialCount = max(1, materialCount);
        selectedMaterialIndex = std::clamp(selectedMaterialIndex, 0, clampedMaterialCount - 1);

        const string materialLabel = BuildMaterialSlotLabel(model, selectedMaterialIndex);
        ImGui::SetNextItemWidth(280.f);
        if (ImGui::BeginCombo("Material Slot", materialLabel.c_str()))
        {
            for (int32 i = 0; i < clampedMaterialCount; ++i)
            {
                const bool selected = (selectedMaterialIndex == i);
                const string label = BuildMaterialSlotLabel(model, i);
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    selectedMaterialIndex = i;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    string GetResolvedTexturePath(const Material::TextureSlot* textureSlot)
    {
        if (textureSlot == nullptr)
        {
            return {};
        }

        if (!textureSlot->resolvedPath.empty())
        {
            return String::ToString(textureSlot->resolvedPath);
        }

        return {};
    }

    void DrawCurrentMaterialSummary(const Shared<Material>& material, int32 materialIndex)
    {
        ImGui::SeparatorText("Current Material");

        if (!material)
        {
            ImGui::TextDisabled("Material[%d] is not populated on this ModelCom.", materialIndex);
            return;
        }

        DrawInfoRow("Slot", "[" + to_string(materialIndex) + "] " + material->Get_MaterialName());
        DrawInfoRow("Template", magic_enum::enum_name(material->Get_TemplateId()).data());
        DrawInfoRow("Pass", to_string(material->Get_PassIndex()));
    }

    void DrawCurrentMaterialTextureViewer(const Shared<Material>& material)
    {
        ImGui::SeparatorText("Current Applied Textures");

        constexpr MaterialTextureSlot slots[] = {
            MaterialTextureSlot::BaseColor,
            MaterialTextureSlot::Normal,
            MaterialTextureSlot::ORM,
            MaterialTextureSlot::ORM2,
            MaterialTextureSlot::ORM3,
            MaterialTextureSlot::Emissive,
            MaterialTextureSlot::OpacityMask,
            MaterialTextureSlot::HairDepth,
            MaterialTextureSlot::HairID,
            MaterialTextureSlot::IrisHeight,
            MaterialTextureSlot::IrisMask,
        };

        if (!material)
        {
            ImGui::TextDisabled("No runtime material is currently assigned to this slot.");
            return;
        }

        for (MaterialTextureSlot slot : slots)
        {
            const Material::TextureSlot* textureSlot = material->Find_TextureSlot(slot, 0);
            const string texturePath = GetResolvedTexturePath(textureSlot);
            const string displayName = BuildTextureDisplayName(texturePath);

            ImGui::PushID(static_cast<int32>(slot));
            ImGui::Text("%-12s", magic_enum::enum_name(slot).data());
            ImGui::SameLine();

            char pathBuffer[MAX_PATH] = {};
            strncpy_s(pathBuffer, displayName.c_str(), sizeof(pathBuffer) - 1);

            ImGui::SetNextItemWidth(300.f);
            ImGui::InputText("##current_tex_path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
            if (!texturePath.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("%s", texturePath.c_str());
            }

            if (texturePath.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(none)");
            }

            ImGui::PopID();
        }
    }

    // v2 material binding의 CB 오버라이드 편집. MainEditor MaterialView의 ModelCom 편집 항목과 같은 키를 쓴다.
    void DrawMaterialCBEditor(json& instJson, MapEditor::ModelOverrideEntry& entry, MapEditor::SceneContext* ctx)
    {
        (void)entry;

        ImGui::SeparatorText("CB Overrides");

        const MaterialCB defaults{};

        auto isEnabled = [&](const char* key) -> bool
        {
            return instJson.contains("cb_overrides") &&
                instJson["cb_overrides"].is_object() &&
                instJson["cb_overrides"].value(key, false);
        };

        auto ensureCBDefault = [&](const char* key, const json& defaultValue)
        {
            if (!instJson.contains("cb") || !instJson["cb"].is_object())
            {
                instJson["cb"] = json::object();
            }

            if (!instJson["cb"].contains(key))
            {
                instJson["cb"][key] = defaultValue;
            }
        };

        auto setEnabled = [&](const char* key, bool enabled, const json& defaultValue)
        {
            SetCBOverride(instJson, key, enabled);
            if (enabled)
            {
                ensureCBDefault(key, defaultValue);
            }
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        };

        auto drawVec4CB = [&](const char* label, const char* key, const Vec4& defaultValue)
        {
            bool enabled = isEnabled(key);
            string checkboxLabel = string("##") + key + "_enable";
            if (ImGui::Checkbox(checkboxLabel.c_str(), &enabled))
            {
                setEnabled(key, enabled, { defaultValue.x, defaultValue.y, defaultValue.z, defaultValue.w });
            }
            ImGui::SameLine();

            Vec4 value = defaultValue;
            if (instJson.contains("cb") && instJson["cb"].is_object())
            {
                (void)ReadVec4(instJson["cb"], key, value);
            }

            float values[4] = { value.x, value.y, value.z, value.w };
            if (!enabled)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::ColorEdit4(label, values))
            {
                instJson["cb"][key] = { values[0], values[1], values[2], values[3] };
                SetCBOverride(instJson, key, true);
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }
            if (!enabled)
            {
                ImGui::EndDisabled();
            }
        };

        auto drawFloatCB = [&](const char* label, const char* key, float defaultValue, float minValue, float maxValue)
        {
            bool enabled = isEnabled(key);
            string checkboxLabel = string("##") + key + "_enable";
            if (ImGui::Checkbox(checkboxLabel.c_str(), &enabled))
            {
                setEnabled(key, enabled, defaultValue);
            }
            ImGui::SameLine();

            float value = defaultValue;
            if (instJson.contains("cb") && instJson["cb"].is_object())
            {
                value = instJson["cb"].value(key, defaultValue);
            }

            ImGui::SetNextItemWidth(160.f);
            if (!enabled)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::DragFloat(label, &value, 0.01f, minValue, maxValue))
            {
                instJson["cb"][key] = value;
                SetCBOverride(instJson, key, true);
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }
            if (!enabled)
            {
                ImGui::EndDisabled();
            }
        };

        auto drawVec2CB = [&](const char* label, const char* key, const Vec2& defaultValue)
        {
            bool enabled = isEnabled(key);
            string checkboxLabel = string("##") + key + "_enable";
            if (ImGui::Checkbox(checkboxLabel.c_str(), &enabled))
            {
                setEnabled(key, enabled, { defaultValue.x, defaultValue.y });
            }
            ImGui::SameLine();

            Vec2 value = defaultValue;
            if (instJson.contains("cb") && instJson["cb"].is_object())
            {
                (void)ReadVec2(instJson["cb"], key, value);
            }

            float values[2] = { value.x, value.y };
            ImGui::SetNextItemWidth(180.f);
            if (!enabled)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::DragFloat2(label, values, 0.01f))
            {
                instJson["cb"][key] = { values[0], values[1] };
                SetCBOverride(instJson, key, true);
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }
            if (!enabled)
            {
                ImGui::EndDisabled();
            }
        };

        auto drawChannelCB = [&](const char* label, const char* key, uint32 defaultValue)
        {
            bool enabled = isEnabled(key);
            string checkboxLabel = string("##") + key + "_enable";
            if (ImGui::Checkbox(checkboxLabel.c_str(), &enabled))
            {
                setEnabled(key, enabled, defaultValue);
            }
            ImGui::SameLine();

            uint32 currentValue = defaultValue;
            if (instJson.contains("cb") && instJson["cb"].is_object())
            {
                currentValue = instJson["cb"].value(key, defaultValue);
            }

            int32 current = ClampMaterialChannelSourceIndex(currentValue);
            ImGui::SetNextItemWidth(160.f);
            if (!enabled)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Combo(label, &current, kMaterialChannelSourceLabels, kMaterialChannelSourceLabelCount))
            {
                instJson["cb"][key] = static_cast<uint32>(current);
                SetCBOverride(instJson, key, true);
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }
            if (!enabled)
            {
                ImGui::EndDisabled();
            }
        };

        auto drawUIntCB = [&](const char* label, const char* key, uint32 defaultValue)
        {
            bool enabled = isEnabled(key);
            string checkboxLabel = string("##") + key + "_enable";
            if (ImGui::Checkbox(checkboxLabel.c_str(), &enabled))
            {
                setEnabled(key, enabled, defaultValue);
            }
            ImGui::SameLine();

            int32 value = static_cast<int32>(defaultValue);
            if (instJson.contains("cb") && instJson["cb"].is_object())
            {
                value = static_cast<int32>(instJson["cb"].value(key, defaultValue));
            }

            ImGui::SetNextItemWidth(160.f);
            if (!enabled)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::InputInt(label, &value))
            {
                instJson["cb"][key] = static_cast<uint32>(max(0, value));
                SetCBOverride(instJson, key, true);
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }
            if (!enabled)
            {
                ImGui::EndDisabled();
            }
        };

        drawVec4CB("Base Color Tint", "baseColorTint", defaults.baseColorTint);
        drawVec4CB("Emissive Color", "emissiveColor", defaults.emissiveColor);
        drawFloatCB("Emissive Intensity", "emissiveIntensity", defaults.emissiveIntensity, 0.f, 100.f);
        drawFloatCB("Emissive Mask Mul", "emissiveMaskMul", defaults.emissiveMaskMul, 0.f, 100.f);
        drawFloatCB("Normal Strength", "normalStrength", defaults.normalStrength, 0.f, 10.f);
        drawFloatCB("Opacity Clip CB", "opacityMaskClipValue", defaults.opacityMaskClipValue, 0.f, 1.f);
        drawFloatCB("Roughness Scale", "roughnessScale", defaults.roughnessScale, 0.f, 10.f);
        drawFloatCB("Metalness Scale", "metalnessScale", defaults.metalnessScale, 0.f, 10.f);
        drawFloatCB("AO Scale", "aoScale", defaults.aoScale, 0.f, 10.f);
        drawChannelCB("ORM AO Source", "ormAoSource", defaults.ormAoSource);
        drawChannelCB("ORM Roughness Source", "ormRoughnessSource", defaults.ormRoughnessSource);
        drawChannelCB("ORM Metalness Source", "ormMetalnessSource", defaults.ormMetalnessSource);
        drawChannelCB("Opacity Mask Source", "opacityMaskSource", defaults.opacityMaskSource);
        drawVec2CB("UV Scale", "uvScale", defaults.uvScale);
        drawVec2CB("UV Offset", "uvOffset", defaults.uvOffset);
        drawFloatCB("UV Rotation", "uvRotation", defaults.uvRotation, -100.f, 100.f);
        drawUIntCB("Material Mask", "materialMask", defaults.materialMask);
    }

    // v2 material binding의 texture slot 오버라이드 편집.
    void DrawMaterialTextureEditor(json& instJson, MapEditor::SceneContext* ctx, const Shared<Material>& currentMaterial)
    {
        ImGui::SeparatorText("Texture Overrides");

        constexpr MaterialTextureSlot slots[] = {
            MaterialTextureSlot::BaseColor,
            MaterialTextureSlot::Normal,
            MaterialTextureSlot::ORM,
            MaterialTextureSlot::ORM2,
            MaterialTextureSlot::ORM3,
            MaterialTextureSlot::Emissive,
            MaterialTextureSlot::OpacityMask,
            MaterialTextureSlot::HairDepth,
            MaterialTextureSlot::HairID,
            MaterialTextureSlot::IrisHeight,
            MaterialTextureSlot::IrisMask,
        };

        for (MaterialTextureSlot slot : slots)
        {
            const char* key = TextureBindingKey(slot);
            if (key[0] == '\0')
            {
                continue;
            }

            ImGui::PushID(key);

            bool enabled = instJson.contains("textures") &&
                instJson["textures"].is_object() &&
                instJson["textures"].contains(key) &&
                instJson["textures"][key].value("enabled", false);

            if (ImGui::Checkbox("##tex_enable", &enabled))
            {
                if (enabled)
                {
                    instJson["textures"][key]["enabled"] = true;
                    if (!instJson["textures"][key].contains("texture_path"))
                    {
                        instJson["textures"][key]["texture_path"] = "";
                    }
                }
                else if (instJson.contains("textures") && instJson["textures"].is_object())
                {
                    instJson["textures"].erase(key);
                }
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }

            ImGui::SameLine();
            ImGui::Text("%-12s", magic_enum::enum_name(slot).data());
            ImGui::SameLine();

            const Material::TextureSlot* currentSlot = currentMaterial ? currentMaterial->Find_TextureSlot(slot, 0) : nullptr;
            const string currentPath = GetResolvedTexturePath(currentSlot);
            string texPath = enabled ? ReadTextureOverrideDisplayPath(instJson, key) : currentPath;
            const string texDisplayName = BuildTextureDisplayName(texPath);
            char pathBuffer[MAX_PATH] = {};
            strncpy_s(pathBuffer, texDisplayName.c_str(), sizeof(pathBuffer) - 1);

            ImGui::SetNextItemWidth(300.f);
            ImGui::InputText("##tex_path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
            if (!texPath.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("%s", texPath.c_str());
            }
            ImGui::SameLine();

            if (ImGui::SmallButton("Browse"))
            {
                string pickedPath;
                string pickedGuid;
                if (TryPickTextureFile(pickedPath, pickedGuid))
                {
                    SetTextureOverrideFromFile(instJson, key, pickedPath, pickedGuid);
                    if (ctx)
                    {
                        ctx->_dirtyPassOverrides = true;
                    }
                }
            }
            ImGui::SameLine();

            if (ImGui::SmallButton("Clear"))
            {
                ClearTextureOverride(instJson, key);
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }

            ImGui::SameLine();
            ImGui::TextDisabled("%s", enabled ? "Override" : "Current");

            ImGui::PopID();
        }
    }

    // v2 instance override의 Material template 선택. 엔진 MaterialInstanceOverride에 저장되어 실제 Material에 반영된다.
    void DrawMaterialTemplateEditor(json& instJson, MapEditor::SceneContext* ctx)
    {
        ImGui::SeparatorText("Template");

        bool enabled = instJson.value("override_template_enabled", false);
        if (ImGui::Checkbox("Override Template", &enabled))
        {
            instJson["override_template_enabled"] = enabled;
            if (enabled && !instJson.contains("template"))
            {
                instJson["template"] = static_cast<uint32>(MaterialShaderTemplate::OpaquePBR);
            }
            if (!enabled)
            {
                instJson.erase("template");
                instJson.erase("template_id");
            }
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }

        uint32 templateValue = instJson.value("template", static_cast<uint32>(MaterialShaderTemplate::OpaquePBR));
        templateValue = min<uint32>(templateValue, static_cast<uint32>(MaterialShaderTemplate::END) - 1u);
        MaterialShaderTemplate currentTemplate = static_cast<MaterialShaderTemplate>(templateValue);

        ImGui::SetNextItemWidth(180.f);
        if (!enabled)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::BeginCombo("Template", magic_enum::enum_name(currentTemplate).data()))
        {
            for (uint32 i = 0; i < static_cast<uint32>(MaterialShaderTemplate::END); ++i)
            {
                const MaterialShaderTemplate candidate = static_cast<MaterialShaderTemplate>(i);
                const bool selected = (candidate == currentTemplate);
                if (ImGui::Selectable(magic_enum::enum_name(candidate).data(), selected))
                {
                    instJson["template"] = i;
                    instJson["override_template_enabled"] = true;
                    if (ctx)
                    {
                        ctx->_dirtyPassOverrides = true;
                    }
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (!enabled)
        {
            ImGui::EndDisabled();
        }
    }

    // v2 instance override의 RenderState 편집. pass 자동 계산과 masked clip 기준에 영향을 준다.
    void DrawMaterialRenderStateEditor(json& instJson, MapEditor::SceneContext* ctx)
    {
        ImGui::SeparatorText("Render State");

        bool enabled = instJson.value("override_render_state_enabled", false);
        if (ImGui::Checkbox("Override RenderState", &enabled))
        {
            instJson["override_render_state_enabled"] = enabled;
            if (enabled && (!instJson.contains("render_state") || !instJson["render_state"].is_object()))
            {
                MaterialRenderState defaults{};
                instJson["render_state"]["twoSided"] = defaults.twoSided;
                instJson["render_state"]["blendMode"] = static_cast<uint32>(defaults.blendMode);
                instJson["render_state"]["alphaClip"] = defaults.alphaClip;
                instJson["render_state"]["opacityMaskClipValue"] = defaults.opacityMaskClipValue;
            }
            if (!enabled)
            {
                instJson.erase("render_state");
            }
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }

        if (!enabled)
        {
            ImGui::BeginDisabled();
        }

        const MaterialRenderState defaults{};
        json defaultRenderState = {
            {"twoSided", defaults.twoSided},
            {"blendMode", static_cast<uint32>(defaults.blendMode)},
            {"alphaClip", defaults.alphaClip},
            {"opacityMaskClipValue", defaults.opacityMaskClipValue},
        };
        json* rsPtr = &defaultRenderState;
        if (enabled)
        {
            if (!instJson.contains("render_state") || !instJson["render_state"].is_object())
            {
                instJson["render_state"] = defaultRenderState;
            }
            rsPtr = &instJson["render_state"];
        }
        else if (instJson.contains("render_state") && instJson["render_state"].is_object())
        {
            rsPtr = &instJson["render_state"];
        }
        json& rs = *rsPtr;

        bool twoSided = rs.value("twoSided", defaults.twoSided);
        if (ImGui::Checkbox("TwoSided", &twoSided))
        {
            rs["twoSided"] = twoSided;
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }

        uint32 blendValue = rs.value("blendMode", static_cast<uint32>(defaults.blendMode));
        blendValue = min<uint32>(blendValue, static_cast<uint32>(MaterialBlendMode::END) - 1u);
        MaterialBlendMode currentBlend = static_cast<MaterialBlendMode>(blendValue);

        ImGui::SetNextItemWidth(180.f);
        if (ImGui::BeginCombo("Blend", magic_enum::enum_name(currentBlend).data()))
        {
            for (uint32 i = 0; i < static_cast<uint32>(MaterialBlendMode::END); ++i)
            {
                const MaterialBlendMode candidate = static_cast<MaterialBlendMode>(i);
                const bool selected = (candidate == currentBlend);
                if (ImGui::Selectable(magic_enum::enum_name(candidate).data(), selected))
                {
                    rs["blendMode"] = i;
                    if (ctx)
                    {
                        ctx->_dirtyPassOverrides = true;
                    }
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        bool alphaClip = rs.value("alphaClip", defaults.alphaClip);
        if (ImGui::Checkbox("Alpha Clip", &alphaClip))
        {
            rs["alphaClip"] = alphaClip;
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }

        float clip = rs.value("opacityMaskClipValue", defaults.opacityMaskClipValue);
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::DragFloat("Clip", &clip, 0.001f, 0.f, 1.f))
        {
            rs["opacityMaskClipValue"] = clip;
            instJson["cb"]["opacityMaskClipValue"] = clip;
            SetCBOverride(instJson, "opacityMaskClipValue", true);
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }

        if (!enabled)
        {
            ImGui::EndDisabled();
        }
    }

    // 선택 material index에 대한 v2 material_bindings 편집 전체.
    void DrawMaterialBindingEditor(
        MapEditor::SceneContext* ctx,
        const SelectedModelContext& target,
        MapEditor::ModelOverrideEntry& entry,
        int32 selectedMaterialIndex)
    {
        json& binding = EnsureBinding(entry, static_cast<uint32>(selectedMaterialIndex), target);
        json& instJson = EnsureInstanceOverride(binding);

        string baseBundleKey = binding.value("base_bundle_key", entry.baseBundleKey);
        if (InputTextString("Base Bundle Key", baseBundleKey, 220.f))
        {
            binding["base_bundle_key"] = ToLowerAscii(baseBundleKey);
            entry.baseBundleKey = ToLowerAscii(baseBundleKey);
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }

        if (ImGui::Button("Clear Slot Overrides"))
        {
            instJson = json::object();
            if (ctx)
            {
                ctx->_dirtyPassOverrides = true;
            }
        }

        DrawMaterialTemplateEditor(instJson, ctx);
        DrawMaterialRenderStateEditor(instJson, ctx);

        const bool isInstance = (entry.shaderType == ShaderType::VtxInstanceMeshExact);
        const auto* opts = isInstance ? g_instanceMeshPassOptions : g_staticMeshPassOptions;
        const int32 passCount = isInstance ? g_instanceMeshPassOptionCount : g_staticMeshPassOptionCount;
        int32 forcePassIndex = instJson.value("force_pass_index", -1);
        string passLabel = "Inherit";
        for (int32 i = 0; i < passCount; ++i)
        {
            if (static_cast<int32>(opts[i].second) == forcePassIndex)
            {
                passLabel = opts[i].first;
                break;
            }
        }

        ImGui::SetNextItemWidth(160.f);
        if (ImGui::BeginCombo("Force Pass", passLabel.c_str()))
        {
            if (ImGui::Selectable("Inherit", forcePassIndex < 0))
            {
                instJson.erase("force_pass_index");
                if (ctx)
                {
                    ctx->_dirtyPassOverrides = true;
                }
            }
            for (int32 i = 0; i < passCount; ++i)
            {
                const int32 pass = static_cast<int32>(opts[i].second);
                if (ImGui::Selectable(opts[i].first, forcePassIndex == pass))
                {
                    instJson["force_pass_index"] = pass;
                    if (ctx)
                    {
                        ctx->_dirtyPassOverrides = true;
                    }
                }
            }
            ImGui::EndCombo();
        }

        DrawMaterialCBEditor(instJson, entry, ctx);
        DrawMaterialTextureEditor(instJson, ctx, target.model ? target.model->Get_Material(static_cast<uint32>(selectedMaterialIndex)) : nullptr);
    }

    // 하단 접이식 영역: 기존 pass/texture slot 기반 ModelOverride 관리.
    void DrawLegacyOverrideEntries(MapEditor::SceneContext* ctx)
    {
        if (!ctx || !ImGui::CollapsingHeader("Model Override Entries"))
        {
            return;
        }

        constexpr int32 legacySlotCount = static_cast<int32>(magic_enum::enum_count<MaterialTextureSlot>() - 1);
        vector<string> toRemove;

        for (auto& [filename, entry] : ctx->_modelOverrides)
        {
            ImGui::PushID(filename.c_str());

            if (ImGui::Button("X"))
            {
                toRemove.push_back(filename);
                ImGui::PopID();
                continue;
            }
            ImGui::SameLine();

            ImGui::Text("%s", filename.c_str());
            ImGui::SameLine();

            vector<int32> toRemoveSlot;
            for (auto& [slotIdx, texPath] : entry.textureSlots)
            {
                ImGui::PushID(slotIdx);
                if (ImGui::SmallButton("X"))
                {
                    toRemoveSlot.push_back(slotIdx);
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();

                int32 currentSlot = slotIdx < legacySlotCount ? slotIdx : 0;
                const char* slotNames[16] = {};
                for (int32 i = 0; i < legacySlotCount; ++i)
                {
                    slotNames[i] = magic_enum::enum_name(static_cast<MaterialTextureSlot>(i)).data();
                }

                ImGui::SetNextItemWidth(120.f);
                if (ImGui::Combo("##slot", &currentSlot, slotNames, legacySlotCount))
                {
                    if (entry.textureSlots.find(currentSlot) == entry.textureSlots.end())
                    {
                        entry.textureSlots[currentSlot] = texPath;
                        toRemoveSlot.push_back(slotIdx);
                        ctx->_dirtyPassOverrides = true;
                    }
                }
                ImGui::SameLine();

                char pathBuf[MAX_PATH] = {};
                strncpy_s(pathBuf, texPath.c_str(), sizeof(pathBuf) - 1);
                ImGui::SetNextItemWidth(220.f);
                if (ImGui::InputText("##path", pathBuf, sizeof(pathBuf)))
                {
                    texPath = pathBuf;
                    ctx->_dirtyPassOverrides = true;
                }
                ImGui::SameLine();

                error_code ec;
                fs::path root = fs::weakly_canonical(fs::absolute(_mapSavePath), ec);

                if (ImGui::SmallButton("..."))
                {
                    ImGui::GetIO().AppFocusLost = true;
                    WCHAR buf[MAX_PATH] = {};
                    OPENFILENAME ofn{};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.lpstrFilter = L"Texture Files\0*.dds;*.png;*.jpg\0All Files\0*.*\0";
                    ofn.lpstrFile = buf;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrInitialDir = L"../../../Client/Bin/Resources/Map/";
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                    if (GetOpenFileNameW(&ofn))
                    {
                        fs::path abs = fs::path(buf);
                        if (!ec && !root.empty())
                        {
                            texPath = abs.lexically_relative(root).string();
                        }
                        ctx->_dirtyPassOverrides = true;
                    }
                }
                ImGui::PopID();
            }

            for (int32 s : toRemoveSlot)
            {
                entry.textureSlots.erase(s);
            }

            int32 newSlot = 0;
            const char* slotNames[16] = {};
            for (int32 i = 0; i < legacySlotCount; ++i)
            {
                slotNames[i] = magic_enum::enum_name(static_cast<MaterialTextureSlot>(i)).data();
            }

            ImGui::SetNextItemWidth(120.f);
            ImGui::Combo("##newslot", &newSlot, slotNames, legacySlotCount);
            ImGui::SameLine();
            if (ImGui::SmallButton("+ Add Slot") && entry.textureSlots.find(newSlot) == entry.textureSlots.end())
            {
                entry.textureSlots[newSlot] = "";
                ctx->_dirtyPassOverrides = true;
            }

            ImGui::PopID();
        }

        for (const string& filename : toRemove)
        {
            ctx->_modelOverrides.erase(filename);
            ctx->_dirtyPassOverrides = true;
        }
    }
}

void PanelModelOverride::Render()
{
    bool open = IsOpen();
    if (!ImGui::Begin("Model Override", &open))
    {
        ImGui::End();
        SetOpen(open);
        return;
    }

    if (!_ctx)
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("+ 선택 오브젝트 추가"))
    {
        auto& sel = _ctx->_selected;
        if (sel)
        {
            string filename;
            if (!sel->_modelPath.empty())
            {
                filename = String::ToString(fs::path(sel->_modelPath).filename().wstring());
            }
            else if (!sel->_instancedModelPath.empty())
            {
                filename = String::ToString(fs::path(sel->_instancedModelPath).filename().wstring());
            }

            if (!filename.empty())
            {
                ModelOverrideEntry newEntry;
                newEntry.shaderType = sel->_modelPath.empty() ? ShaderType::VtxInstanceMeshExact : ShaderType::VtxMesh;
                if (_ctx->_modelOverrides.find(filename) == _ctx->_modelOverrides.end())
                {
                    _ctx->_modelOverrides[filename] = newEntry;
                    _ctx->_dirtyPassOverrides = true;
                }
            }
        }
    }

    RenderSelectedMaterialOverride();

    ImGui::Separator();
    DrawLegacyOverrideEntries(_ctx);

    ImGui::End();
    SetOpen(open);
}

void PanelModelOverride::RenderSelectedMaterialOverride()
{
    ImGui::Spacing();
    ImGui::SeparatorText("Selected Model Material");

    SelectedModelContext target;
    if (!ResolveSelectedModel(_ctx, target))
    {
        ImGui::BeginChild("##SelectedMaterialEmpty", ImVec2(0.f, 220.f), true);
        ImGui::TextDisabled("StaticMesh 또는 InstancedMesh 오브젝트를 선택하세요.");
        ImGui::EndChild();
        return;
    }

    const string mapName = GetMapName(_ctx);
    const fs::path materialPath = BuildMaterialJsonPath(mapName, target.modelPath);

    auto ensureEntry = [&]() -> ModelOverrideEntry&
    {
        ModelOverrideEntry& entry = _ctx->_modelOverrides[target.filename];
        entry.shaderType = target.shaderType;
        entry.materialObjectKind = target.materialObjectKind;
        if (entry.baseBundleKey.empty())
        {
            entry.baseBundleKey = BuildBaseBundleKey(target.modelPath);
        }
        return entry;
    };

    auto loadMapMaterial = [&]()
    {

            ifstream ifs(materialPath);
            if (ifs.is_open())
            {
                try
                {
                    json root = json::parse(ifs);
                    ModelOverrideEntry& entry = ensureEntry();
                    if (root.contains("material_bindings"))
                    {
                        entry.LoadMaterialBindingsJson(root);
                    }
                    else
                    {
                        entry.LoadLegacyMaterialJson(root);
                    }

                    entry.shaderType = target.shaderType;
                    if (entry.materialObjectKind == EMapMaterialObjectKind::Unsupported)
                    {
                        entry.materialObjectKind = target.materialObjectKind;
                    }

                    if (entry.baseBundleKey.empty())
                    {
                        entry.baseBundleKey = BuildBaseBundleKey(target.modelPath);
                    }
                    ApplySelectedEntryToModel(_ctx, target, entry);
                }

                catch (...)
                {
                }
            }
    };

    auto createEntry = [&]()
    {
        (void)ensureEntry();
        _ctx->_dirtyPassOverrides = true;
    };

    // 레이아웃만 여기서 관리한다. 좌측은 미리보기/헤더, 우측은 material 편집 기능.
    const float availWidth = ImGui::GetContentRegionAvail().x;
    const float leftWidth = std::clamp(availWidth * 0.38f, 320.f, 460.f);
    if (ImGui::BeginTable("##SelectedMaterialWorkspace", 2,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, leftWidth);
        ImGui::TableSetupColumn("Material", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("##SelectedMaterialLeft", ImVec2(0.f, 0.f), true);
        DrawSelectedMaterialHeaderPane(_viewport, target, mapName, materialPath, createEntry, loadMapMaterial);
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##SelectedMaterialRight", ImVec2(0.f, 0.f), true);
        ImGui::TextDisabled("Material");
        ImGui::SameLine();
        ImGui::Text("%s", target.filename.c_str());

        // material json이 아직 없는 모델도 선택 상태가 바뀌어도 패널 편집 상태를 유지하도록 entry를 즉시 고정한다.
        ModelOverrideEntry& entry = ensureEntry();
        entry.shaderType = target.shaderType;
        entry.materialObjectKind = target.materialObjectKind;
        if (entry.baseBundleKey.empty())
        {
            entry.baseBundleKey = BuildBaseBundleKey(target.modelPath);
        }

        const string materialStateBefore = entry.BuildMaterialJson(target.modelPath, mapName).dump();

        ImGui::Checkbox("Live Apply", &_liveApplyMaterialOverride);
        ImGui::SameLine();
        DrawMaterialCommandBar(_ctx, target, entry, mapName);

        DrawSelectedModelMeshMap(target.model);
        DrawMaterialSlotPicker(_selectedMaterialIndex, target.model);

        const Shared<Material> currentMaterial = target.model
            ? target.model->Get_Material(static_cast<uint32>(_selectedMaterialIndex))
            : nullptr;

        DrawCurrentMaterialSummary(currentMaterial, _selectedMaterialIndex);
        DrawCurrentMaterialTextureViewer(currentMaterial);

        const uint32 runtimeMaterialCount = target.model ? static_cast<uint32>(target.model->Get_NumMaterials()) : 0u;
        if (static_cast<uint32>(_selectedMaterialIndex) >= runtimeMaterialCount)
        {
            ImGui::TextDisabled("This slot is referenced by mesh indices, but no runtime Material exists in ModelCom for it yet.");
        }

        json* existingBinding = FindBinding(entry.materialBindingsJson, static_cast<uint32>(_selectedMaterialIndex));
        bool useBinding = existingBinding != nullptr;
        if (ImGui::Checkbox("Use material_bindings for this material", &useBinding))
        {
            if (useBinding)
            {
                (void)EnsureBinding(entry, static_cast<uint32>(_selectedMaterialIndex), target);
                entry.hasMaterialBindings = true;
            }
            else
            {
                RemoveBinding(entry, static_cast<uint32>(_selectedMaterialIndex));
            }
            _ctx->_dirtyPassOverrides = true;
        }

        if (useBinding)
        {
            DrawMaterialBindingEditor(_ctx, target, entry, _selectedMaterialIndex);
        }

        const string materialStateAfter = entry.BuildMaterialJson(target.modelPath, mapName).dump();
        if (_liveApplyMaterialOverride &&
            materialStateAfter != materialStateBefore &&
            entry.HasMaterialBindingsOverride())
        {
            ApplySelectedEntryToModel(_ctx, target, entry);
        }

        ImGui::EndChild();
        ImGui::EndTable();
    }
}
namespace MapEditor
{
    static bool HasNonEmptyTextureSlot(const map<int32, string>& textureSlots)
    {
        for (const auto& [slotIdx, texPath] : textureSlots)
        {
            if (!texPath.empty())
            {
                return true;
            }
        }
        return false;
    }

    static string ReadTexturePathValue(const json& value)
    {
        if (value.is_string())
        {
            return value.get<string>();
        }

        if (value.is_object())
        {
            return value.value("texture_path", string{});
        }

        return {};
    }

    static void WriteLegacyMaterialCB(json& cbJson, const Engine::MaterialCB& materialCB)
    {
        cbJson["baseColorTint"] = {
            materialCB.baseColorTint.x,
            materialCB.baseColorTint.y,
            materialCB.baseColorTint.z,
            materialCB.baseColorTint.w
        };
        cbJson["emissiveColor"] = {
            materialCB.emissiveColor.x,
            materialCB.emissiveColor.y,
            materialCB.emissiveColor.z,
            materialCB.emissiveColor.w
        };
        cbJson["emissiveIntensity"] = materialCB.emissiveIntensity;
        cbJson["emissiveMaskMul"] = materialCB.emissiveMaskMul;
        cbJson["normalStrength"] = materialCB.normalStrength;
        cbJson["opacityMaskClipValue"] = materialCB.opacityMaskClipValue;
        cbJson["aoScale"] = materialCB.aoScale;
        cbJson["roughnessScale"] = materialCB.roughnessScale;
        cbJson["metalnessScale"] = materialCB.metalnessScale;
        cbJson["uvScale"] = { materialCB.uvScale.x, materialCB.uvScale.y };
        cbJson["uvOffset"] = { materialCB.uvOffset.x, materialCB.uvOffset.y };
        cbJson["uvRotation"] = materialCB.uvRotation;
        cbJson["materialMask"] = materialCB.materialMask;
        cbJson["ormAoSource"] = materialCB.ormAoSource;
        cbJson["ormRoughnessSource"] = materialCB.ormRoughnessSource;
        cbJson["ormMetalnessSource"] = materialCB.ormMetalnessSource;
        cbJson["opacityMaskSource"] = materialCB.opacityMaskSource;
    }

    static void ReadLegacyMaterialCB(const json& cbJson, Engine::MaterialCB& materialCB)
    {
        auto readVec4 = [](const json& data, const string& key, const Vec4& fallback) -> Vec4
        {
            if (!data.contains(key) || !data[key].is_array() || data[key].size() < 4)
            {
                return fallback;
            }
            return Vec4(data[key][0].get<float>(), data[key][1].get<float>(), data[key][2].get<float>(), data[key][3].get<float>());
        };

        auto readVec2 = [](const json& data, const string& key, const Vec2& fallback) -> Vec2
        {
            if (!data.contains(key) || !data[key].is_array() || data[key].size() < 2)
            {
                return fallback;
            }
            return Vec2(data[key][0].get<float>(), data[key][1].get<float>());
        };

        materialCB.baseColorTint = readVec4(cbJson, "baseColorTint", materialCB.baseColorTint);
        materialCB.emissiveColor = readVec4(cbJson, "emissiveColor", materialCB.emissiveColor);
        materialCB.emissiveIntensity = cbJson.value("emissiveIntensity", materialCB.emissiveIntensity);
        materialCB.emissiveMaskMul = cbJson.value("emissiveMaskMul", materialCB.emissiveMaskMul);
        materialCB.normalStrength = cbJson.value("normalStrength", materialCB.normalStrength);
        materialCB.opacityMaskClipValue = cbJson.value("opacityMaskClipValue", materialCB.opacityMaskClipValue);
        materialCB.aoScale = cbJson.value("aoScale", materialCB.aoScale);
        materialCB.roughnessScale = cbJson.value("roughnessScale", materialCB.roughnessScale);
        materialCB.metalnessScale = cbJson.value("metalnessScale", materialCB.metalnessScale);
        materialCB.uvScale = readVec2(cbJson, "uvScale", materialCB.uvScale);
        materialCB.uvOffset = readVec2(cbJson, "uvOffset", materialCB.uvOffset);
        materialCB.uvRotation = cbJson.value("uvRotation", materialCB.uvRotation);
        materialCB.materialMask = cbJson.value("materialMask", materialCB.materialMask);
        materialCB.ormAoSource = cbJson.value("ormAoSource", materialCB.ormAoSource);
        materialCB.ormRoughnessSource = cbJson.value("ormRoughnessSource", materialCB.ormRoughnessSource);
        materialCB.ormMetalnessSource = cbJson.value("ormMetalnessSource", materialCB.ormMetalnessSource);
        materialCB.opacityMaskSource = cbJson.value("opacityMaskSource", materialCB.opacityMaskSource);
    }

    bool ModelOverrideEntry::HasLegacyOverride() const
    {
        return hasMaterialCB || HasNonEmptyTextureSlot(textureSlots);
    }

    bool ModelOverrideEntry::HasMaterialBindingsOverride() const
    {
        if (!hasMaterialBindings || !materialBindingsJson.is_array())
        {
            return false;
        }

        for (const json& binding : materialBindingsJson)
        {
            if (IsBindingMeaningful(binding))
            {
                return true;
            }
        }

        return false;
    }

    bool ModelOverrideEntry::HasAnyMaterialOverride() const
    {
        return HasMaterialBindingsOverride() || HasLegacyOverride();
    }

    void ModelOverrideEntry::LoadLegacyMaterialJson(const json& root)
    {
        if (!root.contains("instances") || !root["instances"].is_array() || root["instances"].empty())
        {
            return;
        }

        const json& instJson = root["instances"][0];
        if (!instJson.is_object())
        {
            return;
        }

        hasMaterialBindings = false;
        materialBindingsJson = json::array();
        materialMetaJson = json::object();

        shaderType = static_cast<ShaderType>(instJson.value("editor_shaderType", static_cast<int>(ShaderType::VtxMesh)));

        json instanceOverride = json::object();
        const int32 legacyPassIndex = instJson.value("forcePassIndex", instJson.value("force_pass_index", -1));
        if (legacyPassIndex >= 0)
        {
            instanceOverride["force_pass_index"] = legacyPassIndex;
        }

        if (instJson.contains("cb") && instJson["cb"].is_object())
        {
            hasMaterialCB = true;
            ReadLegacyMaterialCB(instJson["cb"], materialCB);

            json cbOverrides = json::object();
#define PANEL_MODEL_OVERRIDE_SET_CB_OVERRIDE(_key, _member, _kind) \
            if (instJson["cb"].contains(_key)) cbOverrides[_key] = true;
            MATERIAL_CB_OVERRIDE_FIELDS(PANEL_MODEL_OVERRIDE_SET_CB_OVERRIDE)
#undef PANEL_MODEL_OVERRIDE_SET_CB_OVERRIDE

            if (!cbOverrides.empty())
            {
                instanceOverride["cb_overrides"] = cbOverrides;
                instanceOverride["cb"] = instJson["cb"];
            }
        }

        textureSlots.clear();
        json textures = json::object();
        if (instJson.contains("textures") && instJson["textures"].is_object())
        {
            const json& texObj = instJson["textures"];
            constexpr MaterialTextureSlot slots[] = {
                MaterialTextureSlot::BaseColor,
                MaterialTextureSlot::Normal,
                MaterialTextureSlot::ORM,
                MaterialTextureSlot::ORM2,
                MaterialTextureSlot::ORM3,
                MaterialTextureSlot::Emissive,
                MaterialTextureSlot::OpacityMask,
                MaterialTextureSlot::HairDepth,
                MaterialTextureSlot::HairID,
                MaterialTextureSlot::IrisHeight,
                MaterialTextureSlot::IrisMask,
            };

            for (MaterialTextureSlot slot : slots)
            {
                const char* key = LegacyTextureKey(slot);
                if (key[0] == '\0' || !texObj.contains(key))
                {
                    continue;
                }

                const string texPath = ReadTexturePathValue(texObj[key]);
                if (!texPath.empty())
                {
                    textureSlots[static_cast<int32>(slot)] = texPath;
                    const char* bindingKey = TextureBindingKey(slot);
                    if (bindingKey[0] != '\0')
                    {
                        textures[bindingKey] = {
                            {"enabled", true},
                            {"texture_path", texPath},
                        };
                    }
                }
            }
        }

        if (!textures.empty())
        {
            instanceOverride["textures"] = std::move(textures);
        }

        if (!instanceOverride.empty())
        {
            hasMaterialBindings = true;
            json binding = json::object();
            binding["material_index"] = 0u;
            binding["instance_override"] = instanceOverride;

            if (root.contains("mappings") && root["mappings"].is_array() && !root["mappings"].empty())
            {
                for (const json& mapping : root["mappings"])
                {
                    if (!mapping.is_object())
                    {
                        continue;
                    }

                    const uint32 materialIndex = mapping.value("materialIndex", UINT32_MAX);
                    if (materialIndex == UINT32_MAX)
                    {
                        continue;
                    }

                    json mappedBinding = binding;
                    mappedBinding["material_index"] = materialIndex;
                    materialBindingsJson.push_back(std::move(mappedBinding));
                }
            }

            if (materialBindingsJson.empty())
            {
                materialBindingsJson.push_back(std::move(binding));
            }
        }
    }

    void ModelOverrideEntry::LoadMaterialBindingsJson(const json& root)
    {
        if (!root.contains("material_bindings") || !root["material_bindings"].is_array())
        {
            return;
        }

        hasMaterialBindings = true;
        materialBindingsJson = root["material_bindings"];
        materialMetaJson = root.value("material_meta", json::object());
        materialObjectKind = MapMaterialObjectKindFromName(root.value("map_object_kind", string{}));
        baseBundleKey = ToLowerAscii(root.value("base_bundle_key", string{}));
        shaderType = static_cast<ShaderType>(root.value("editor_shaderType", static_cast<int>(shaderType)));

        if (baseBundleKey.empty())
        {
            for (const json& binding : materialBindingsJson)
            {
                if (binding.is_object())
                {
                    baseBundleKey = ToLowerAscii(binding.value("base_bundle_key", string{}));
                    if (!baseBundleKey.empty())
                    {
                        break;
                    }
                }
            }
        }
    }

    json ModelOverrideEntry::BuildMaterialJson(const string& modelPath, const string& mapName) const
    {
        json root = json::object();
        root["schema"] = "map_model_material";
        root["schema_version"] = 2;
        root["map_name"] = mapName;
        root["map_object_kind"] = MapMaterialObjectKindName(materialObjectKind);
        root["editor_modelPath"] = ToRelativePath(modelPath);
        root["editor_modelStem"] = fs::path(modelPath).stem().string();
        root["editor_shaderType"] = static_cast<int>(shaderType);

        string resolvedBaseBundleKey = baseBundleKey;
        if (resolvedBaseBundleKey.empty())
        {
            resolvedBaseBundleKey = BuildBaseBundleKey(modelPath);
        }
        root["base_bundle_key"] = resolvedBaseBundleKey;

        json bindings = json::array();
        if (materialBindingsJson.is_array())
        {
            for (json binding : materialBindingsJson)
            {
                if (!binding.is_object())
                {
                    continue;
                }

                if (!binding.contains("base_bundle_key") || !binding["base_bundle_key"].is_string())
                {
                    binding["base_bundle_key"] = resolvedBaseBundleKey;
                }

                if (IsBindingMeaningful(binding))
                {
                    bindings.push_back(std::move(binding));
                }
            }
        }
        root["material_bindings"] = std::move(bindings);

        if (materialMetaJson.is_object() && !materialMetaJson.empty())
        {
            root["material_meta"] = materialMetaJson;
        }

        return root;
    }

    void ModelOverrideEntry::ExportToNativeMaterial(const string& modelPath, const string& mapName) const
    {
        if (modelPath.empty() || !HasAnyMaterialOverride())
        {
            return;
        }

        string modelName = fs::path(modelPath).stem().string();
        fs::path matDir = fs::path(_materialsPath) / mapName;
        
        if (!fs::exists(matDir))
        {
            fs::create_directories(matDir);
        }

        string matStem = modelName;
        fs::path matPath = matDir / (matStem + ".material.json");

        if (HasMaterialBindingsOverride())
        {
            ofstream ofs(matPath);
            if (ofs.is_open())
            {
                ofs << BuildMaterialJson(modelPath, mapName).dump(4);
            }
            return;
        }

        if (!HasLegacyOverride())
        {
            return;
        }

        json root;
        json instJson;

        string assetName = modelName + "_Inst";
        instJson["assetName"] = assetName;
        
        instJson["editor_modelPath"] = ToRelativePath(modelPath);
        instJson["editor_shaderType"] = static_cast<int>(shaderType);
        instJson["map_object_kind"] = MapMaterialObjectKindName(materialObjectKind);

        if (hasMaterialCB)
        {
            WriteLegacyMaterialCB(instJson["cb"], materialCB);
        }

        json overridesJson = json::object();
        json texturesJson = json::object();

        MaterialBuilder* builder = GAME->Get_AssetManager()->Get_MaterialBuilder();
        if (builder)
        {
            string bundleKey = baseBundleKey.empty() ? BuildBaseBundleKey(modelPath) : baseBundleKey;

            const auto* bundle = builder->GetOrLoadBundle(bundleKey);
            if (bundle && !bundle->instancesByAssetName.empty())
            {
                const auto& instData = bundle->instancesByAssetName.begin()->second;

                if (!instData.baseColorPath.empty()) texturesJson["baseColor"] = String::ToString(instData.baseColorPath);
                if (!instData.normalPath.empty()) texturesJson["normal"] = String::ToString(instData.normalPath);
                if (!instData.ormPath.empty()) texturesJson["orm"] = String::ToString(instData.ormPath);
                if (!instData.emissivePath.empty()) texturesJson["emissive"] = String::ToString(instData.emissivePath);
                if (!instData.opacityMaskPath.empty()) texturesJson["opacityMask"] = String::ToString(instData.opacityMaskPath);
                if (!instData.hairDepthPath.empty()) texturesJson["hairDepth"] = String::ToString(instData.hairDepthPath);
                if (!instData.hairIDPath.empty()) texturesJson["hairID"] = String::ToString(instData.hairIDPath);
                if (!instData.irisHeightPath.empty()) texturesJson["irisHeight"] = String::ToString(instData.irisHeightPath);
                if (!instData.irisMaskPath.empty()) texturesJson["irisMask"] = String::ToString(instData.irisMaskPath);
            }
        }

        for (auto& pair : textureSlots)
        {
            int32 slotIdx = pair.first;
            string texPath = pair.second;

            if (texPath.empty()) continue;

            Engine::MaterialTextureSlot slotEnum = static_cast<Engine::MaterialTextureSlot>(slotIdx);

            const char* slotNameStr = LegacyTextureKey(slotEnum);

            if (slotNameStr[0] != '\0')
            {
                overridesJson[slotNameStr] = true;
                texturesJson[slotNameStr] = texPath;
            }
        }

        if (!overridesJson.empty())
        {
            instJson["overrides"] = overridesJson;
            instJson["textures"] = texturesJson;
        }
        else if (!texturesJson.empty())
        {
            instJson["textures"] = texturesJson;
        }

        root["instances"] = json::array({ instJson });

        json mappingJson;
        mappingJson["materialIndex"] = 0;
        mappingJson["ref"]["assetName"] = assetName;
        root["mappings"] = json::array({ mappingJson });

        ofstream ofs(matPath);
        if (ofs.is_open())
        {
            ofs << root.dump(4);
        }
    }
}

Shared<PanelModelOverride> PanelModelOverride::Create(SceneContext* ctx, Shared<PanelViewport> viewport)
{
    return make_shared<PanelModelOverride>(ctx, viewport);
}
