#include "MapLoader.h"
#include "GameInstance.h"
#include "MapStaticMesh.h"
#include "MapInstancedMesh.h"

HRESULT MapLoader::Load(const ComPtr<Device>& device, const ComPtr<Context>& context, const wstring& filePath, uint32 levelIndex, const wstring& layerTag)
{
    string pathA = String::ToString(filePath);
    ifstream ifs(pathA);
    if (!ifs.is_open())
        return E_FAIL;

    json root = json::parse(ifs);
    const json& mapData = root["MapData"];

    if (!mapData.contains("StaticMeshes"))
        return S_OK;

    unordered_map<string, Shared<ModelCom>> modelCache;
    string modelFolder = mapData.value("ModelFolder", string{});

    for (auto& e : mapData["StaticMeshes"])
    {
        string modelPathStr = e.value("ModelPath", string{});
        if (modelPathStr.empty())
            continue;

        MapStaticMesh::MAPSTATICMESH_DESC desc{};

        auto it = modelCache.find(modelPathStr);
        if (it != modelCache.end())
        {
            desc.model = it->second;
        }
        else
        {
            string fullPathA = "../../../Client/Bin/Resources/Map/" + modelFolder + "/" + modelPathStr;
            wstring fullPathW = String::ToWString(fullPathA);
            auto model = ModelCom::Create(device, context, ModelType::NonAnim, fullPathA.c_str(), Matrix::CreateScale(0.01f));
            if (!model)
                continue;
            modelCache[modelPathStr] = model;
            desc.model = model;
        }

        desc.modelPath = {};

        auto& pos = e["Position"];
        desc.position = Vec3(pos[0], pos[1], pos[2]);

        auto& rot = e["Rotation"];
        desc.rotation = Quat(rot[0], rot[1], rot[2], rot[3]);

        auto& scl = e["Scale"];
        desc.scale = Vec3(scl[0], scl[1], scl[2]);

        if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_MapStaticMesh"),
            levelIndex, layerTag, &desc)))
        {
            continue;
        }
    }

    if (mapData.contains("InstancedMeshes"))
    {
        unordered_map<string, Shared<ModelCom>> instancedModelCache;

        for (auto& e : mapData["InstancedMeshes"])
        {
            string modelPathStr = e.value("ModelPath", string{});
            if (modelPathStr.empty())
            {
                continue;
            }

            MapInstancedMesh::MAPINSTANCEDMESH_DESC desc{};

            auto it = instancedModelCache.find(modelPathStr);
            if (it != instancedModelCache.end())
            {
                desc.model = it->second;
            }
            else
            {
                string fullPathA = "../../../Client/Bin/Resources/Map/" + modelFolder + "/" + modelPathStr;
                auto model = ModelCom::Create(device, context, ModelType::NonAnim, fullPathA.c_str(), Matrix::CreateScale(0.01f));
                if (!model)
                {
                    continue;
                }

                instancedModelCache[modelPathStr] = model;
                desc.model = model;
            }

            if (e.contains("Instances"))
            {
                for (auto& inst : e["Instances"])
                {
                    InstanceTransform t{};
                    auto& p = inst["Position"];
                    t._position = { p[0], p[1], p[2] };
                    auto& r = inst["Rotation"];
                    t._rotation = { r[0], r[1], r[2], r[3] };
                    auto& s = inst["Scale"];
                    t._scale = { s[0], s[1], s[2] };
                    desc.transforms.push_back(t);
                }
            }

            if (desc.transforms.empty())
            {
                continue;
            }

            GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_MapInstancedMesh"),
                levelIndex, layerTag, &desc);
        }
    }


    return S_OK;
}
