#include "MEMaterialApplicator.h"
#include "MEStaticMeshActor.h"
#include "MEInstancedMeshActor.h"

namespace
{
    bool HasForcedPassIndex(const MapEditor::ModelOverrideEntry& entry, int32 passIndex)
    {
        if (!entry.materialBindingsJson.is_array())
        {
            return false;
        }

        for (const json& binding : entry.materialBindingsJson)
        {
            if (!binding.is_object() ||
                !binding.contains("instance_override") ||
                !binding["instance_override"].is_object())
            {
                continue;
            }

            const json& instJson = binding["instance_override"];
            if (instJson.value("force_pass_index", -1) == passIndex ||
                instJson.value("forcePassIndex", -1) == passIndex)
            {
                return true;
            }
        }

        return false;
    }
}

HRESULT MEMaterialApplicator::Initialize(const Material_Desc& desc)
{
    _scene = desc.scene;
    return S_OK;
}

void MEMaterialApplicator::Apply_MaterialFromJson(const fs::path& modelPath, const string& bundleKey, Shared<ModelCom>& model)
{
    fs::path jsonPath = modelPath;
    jsonPath.replace_extension(L".json");

    ifstream ifs(jsonPath);
    if (!ifs.is_open())
    {
        return;
    }

    try
    {
        json modelJson = json::parse(ifs);
        for (auto& entry : modelJson)
        {
            if (entry.value("Type", "") != "StaticMesh")
            {
                continue;
            }

            if (!entry.contains("Properties") || !entry["Properties"].contains("StaticMaterials"))
            {
                break;
            }

            const auto& staticMats = entry["Properties"]["StaticMaterials"];
            for (uint32 i = 0; i < static_cast<uint32>(staticMats.size()); ++i)
            {
                const auto& mat = staticMats[i];
                if (!mat.contains("MaterialInterface"))
                {
                    continue;
                }

                const auto& mi = mat["MaterialInterface"];
                if (!mi.is_object())
                {
                    continue;
                }

                string objectPath = mi.value("ObjectPath", "");
                if (objectPath.empty())
                {
                    continue;
                }

                size_t dot = objectPath.find('.');
                string packagePath = (dot != string::npos) ? objectPath.substr(0, dot) : objectPath;
                size_t slash = packagePath.rfind('/');
                string miName = (slash != string::npos) ? packagePath.substr(slash + 1) : packagePath;

                model->Apply_MaterialByAssetName(i, bundleKey, miName);
            }
            break;
        }
    }
    catch (...) {}
}

void MEMaterialApplicator::Apply_PassOverrideToModel(ModelCom& model, const string& modelFilename, Vec3 worldPos, const string& fullModelPath)
{
    auto it = _scene->_modelOverrides.find(modelFilename);
    const ModelOverrideEntry* overrideEntry = (it != _scene->_modelOverrides.end()) ? &it->second : nullptr;

    uint32 matSize = static_cast<uint32>(model.Get_NumMaterials());
    for (uint32 i = 0; i < matSize; ++i)
    {
        auto mat = model.Get_Material(i);
        if (!mat)
        {
            continue;
        }

        mat->Set_ForcedPassIndex(-1);

        if (overrideEntry && overrideEntry->hasMaterialCB)
        {
            mat->Set_CB(overrideEntry->materialCB);
        }

        if (overrideEntry)
        {
            for (auto& [slotIdx, texPath] : overrideEntry->textureSlots)
            {
                if (texPath.empty())
                {
                    continue;
                }
                string fullPath = string(_mapSavePath) + texPath;
                auto slot = static_cast<MaterialTextureSlot>(slotIdx);
                mat->Override_TextureSlot(slot, 0, String::ToWString(fullPath));
            }

            if (HasForcedPassIndex(*overrideEntry, static_cast<int32>(VtxMeshPassIndex::LandscapeBlendPass)))
            {
                auto& cb = mat->Get_CB();
                cb.uvOffset = Vec2(worldPos.x, worldPos.z);

                string guid = fs::path(fullModelPath).parent_path().filename().string();
                if (!guid.empty())
                {
                    string guidJsonPath = string(_mapSavePath) + "OriginJson/" +
                        String::ToString(_scene->_modelFolder) + "/_Generated_/" + guid + ".json";

                    ifstream jsonFile(guidJsonPath);
                    if (jsonFile.is_open())
                    {
                        try
                        {
                            json j = json::parse(jsonFile);

                            string wmRef = j["Textures"]["Weightmap0"].get<string>();
                            size_t lastDot = wmRef.rfind('.');
                            if (lastDot != string::npos)
                            {
                                string wmName = wmRef.substr(lastDot + 1);
                                string weightmapPath = string(_mapSavePath) + "OriginJson/" +
                                    String::ToString(_scene->_modelFolder) + "/_Generated_/" + wmName + ".dds";
                                mat->Override_TextureSlot(static_cast<MaterialTextureSlot>(6), 0, String::ToWString(weightmapPath));
                            }

                            Vec4 layer1Mask = Vec4(1.f, 0.f, 0.f, 0.f);
                            Vec4 layer2Mask = Vec4(0.f, 0.f, 0.f, 0.f);

                            if (j.contains("Parameters") && j["Parameters"].contains("Colors"))
                            {
                                auto& colors = j["Parameters"]["Colors"];
                                if (colors.contains("LayerMask_Layer1"))
                                {
                                    auto& c = colors["LayerMask_Layer1"];
                                    layer1Mask = Vec4(c["R"].get<float>(), c["G"].get<float>(), c["B"].get<float>(), c["A"].get<float>());
                                }
                                if (colors.contains("LayerMask_Layer2"))
                                {
                                    auto& c = colors["LayerMask_Layer2"];
                                    layer2Mask = Vec4(c["R"].get<float>(), c["G"].get<float>(), c["B"].get<float>(), c["A"].get<float>());
                                }
                            }

                            cb.baseColorTint = layer1Mask;
                            cb.emissiveColor = layer2Mask;
                        }
                        catch (...) {}
                    }
                }
            }

            if (HasForcedPassIndex(*overrideEntry, static_cast<int32>(VtxMeshPassIndex::EclipseSpherePass)))
            {
                mat->Set_ForcedPassIndex(static_cast<int32>(VtxMeshPassIndex::EclipseSpherePass));
            }
        }
    }

    if (overrideEntry && overrideEntry->HasMaterialBindingsOverride())
    {
        const string modelPathForJson = fullModelPath.empty() ? modelFilename : fullModelPath;
        const string mapName = _scene ? String::ToString(_scene->_mapName) : string{};
        model.From_Json(overrideEntry->BuildMaterialJson(modelPathForJson, mapName));
    }

    if (overrideEntry && HasForcedPassIndex(*overrideEntry, static_cast<int32>(VtxMeshPassIndex::EclipseSpherePass)))
    {
        uint32 matSize2 = static_cast<uint32>(model.Get_NumMaterials());
        for (uint32 j = 0; j < matSize2; ++j)
        {
            auto mat = model.Get_Material(j);
            if (mat)
            {
                mat->Set_ForcedPassIndex(static_cast<int32>(VtxMeshPassIndex::EclipseSpherePass));
            }
        }
    }
}

void MEMaterialApplicator::Apply_PassOverrides()
{
    if (!_scene->_dirtyPassOverrides)
    {
        return;
    }

    _scene->_dirtyPassOverrides = false;

    for (auto& obj : _scene->_staticMeshes)
    {
        if (!obj->_renderHandle)
        {
            continue;
        }

        auto model = obj->_renderHandle->GetModel();
        if (!model)
        {
            continue;
        }

        const string modelFilename = String::ToString(fs::path(obj->_modelPath).filename().wstring());
        Apply_PassOverrideToModel(*model, modelFilename, obj->_position, String::ToString(obj->_modelPath));
    }

    for (auto& obj : _scene->_instancedMeshes)
    {
        if (obj->_instancedChunkHandles.empty())
        {
            continue;
        }

        auto model = obj->_instancedChunkHandles[0]->GetModel();
        if (!model)
        {
            continue;
        }

        const string modelFilename = String::ToString(fs::path(obj->_instancedModelPath).filename().wstring());
        Apply_PassOverrideToModel(*model, modelFilename, obj->_position, String::ToString(obj->_instancedModelPath));
    }
}

string MEMaterialApplicator::Resolve_BundleKey(const fs::path& modelPath) const
{
    string bundleKey = String::ToString(modelPath.parent_path().filename().wstring());
    transform(bundleKey.begin(), bundleKey.end(), bundleKey.begin(), ::tolower);
    return bundleKey;
}



Unique<MEMaterialApplicator> MEMaterialApplicator::Create(const Material_Desc& desc)
{
    auto instance = make_unique<MEMaterialApplicator>();
    if (FAILED(instance->Initialize(desc)))
    {
        return nullptr;
    }

    return instance;
}
