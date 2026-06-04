#include "pch.h"
#include "PanelModelOverride.h"
#include "SceneContext.h"

PanelModelOverride::PanelModelOverride(SceneContext* ctx)
    : MEWindow{ L"Model Override" }, _ctx{ ctx }
{
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

            if (!filename.empty() && _ctx->_modelOverrides.find(filename) == _ctx->_modelOverrides.end())
            {
                ModelOverrideEntry newEntry;
                newEntry.shaderType = sel->_modelPath.empty() ? ShaderType::VtxInstanceMeshExact
                    : ShaderType::VtxMesh;
                newEntry.passIndex = -1;
                _ctx->_modelOverrides[filename] = newEntry;
                _ctx->_dirtyPassOverrides = true;
            }
        }
    }

    ImGui::Separator();

    vector<string> toRemove;
    for (auto& [filename, entry] : _ctx->_modelOverrides)
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

        const bool isInstance = (entry.shaderType == ShaderType::VtxInstanceMeshExact);
        auto* opts = isInstance ? g_instanceMeshPassOptions : g_staticMeshPassOptions;
        int32 count = isInstance ? g_instanceMeshPassOptionCount : g_staticMeshPassOptionCount;

        const char* names[16];
        for (int32 i = 0; i < count; ++i)
        {
            names[i] = opts[i].first;
        }

        int32 current = 0;
        for (int32 i = 0; i < count; ++i)
        {
            if ((int32)opts[i].second == entry.passIndex)
            {
                current = i;
                break;
            }
        }

        if (ImGui::Combo("Pass", &current, names, count))
        {
            entry.passIndex = (int32)opts[current].second;
            _ctx->_dirtyPassOverrides = true;
        }

        if (entry.passIndex == static_cast<int32>(VtxMeshPassIndex::LandscapeBlendPass))
        {
            auto DrawSlot = [&](int32 slot, const char* label)
                {
                    string& path = entry.textureSlots[slot];
                    char buf[256] = {};
                    strncpy_s(buf, path.c_str(), sizeof(buf) - 1);
                    if (ImGui::InputText(label, buf, sizeof(buf)))
                    {
                        path = buf;
                        _ctx->_dirtyPassOverrides = true;
                    }
                };

            DrawSlot(0, "Layer1 AR (Slot 0)");
            DrawSlot(5, "Layer2 AR (Slot 5)");
            DrawSlot(6, "Weightmap (Slot 6)");
        }

        ImGui::PopID();
    }


    for (auto& f : toRemove)
    {
        _ctx->_modelOverrides.erase(f);
        _ctx->_dirtyPassOverrides = true;
    }

    ImGui::End();
    SetOpen(open);
}
namespace MapEditor
{
    static string Export_TextureToSlotFolder(const string& mapName, const string& modelName, Engine::MaterialTextureSlot slot, const string& sourceTexPath)
    {
        if (slot != Engine::MaterialTextureSlot::Normal)
        {
            return sourceTexPath;
        }

        fs::path srcPath = fs::path("../../../Client/Bin/Resources/Map") / sourceTexPath;
        if (!fs::exists(srcPath))
        {
            return sourceTexPath; 
        }

        string slotName = magic_enum::enum_name(slot).data();
        fs::path destDir = fs::path("../../../Client/Bin/Resources/Map/MapInstance") / mapName / "Textures" / slotName;
        
        if (!fs::exists(destDir))
        {
            fs::create_directories(destDir);
        }

        fs::path destPath = destDir / srcPath.filename();

        try
        {
            fs::copy_file(srcPath, destPath, fs::copy_options::overwrite_existing);
        }
        catch (...) {}

        return "MapInstance/" + mapName + "/Textures/" + slotName + "/" + destPath.filename().string();
    }

    void ModelOverrideEntry::ExportToNativeMaterial(const string& modelPath, const string& mapName) const
    {
        if (modelPath.empty()) return;

        string modelName = fs::path(modelPath).stem().string();
        fs::path matDir = fs::path("../../../Client/Bin/Resources/Map/MapInstance") / mapName / "Materials";
        
        if (!fs::exists(matDir))
        {
            fs::create_directories(matDir);
        }

        fs::path matPath = matDir / (modelName + ".material.json");

        json root;
        json& instJson = root["instances"][0];

        instJson["name"] = modelName + "_Inst";
        
        instJson["cbMask"]["baseColorTint"] = true;
        instJson["cbMask"]["emissiveColor"] = true;
        instJson["cbMask"]["emissiveIntensity"] = true;
        instJson["cbMask"]["roughnessScale"] = true;
        instJson["cbMask"]["metalnessScale"] = true;

        instJson["cb"]["baseColorTint"] = { materialCB.baseColorTint.x, materialCB.baseColorTint.y, materialCB.baseColorTint.z, materialCB.baseColorTint.w };
        instJson["cb"]["emissiveColor"] = { materialCB.emissiveColor.x, materialCB.emissiveColor.y, materialCB.emissiveColor.z, materialCB.emissiveColor.w };
        instJson["cb"]["emissiveIntensity"] = materialCB.emissiveIntensity;
        instJson["cb"]["roughnessScale"] = materialCB.roughnessScale;
        instJson["cb"]["metalnessScale"] = materialCB.metalnessScale;

        json texArray = json::array();
        for (auto& pair : textureSlots)
        {
            int32 slotIdx = pair.first;
            string texPath = pair.second;

            if (texPath.empty()) continue;

            Engine::MaterialTextureSlot slotEnum = static_cast<Engine::MaterialTextureSlot>(slotIdx);
            
            string newRelPath = Export_TextureToSlotFolder(mapName, modelName, slotEnum, texPath);

            json texJson;
            texJson["slot"] = static_cast<int>(slotEnum);
            texJson["path"] = newRelPath;
            texArray.push_back(texJson);
        }
        instJson["textures"] = texArray;

        ofstream ofs(matPath);
        if (ofs.is_open())
        {
            ofs << root.dump(4);
        }
    }
}

Shared<PanelModelOverride> PanelModelOverride::Create(SceneContext* ctx)
{
    return make_shared<PanelModelOverride>(ctx);
}
