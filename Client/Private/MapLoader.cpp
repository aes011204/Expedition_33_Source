#include "MapLoader.h"
#include "GameInstance.h"
#include "MapStaticMesh.h"

HRESULT MapLoader::Load(const wstring& filePath, uint32 levelIndex, const wstring& layerTag)
{
    string pathA = String::ToString(filePath);
    ifstream ifs(pathA);
    if (!ifs.is_open())
    {
        return E_FAIL;
    }

    json root = json::parse(ifs);
    const json& mapData = root["MapData"];

    if (!mapData.contains("StaticMeshes"))
        return S_OK;

    for (auto& e : mapData["StaticMeshes"])
    {
        MapStaticMesh::MAPSTATICMESH_DESC desc{};

        string modelPathA = "../../../Client/Bin/Resources/Map/" + e.value("ModelPath", string{});
        desc.modelPath = String::ToWString(modelPathA);

        if (e.value("ModelPath", string{}).empty())
        {
            continue;
        }

        auto& pos = e["Position"];
        desc.position = Vec3(pos[0], pos[1], pos[2]);

        auto& rot = e["Rotation"];
        desc.rotation = Quat(rot[0], rot[1], rot[2], rot[3]);

        auto& scl = e["Scale"];
        desc.scale = Vec3(scl[0], scl[1], scl[2]);

        if (FAILED(GAME->Add_GameObject(
            levelIndex,
            TEXT("GameObject_MapStaticMesh"),
            levelIndex,
            layerTag,
            &desc)))
        {
            continue;
        }
    }

    return S_OK;
}
