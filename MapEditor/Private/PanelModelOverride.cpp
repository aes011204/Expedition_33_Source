#include "pch.h"
#include "PanelModelOverride.h"
#include "SceneContext.h"
#include "MaterialBuilder.h"
#include "Asset_Manager.h"

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

            int32 current = slotIdx < slotCount ? slotIdx : 0;
            const char* slotNames[16] = {};
            for (int32 i = 0; i < slotCount; ++i)
            {
                slotNames[i] = magic_enum::enum_name(static_cast<MaterialTextureSlot>(i)).data();
            }

            ImGui::SetNextItemWidth(130.f);
            if (ImGui::Combo("##slot", &current, slotNames, slotCount))
            {
                if (entry.textureSlots.find(current) == entry.textureSlots.end())
                {
                    entry.textureSlots[current] = texPath;
                    toRemoveSlot.push_back(slotIdx);
                    _ctx->_dirtyPassOverrides = true;
                }
            }
            ImGui::SameLine();

            char pathBuf[MAX_PATH] = {};
            strncpy_s(pathBuf, texPath.c_str(), sizeof(pathBuf) - 1);
            ImGui::SetNextItemWidth(200.f);
            if (ImGui::InputText("##path", pathBuf, sizeof(pathBuf)))
            {
                texPath = pathBuf;
                _ctx->_dirtyPassOverrides = true;
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
                    _ctx->_dirtyPassOverrides = true;
                }
            }
            ImGui::PopID();
        }

        for (int32 s : toRemoveSlot)
        {
            entry.textureSlots.erase(s);
        }

        ImGui::Spacing();
        int32 newSlot = 0;
        const char* slotNames[16] = {};

        for (int32 i = 0; i < slotCount; ++i)
        {
            slotNames[i] = magic_enum::enum_name(static_cast<MaterialTextureSlot>(i)).data();
        }

        ImGui::SetNextItemWidth(130.f);
        ImGui::Combo("##newslot", &newSlot, slotNames, slotCount);
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add Slot"))
        {
            if (entry.textureSlots.find(newSlot) == entry.textureSlots.end())
            {
                entry.textureSlots[newSlot] = "";
                _ctx->_dirtyPassOverrides = true;
            }
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
    static string Export_TextureToSlotFolder(const string& mapName, const string& modelName, MaterialTextureSlot slot, const string& sourceTexPath)
    {
        if (slot != Engine::MaterialTextureSlot::Normal)
        {
            return sourceTexPath;
        }

        fs::path srcPath = fs::path(_mapSavePath) / sourceTexPath;
        if (!fs::exists(srcPath))
        {
            return sourceTexPath; 
        }

        string slotName = magic_enum::enum_name(slot).data();
        fs::path destDir = fs::path(_mapSavePath) / "MapInstance" / mapName / "Textures" / slotName;
        
        if (!fs::exists(destDir))
        {
            fs::create_directories(destDir);
        }

        fs::path destPath = destDir / srcPath.filename();
        fs::copy_file(srcPath, destPath, fs::copy_options::overwrite_existing);

        return "MapInstance/" + mapName + "/Textures/" + slotName + "/" + destPath.filename().string();
    }

    void ModelOverrideEntry::ExportToNativeMaterial(const string& modelPath, const string& mapName) const
    {
        if (modelPath.empty()) return;

        string modelName = fs::path(modelPath).stem().string();
        fs::path matDir = fs::path(_materialsPath) / mapName;
        
        if (!fs::exists(matDir))
        {
            fs::create_directories(matDir);
        }

        fs::path matPath = matDir / (modelName + ".material.json");

        json root;
        json instJson;

        string assetName = modelName + "_Inst";
        instJson["assetName"] = assetName;
        
        instJson["editor_modelPath"] = modelPath; 
        instJson["editor_shaderType"] = shaderType;
        
        instJson["forcePassIndex"] = passIndex;
        
        instJson["cb"]["baseColorTint"] = {
            materialCB.baseColorTint.x, 
            materialCB.baseColorTint.y, 
            materialCB.baseColorTint.z, 
            materialCB.baseColorTint.w
        };
        instJson["cb"]["emissiveColor"] = {
            materialCB.emissiveColor.x, 
            materialCB.emissiveColor.y, 
            materialCB.emissiveColor.z, 
            materialCB.emissiveColor.w
        };
        instJson["cb"]["emissiveIntensity"] = materialCB.emissiveIntensity;
        instJson["cb"]["emissiveMaskMul"] = materialCB.emissiveMaskMul;
        instJson["cb"]["normalStrength"] = materialCB.normalStrength;
        instJson["cb"]["opacityMaskClipValue"] = materialCB.opacityMaskClipValue;
        instJson["cb"]["aoScale"] = materialCB.aoScale;
        instJson["cb"]["roughnessScale"] = materialCB.roughnessScale;
        instJson["cb"]["metalnessScale"] = materialCB.metalnessScale;
        instJson["cb"]["uvScale"] = { materialCB.uvScale.x, materialCB.uvScale.y };
        instJson["cb"]["uvOffset"] = { materialCB.uvOffset.x, materialCB.uvOffset.y };
        instJson["cb"]["uvRotation"] = materialCB.uvRotation;
        instJson["cb"]["materialMask"] = materialCB.materialMask;
        instJson["cb"]["ormAoSource"] = materialCB.ormAoSource;
        instJson["cb"]["ormRoughnessSource"] = materialCB.ormRoughnessSource;
        instJson["cb"]["ormMetalnessSource"] = materialCB.ormMetalnessSource;
        instJson["cb"]["opacityMaskSource"] = materialCB.opacityMaskSource;

        json overridesJson = json::object();
        json texturesJson = json::object();

        // 1. 기존 베이스 텍스처 정보 로드
        MaterialBuilder* builder = GAME->Get_AssetManager()->Get_MaterialBuilder();
        if (builder)
        {
            string bundleKey = modelName;
            transform(bundleKey.begin(), bundleKey.end(), bundleKey.begin(), ::tolower);

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

        // 2. MapEditor에서 오버라이드한 텍스처 덮어쓰기
        for (auto& pair : textureSlots)
        {
            int32 slotIdx = pair.first;
            string texPath = pair.second;

            if (texPath.empty()) continue;

            Engine::MaterialTextureSlot slotEnum = static_cast<Engine::MaterialTextureSlot>(slotIdx);
            string newRelPath = Export_TextureToSlotFolder(mapName, modelName, slotEnum, texPath);

            string slotNameStr = "";
            if (slotEnum == MaterialTextureSlot::BaseColor) slotNameStr = "baseColor";
            else if (slotEnum == MaterialTextureSlot::Normal) slotNameStr = "normal";
            else if (slotEnum == MaterialTextureSlot::ORM) slotNameStr = "orm";
            else if (slotEnum == MaterialTextureSlot::Emissive) slotNameStr = "emissive";
            else if (slotEnum == MaterialTextureSlot::OpacityMask) slotNameStr = "opacityMask";

            if (!slotNameStr.empty())
            {
                overridesJson[slotNameStr] = true;
                texturesJson[slotNameStr] = newRelPath;
            }
        }

        if (!overridesJson.empty())
        {
            instJson["overrides"] = overridesJson;
            instJson["textures"] = texturesJson;
        }
        else if (!texturesJson.empty())
        {
            // 오버라이드가 없더라도, 원본 텍스처 경로는 기록하여 Standalone 보장
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

Shared<PanelModelOverride> PanelModelOverride::Create(SceneContext* ctx)
{
    return make_shared<PanelModelOverride>(ctx);
}
