#include "ResourceLoader.h"

#include "BackGround.h"
#include "Body_Player.h"
#include "ColliderCom.h"
#include "Component.h"
#include "ComputeSpriteEmitter.h"
#include "ComputeShaderCom.h"
#include "FreeCamera.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "Particle_Types.h"
#include "ShaderCom.h"
#include "TargetCamera.h"
#include "Texture.h"
#include "VIBufferCom_ComputePointParticle.h"
#include "VIBuffer_Cube.h"
#include "VIBuffer_Rect.h"
#include "VIBuffer_Terrain.h"

namespace fs = std::filesystem;
namespace
{
    string Get_TableLabel(const wstring& tablePath)
    {
        return fs::path(tablePath).filename().string();
    }

    void Log_TableWarning(const char* action, const wstring& tablePath)
    {
        LOG_WARN("Skipping DT table {}: {}", action, Get_TableLabel(tablePath));
    }

    string FormatDuration(chrono::milliseconds duration)
    {
        const auto ms = duration.count();

        if (ms >= 1000)
        {
            ostringstream oss;
            oss.setf(ios_base::fixed);
            oss.precision(ms < 10000 ? 1 : 0);
            oss << static_cast<double>(ms) / 1000.0 << " seconds";
            return oss.str();
        }

        return to_string(ms) + " milliseconds";
    }
}

class ResourceLoaderPathHelpers final
{
public:
    static wstring Normalize_PathText(const wstring& path)
    {
        wstring normalized = path;
        for (wchar_t& ch : normalized)
        {
            if (L'/' == ch)
                ch = L'\\';
            else
                ch = static_cast<wchar_t>(towlower(ch));
        }

        return normalized;
    }

    static bool StartsWith(const wstring& value, const wstring& prefix)
    {
        return value.size() >= prefix.size() && 0 == value.compare(0, prefix.size(), prefix);
    }
};

ResourceLoader::ResourceLoader(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT ResourceLoader::Initialize()
{
    if (nullptr == _device || nullptr == _context)
        return E_FAIL;

    return S_OK;
}

HRESULT ResourceLoader::Load_TextureTable(const wstring& strTablePath)
{
    json JsonRoot = {};
    if (!Load_JsonFile(strTablePath, JsonRoot))
    {
        Log_TableWarning("read failed", strTablePath);
        return S_OK;
    }

    if (JsonRoot.contains("DT_Texture"))
        return Load_Textures(JsonRoot["DT_Texture"]);

    if (JsonRoot.contains("Texture"))
        return Load_Textures(JsonRoot["Texture"]);

    return S_OK;
}

HRESULT ResourceLoader::Load_ShaderTable(const wstring& strTablePath)
{
    json JsonRoot = {};
    if (!Load_JsonFile(strTablePath, JsonRoot))
    {
        Log_TableWarning("read failed", strTablePath);
        return S_OK;
    }

    if (JsonRoot.contains("DT_Shader"))
        return Load_Shaders(JsonRoot["DT_Shader"]);

    if (JsonRoot.contains("ShaderCom"))
        return Load_Shaders(JsonRoot["ShaderCom"]);

    return S_OK;
}

HRESULT ResourceLoader::Load_ModelTable(const wstring& strTablePath)
{
    vector<FLoadJob> LoadJobs;
    if (FAILED(Build_ModelJobs(strTablePath, LoadJobs)))
    {
        Log_TableWarning("build failed", strTablePath);
        return S_OK;
    }

    for (const FLoadJob& LoadJob : LoadJobs)
    {
        if (FAILED(Register_LoadJob(LoadJob)))
        {
            LOG_WARN("Skipping DT register failure in {}", Get_TableLabel(strTablePath));
            continue;
        }
    }

    return S_OK;
}

HRESULT ResourceLoader::Load_GameObjectTable(const wstring& strTablePath)
{
    vector<FLoadJob> LoadJobs;
    if (FAILED(Build_GameObjectJobs(strTablePath, LoadJobs)))
    {
        Log_TableWarning("build failed", strTablePath);
        return S_OK;
    }

    for (const FLoadJob& LoadJob : LoadJobs)
    {
        if (FAILED(Register_LoadJob(LoadJob)))
        {
            LOG_WARN("Skipping DT register failure in {}", Get_TableLabel(strTablePath));
            continue;
        }
    }

    return S_OK;
}

HRESULT ResourceLoader::Load_StaticLevelTable(const wstring& strTablePath)
{
    vector<FLoadJob> LoadJobs;
    if (FAILED(Build_StaticLevelJobs(strTablePath, LoadJobs)))
    {
        Log_TableWarning("build failed", strTablePath);
        return S_OK;
    }

    for (const FLoadJob& LoadJob : LoadJobs)
    {
        if (FAILED(Register_LoadJob(LoadJob)))
        {
            LOG_WARN("Skipping DT register failure in {}", Get_TableLabel(strTablePath));
            continue;
        }
    }

    return S_OK;
}

HRESULT ResourceLoader::Load_LevelTables(LevelType eLevel, const wstring& strJsonRootPath)
{
    vector<FLoadJob> LoadJobs;
    const fs::path jsonRootPath = strJsonRootPath.empty()
        ? fs::path(Get_DefaultJsonRootPath())
        : fs::path(Resolve_RuntimePath(strJsonRootPath));

    if (FAILED(Build_TextureJobs((jsonRootPath / L"DT_Texture.json").wstring(), LoadJobs)))
        Log_TableWarning("build failed", (jsonRootPath / L"DT_Texture.json").wstring());

    if (FAILED(Build_ShaderJobs((jsonRootPath / L"DT_Shader.json").wstring(), LoadJobs)))
        Log_TableWarning("build failed", (jsonRootPath / L"DT_Shader.json").wstring());

    if (FAILED(Build_ComponentJobs((jsonRootPath / L"DT_Component.json").wstring(), LoadJobs)))
        Log_TableWarning("build failed", (jsonRootPath / L"DT_Component.json").wstring());

    if (FAILED(Build_StaticLevelJobs((jsonRootPath / L"DT_StaticLevel.json").wstring(), LoadJobs)))
        Log_TableWarning("build failed", (jsonRootPath / L"DT_StaticLevel.json").wstring());

    if (FAILED(Build_ModelJobs((jsonRootPath / L"DT_Model.json").wstring(), LoadJobs)))
        Log_TableWarning("build failed", (jsonRootPath / L"DT_Model.json").wstring());

    if (FAILED(Build_GameObjectJobs((jsonRootPath / L"DT_GameObject.json").wstring(), LoadJobs)))
        Log_TableWarning("build failed", (jsonRootPath / L"DT_GameObject.json").wstring());

    const uint32 iTargetLevel = ETOI(eLevel);
    for (const FLoadJob& LoadJob : LoadJobs)
    {
        if (LoadJob.iLevelIndex != iTargetLevel)
            continue;

        if (FAILED(Register_LoadJob(LoadJob)))
        {
            LOG_WARN("Skipping DT register failure for level {}", iTargetLevel);
            continue;
        }
    }

    return S_OK;
}

HRESULT ResourceLoader::Build_TextureJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs)
{
    json JsonRoot = {};
    if (!Load_JsonFile(strTablePath, JsonRoot))
        return E_FAIL;

    const json* pTextureArray = nullptr;
    if (JsonRoot.contains("DT_Texture"))
        pTextureArray = &JsonRoot["DT_Texture"];
    else if (JsonRoot.contains("Texture"))
        pTextureArray = &JsonRoot["Texture"];

    if (nullptr == pTextureArray || !pTextureArray->is_array())
        return S_OK;

    for (const json& JsonItem : *pTextureArray)
    {
        const string strId = Read_String(JsonItem, "Id", "id");
        const string strPath = Read_String(JsonItem, "Path", "path");
        const string strLevel = Read_String(JsonItem, "Level", "level", "Static");
        const uint32 iCount = Read_UInt(JsonItem, "Count", "count", 1);

        if (strId.empty() || strPath.empty())
            continue;

        FLoadJob LoadJob = {};
        LoadJob.eType = ELoadJobType::TextureCreate;
        LoadJob.iLevelIndex = Get_LevelIndex_FromString(strLevel);
        LoadJob.strPrototypeTag = String::ToWString(strId);
        LoadJob.strPath = Resolve_RuntimePath(String::ToWString(strPath));
        LoadJob.strSourceTable = "DT_Texture";
        LoadJob.iCount = iCount;

        LoadJobs.push_back(LoadJob);
    }

    return S_OK;
}

HRESULT ResourceLoader::Build_ShaderJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs)
{
    json JsonRoot = {};
    if (!Load_JsonFile(strTablePath, JsonRoot))
        return E_FAIL;

    const json* pShaderArray = nullptr;
    if (JsonRoot.contains("DT_Shader"))
        pShaderArray = &JsonRoot["DT_Shader"];
    else if (JsonRoot.contains("ShaderCom"))
        pShaderArray = &JsonRoot["ShaderCom"];

    if (nullptr == pShaderArray || !pShaderArray->is_array())
        return S_OK;

    for (const json& JsonItem : *pShaderArray)
    {
        const string strId = Read_String(JsonItem, "Id", "id");
        const string strPath = Read_String(JsonItem, "Path", "path");
        const string strLevel = Read_String(JsonItem, "Level", "level", "Static");
        const string strInputLayout = Read_String(JsonItem, "Type", "type", "VTXTEX");
        const string strEntryPoint = Read_String(JsonItem, "EntryPoint", "entryPoint", "CS_Main");

        if (strId.empty() || strPath.empty())
            continue;

        FLoadJob LoadJob = {};
        LoadJob.eType = ("Compute" == strInputLayout || "COMPUTE" == strInputLayout)
            ? ELoadJobType::ComputeShader
            : ELoadJobType::Shader;
        LoadJob.iLevelIndex = Get_LevelIndex_FromString(strLevel);
        LoadJob.strPrototypeTag = String::ToWString(strId);
        LoadJob.strPath = Resolve_RuntimePath(String::ToWString(strPath));
        LoadJob.strSourceTable = "DT_Shader";
        LoadJob.strInputLayout = strInputLayout;
        LoadJob.strEntryPoint = strEntryPoint;
        LoadJob.iCount = 1;

        LoadJobs.push_back(LoadJob);
    }

    return S_OK;
}

HRESULT ResourceLoader::Build_ModelJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs)
{
    json JsonRoot = {};
    if (!Load_JsonFile(strTablePath, JsonRoot))
        return E_FAIL;

    const json* pModelArray = nullptr;
    if (JsonRoot.contains("DT_Model"))
        pModelArray = &JsonRoot["DT_Model"];
    else if (JsonRoot.contains("ModelCom"))
        pModelArray = &JsonRoot["ModelCom"];

    if (nullptr == pModelArray || !pModelArray->is_array())
        return S_OK;

    for (const json& JsonItem : *pModelArray)
    {
        const string strId = Read_String(JsonItem, "Id", "id");
        const string strPath = Read_String(JsonItem, "Path", "path");
        const string strLevel = Read_String(JsonItem, "Level", "level", "Static");

        if (strId.empty() || strPath.empty())
            continue;

        FLoadJob LoadJob = {};
        LoadJob.eType = ELoadJobType::Model;
        LoadJob.iLevelIndex = Get_LevelIndex_FromString(strLevel);
        LoadJob.strPrototypeTag = String::ToWString(strId);
        LoadJob.strPath = Resolve_RuntimePath(String::ToWString(strPath));
        LoadJob.strSourceTable = "DT_Model";
        LoadJob.strModelType = Read_String(JsonItem, "ModelType", "modelType", "NONANIM");
        LoadJob.fScaleX = Read_Float(JsonItem, "ScaleX", "scaleX", 1.f);
        LoadJob.fScaleY = Read_Float(JsonItem, "ScaleY", "scaleY", 1.f);
        LoadJob.fScaleZ = Read_Float(JsonItem, "ScaleZ", "scaleZ", 1.f);
        LoadJob.fRotX = Read_Float(JsonItem, "RotX", "rotX", 0.f);
        LoadJob.fRotY = Read_Float(JsonItem, "RotY", "rotY", 0.f);
        LoadJob.fRotZ = Read_Float(JsonItem, "RotZ", "rotZ", 0.f);
        LoadJob.iCount = 1;

        LoadJobs.push_back(LoadJob);
    }

    return S_OK;
}

HRESULT ResourceLoader::Build_GameObjectJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs)
{
    json JsonRoot = {};
    if (!Load_JsonFile(strTablePath, JsonRoot))
        return E_FAIL;

    const json* pGameObjectArray = nullptr;
    if (JsonRoot.contains("DT_GameObject"))
        pGameObjectArray = &JsonRoot["DT_GameObject"];
    else if (JsonRoot.contains("GameObject"))
        pGameObjectArray = &JsonRoot["GameObject"];

    if (nullptr == pGameObjectArray || !pGameObjectArray->is_array())
        return S_OK;

    for (const json& JsonItem : *pGameObjectArray)
    {
        const string strId = Read_String(JsonItem, "Id", "id");
        const string strLevel = Read_String(JsonItem, "Level", "level", "Static");
        const string strClass = Read_String(JsonItem, "Class", "class");

        if (strId.empty() || strClass.empty())
            continue;

        FLoadJob LoadJob = {};
        LoadJob.eType = ELoadJobType::GameObject;
        LoadJob.iLevelIndex = Get_LevelIndex_FromString(strLevel);
        LoadJob.strPrototypeTag = String::ToWString(strId);
        LoadJob.strSourceTable = "DT_GameObject";
        LoadJob.strClassName = strClass;
        LoadJob.iCount = 1;

        LoadJobs.push_back(LoadJob);
    }

    return S_OK;
}

HRESULT ResourceLoader::Build_ComponentJobs(const wstring& strTablePath, vector<FLoadJob>& loadJobs)
{
    json root = {};
    if (!Load_JsonFile(strTablePath, root))
        return E_FAIL;

    const json* componentArray = nullptr;
    if (root.contains("DT_Component"))
        componentArray = &root["DT_Component"];
    else if (root.contains("Component"))
        componentArray = &root["Component"];

    if (nullptr == componentArray || !componentArray->is_array())
        return S_OK;

    for (const json& item : *componentArray)
    {
        const string id = Read_String(item, "Id", "id");
        const string level = Read_String(item, "Level", "level", "Static");

        string componentClass = Read_String(item, "Class", "class");
        if (componentClass.empty())
            componentClass = Read_String(item, "ComponentClass", "componentClass");

        if (id.empty() || componentClass.empty())
            continue;

        FLoadJob loadJob{};
        loadJob.eType = ELoadJobType::Component;
        loadJob.iLevelIndex = Get_LevelIndex_FromString(level);
        loadJob.strPrototypeTag = String::ToWString(id);
        loadJob.strSourceTable = "DT_Component";
        loadJob.strClassName = componentClass;

        loadJobs.push_back(loadJob);
    }

    return S_OK;
}

HRESULT ResourceLoader::Build_StaticLevelJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs)
{
    json JsonRoot = {};
    if (!Load_JsonFile(strTablePath, JsonRoot))
        return E_FAIL;

    if (!JsonRoot.contains("DT_StaticLevel") || !JsonRoot["DT_StaticLevel"].is_array())
        return S_OK;

    for (const json& JsonItem : JsonRoot["DT_StaticLevel"])
    {
        const string strId = Read_String(JsonItem, "Id", "id");
        const string strLevel = Read_String(JsonItem, "Level", "level", "Static");
        const string strResourceType = Read_String(JsonItem, "ResourceType", "resourceType");

        if (strId.empty())
            continue;

        FLoadJob LoadJob = {};
        LoadJob.eType = ELoadJobType::StaticLevel;
        LoadJob.iLevelIndex = Get_LevelIndex_FromString(strLevel);
        LoadJob.strPrototypeTag = String::ToWString(strId);
        LoadJob.strSourceTable = "DT_StaticLevel";
        LoadJob.strResourceType = strResourceType;
        LoadJob.iCount = Read_UInt(JsonItem, "Count", "count", 1);

        LoadJobs.push_back(LoadJob);
    }

    return S_OK;
}

HRESULT ResourceLoader::Build_LevelJobs(LevelType levelType, vector<FLoadJob>& outJobs, const wstring& strJsonRootPath)
{
    outJobs.clear();
    const auto start = chrono::steady_clock::now();

    const fs::path jsonRootPath = strJsonRootPath.empty()
        ? fs::path(Get_DefaultJsonRootPath())
        : fs::path(Resolve_RuntimePath(strJsonRootPath));

    auto buildTable = [&](const wchar_t* fileName, auto buildFunc)
    {
        const wstring tablePath = (jsonRootPath / fileName).wstring();

        const HRESULT hr = (this->*buildFunc)(tablePath, outJobs);

        if (FAILED(hr))
        {
            Log_TableWarning("build failed", tablePath);
            return;
        }
    };

    buildTable(L"DT_Texture.json", &ResourceLoader::Build_TextureJobs);
    buildTable(L"DT_Shader.json", &ResourceLoader::Build_ShaderJobs);
    buildTable(L"DT_Component.json", &ResourceLoader::Build_ComponentJobs);
    buildTable(L"DT_StaticLevel.json", &ResourceLoader::Build_StaticLevelJobs);
    buildTable(L"DT_Model.json", &ResourceLoader::Build_ModelJobs);
    buildTable(L"DT_GameObject.json", &ResourceLoader::Build_GameObjectJobs);

    const uint32 targetLevel = ETOI(levelType);

    vector<FLoadJob> filteredJobs;
    filteredJobs.reserve(outJobs.size());

    for (const FLoadJob& job : outJobs)
    {
        if (job.iLevelIndex == targetLevel)
            filteredJobs.push_back(job);
    }

    outJobs = std::move(filteredJobs);

    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - start);

    LOG_INFO("Built level jobs. level={}, jobCount={}, elapsed={}",
        targetLevel,
        static_cast<int>(outJobs.size()),
        FormatDuration(elapsed));

    return S_OK;
}

ResourceLoader::FInputLayoutInfo ResourceLoader::Get_InputLayout(const string& strName)
{
    if ("VTXNORTEX" == strName || "NorTex" == strName || "VtxNorTex" == strName)
        return { VTXNORTEX::Elements, VTXNORTEX::numElements, ShaderType::VtxNorTex };

    if ("VTXMESH" == strName || "Mesh" == strName || "VtxMesh" == strName)
        return { VTXMESH::Elements, VTXMESH::numElements, ShaderType::VtxMesh };

    if ("VTXANIMMESH" == strName || "AnimMesh" == strName || "VtxAnimMesh" == strName)
        return { VTXANIMMESH::Elements, VTXANIMMESH::numElements, ShaderType::VtxAnimMesh };

    if ("VTXCUBE" == strName || "Cube" == strName || "VtxCube" == strName)
        return { VTXCUBE::Elements, VTXCUBE::numElements, ShaderType::VtxCube };

    if ("ParticleInstance" == strName || "PARTICLEINSTANCE" == strName)
        return { ParticleInstanceLayout::Elements, ParticleInstanceLayout::numElements, ShaderType::BillboardParticle };

    if ("TrailInstance" == strName || "TRAILINSTANCE" == strName)
        return { TrailInstanceLayout::Elements, TrailInstanceLayout::numElements, ShaderType::BillboardParticle };

    // Mesh Instance (roughly)
    if ("VTXMESHINST" == strName || "VtxMeshInst" == strName || "vtxmeshinst" == strName)
        return { VtxMeshInstanceLayout::Elements, VtxMeshInstanceLayout::numElements, ShaderType::VtxInstanceMesh };

    // Mesh Instance (for real)
    if ("VTXINSTMESHEXACT" == strName || "VtxInstMeshExact" == strName)
        return { VtxMeshInstanceExactLayout::Elements, VtxMeshInstanceExactLayout::numElements, ShaderType::VtxInstanceMeshExact };

    return { VTXTEX::Elements, VTXTEX::numElements, ShaderType::VtxTex };
}

HRESULT ResourceLoader::Load_Textures(const json& JsonData)
{
    if (!JsonData.is_array())
        return S_OK;

    for (const json& JsonItem : JsonData)
    {
        const string strId = Read_String(JsonItem, "Id", "id");
        const string strPath = Read_String(JsonItem, "Path", "path");
        const string strLevel = Read_String(JsonItem, "Level", "level", "Static");
        const uint32 iCount = Read_UInt(JsonItem, "Count", "count", 1);

        if (strId.empty() || strPath.empty())
            continue;

        const wstring resolvedPath = Resolve_RuntimePath(String::ToWString(strPath));
        Shared<Texture> pTexture = Texture::Create(_device, _context, resolvedPath.c_str(), iCount);
        if (nullptr == pTexture)
        {
            LOG_WARN("Skipping Texture prototype registration. id='{}', path='{}'", strId, strPath);
            continue;
        }

        if (FAILED(GAME->Add_Prototype(Get_LevelIndex_FromString(strLevel), String::ToWString(strId), pTexture)))
        {
            LOG_WARN("Skipping Texture Add_Prototype failure. id='{}', level='{}'", strId, strLevel);
            continue;
        }
    }

    return S_OK;
}

HRESULT ResourceLoader::Load_Shaders(const json& JsonData)
{
    if (!JsonData.is_array())
        return S_OK;

    for (const json& JsonItem : JsonData)
    {
        const string strId = Read_String(JsonItem, "Id", "id");
        const string strPath = Read_String(JsonItem, "Path", "path");
        const string strLevel = Read_String(JsonItem, "Level", "level", "Static");
        const string strInputLayout = Read_String(JsonItem, "Type", "type", "VTXTEX");
        const string strEntryPoint = Read_String(JsonItem, "EntryPoint", "entryPoint", "CS_Main");

        if (strId.empty() || strPath.empty())
            continue;

        const wstring resolvedPath = Resolve_RuntimePath(String::ToWString(strPath));
        if ("Compute" == strInputLayout || "COMPUTE" == strInputLayout)
        {
            Shared<ComputeShaderCom> computeShader =
                ComputeShaderCom::Create(_device, _context, resolvedPath.c_str(), strEntryPoint.c_str());
            if (nullptr == computeShader)
            {
                LOG_WARN("Skipping ComputeShader prototype registration. id='{}', path='{}', entry='{}'", strId, strPath, strEntryPoint);
                continue;
            }

            if (FAILED(GAME->Add_Prototype(Get_LevelIndex_FromString(strLevel), String::ToWString(strId), computeShader)))
            {
                LOG_WARN("Skipping ComputeShader Add_Prototype failure. id='{}', level='{}'", strId, strLevel);
                continue;
            }

            continue;
        }

        const FInputLayoutInfo InputLayout = Get_InputLayout(strInputLayout);
        Shared<ShaderCom> pShader = ShaderCom::Create(
            _device, _context, resolvedPath.c_str(), InputLayout.pElements, InputLayout.iNumElements, InputLayout.eShaderType);
        if (nullptr == pShader)
        {
            LOG_WARN("Skipping Shader prototype registration. id='{}', path='{}', type='{}'", strId, strPath, strInputLayout);
            continue;
        }

        if (FAILED(GAME->Add_Prototype(Get_LevelIndex_FromString(strLevel), String::ToWString(strId), pShader)))
        {
            LOG_WARN("Skipping Shader Add_Prototype failure. id='{}', level='{}'", strId, strLevel);
            continue;
        }
    }

    return S_OK;
}

HRESULT ResourceLoader::Register_LoadJob(const FLoadJob& loadJob)
{
    if (GAME->Has_Prototype(loadJob.iLevelIndex, loadJob.strPrototypeTag))
        return S_OK;

    Shared<Base> prototype = nullptr;

    CHECK_FAILED(Create_LoadJobPrototype(loadJob, prototype), E_FAIL);
    CHECK_FAILED(Commit_LoadJobPrototype(loadJob, prototype), E_FAIL);

    return S_OK;
}

bool ResourceLoader::Can_CreatePrototypeOnWorker(const FLoadJob& loadJob)
{
    switch (loadJob.eType)
    {
    case ELoadJobType::TextureCreate:
    case ELoadJobType::Shader:
    case ELoadJobType::ComputeShader:
    case ELoadJobType::Model:
        return true;

    default:
        return false;
    }
}

HRESULT ResourceLoader::Create_LoadJobPrototype(const FLoadJob& loadJob, Shared<Base>& outPrototype)
{
    outPrototype = nullptr;

    switch (loadJob.eType)
    {
    case ELoadJobType::TextureCreate:
    {
        outPrototype = Texture::Create(_device, _context, loadJob.strPath.c_str(), loadJob.iCount);
        if (nullptr == outPrototype)
            Log_LoadJobPrototypeFailure(loadJob, "create returned null");
        return S_OK;
    }

    case ELoadJobType::Shader:
    {
        const FInputLayoutInfo inputLayout = Get_InputLayout(loadJob.strInputLayout);

        outPrototype = ShaderCom::Create(
            _device,
            _context,
            loadJob.strPath.c_str(),
            inputLayout.pElements,
            inputLayout.iNumElements,
            inputLayout.eShaderType);

        if (nullptr == outPrototype)
            Log_LoadJobPrototypeFailure(loadJob, "create returned null");
        return S_OK;
    }

    case ELoadJobType::ComputeShader:
    {
        outPrototype = ComputeShaderCom::Create(
            _device,
            _context,
            loadJob.strPath.c_str(),
            loadJob.strEntryPoint.c_str());

        if (nullptr == outPrototype)
            Log_LoadJobPrototypeFailure(loadJob, "create returned null");
        return S_OK;
    }

    case ELoadJobType::Model:
    {
        const ModelType modelType =
            ("ANIM" == loadJob.strModelType || "Anim" == loadJob.strModelType)
            ? ModelType::Anim
            : ModelType::NonAnim;

        const XMMATRIX preLocalTransform =
            XMMatrixScaling(loadJob.fScaleX, loadJob.fScaleY, loadJob.fScaleZ) *
            XMMatrixRotationX(XMConvertToRadians(loadJob.fRotX)) *
            XMMatrixRotationY(XMConvertToRadians(loadJob.fRotY)) *
            XMMatrixRotationZ(XMConvertToRadians(loadJob.fRotZ));

        const string modelPath = String::ToString(loadJob.strPath);

        outPrototype = ModelCom::Create(
            _device,
            _context,
            modelType,
            modelPath.c_str(),
            preLocalTransform);

        if (nullptr == outPrototype)
            Log_LoadJobPrototypeFailure(loadJob, "create returned null");
        return S_OK;
    }

    case ELoadJobType::GameObject:
    {
        Shared<GameObject> gameObject = GAME->Create_GameObjectPrototype(loadJob.strClassName);
        outPrototype = static_pointer_cast<Base>(gameObject);
        if (nullptr == outPrototype)
            Log_LoadJobPrototypeFailure(loadJob, "create returned null");
        return S_OK;
    }

    case ELoadJobType::Component:
    {
        if (loadJob.strClassName == "ColliderCom")
        {
            Shared<ColliderCom> collider =
                ColliderCom::Create(_device, _context, Get_Shape_FromPrototypeTag(loadJob.strPrototypeTag));

            outPrototype = static_pointer_cast<Base>(collider);
            if (nullptr == outPrototype)
                Log_LoadJobPrototypeFailure(loadJob, "create returned null");
            return S_OK;
        }

        Shared<Component> component = GAME->Create_ComponentPrototype(loadJob.strClassName);
        outPrototype = static_pointer_cast<Base>(component);
        if (nullptr == outPrototype)
            Log_LoadJobPrototypeFailure(loadJob, "create returned null");
        return S_OK;
    }

    case ELoadJobType::StaticLevel:
    {
        outPrototype = Create_StaticPrototype(loadJob);
        if (nullptr == outPrototype)
            Log_LoadJobPrototypeFailure(loadJob, "create returned null");
        return S_OK;
    }

    default:
        Log_LoadJobPrototypeFailure(loadJob, "unsupported job type");
        return S_OK;
    }
}

HRESULT ResourceLoader::Commit_LoadJobPrototype(const FLoadJob& loadJob, const Shared<Base>& prototype)
{
    // Loading UI가 미리 만든 Shader_UI / VIBuffer_Rect 같은 중복 등록 방지.
    if (GAME->Has_Prototype(loadJob.iLevelIndex, loadJob.strPrototypeTag))
        return S_OK;

    if (nullptr == prototype)
    {
        return S_OK;
    }

    if (FAILED(GAME->Add_Prototype(loadJob.iLevelIndex, loadJob.strPrototypeTag, prototype)))
    {
        LOG_WARN(
            "Skipping Add_Prototype failure. id='{}', type={}, level={}",
            String::ToString(loadJob.strPrototypeTag),
            string(magic_enum::enum_name(loadJob.eType)),
            loadJob.iLevelIndex);

        return E_FAIL;
    }

    return S_OK;
}

void ResourceLoader::Log_LoadJobPrototypeFailure(const FLoadJob& loadJob, string_view reason)
{
    LOG_WARN(
        "Prototype load job failed. reason='{}', table='{}', id='{}', jobType={}, level={}, path='{}', inputLayout='{}', entryPoint='{}', modelType='{}', className='{}', resourceType='{}'",
        reason,
        loadJob.strSourceTable,
        String::ToString(loadJob.strPrototypeTag),
        string(magic_enum::enum_name(loadJob.eType)),
        loadJob.iLevelIndex,
        String::ToString(loadJob.strPath),
        loadJob.strInputLayout,
        loadJob.strEntryPoint,
        loadJob.strModelType,
        loadJob.strClassName,
        loadJob.strResourceType
    );
}

Shared<Base> ResourceLoader::Create_StaticPrototype(const FLoadJob& LoadJob)
{
    if ("VIBuffer" != LoadJob.strResourceType)
        return nullptr;

    if (TEXT("VIBuffer_Terrain") == LoadJob.strPrototypeTag)
    {
        const wstring heightMapPath = Resolve_RuntimePath(L"../Bin/Resources/Textures/Terrain/Height.bmp");
        return VIBuffer_Terrain::Create(_device, _context, heightMapPath.c_str());
    }

    if (TEXT("VIBuffer_Cube") == LoadJob.strPrototypeTag)
        return VIBuffer_Cube::Create(_device, _context);

    if (TEXT("VIBuffer_Rect") == LoadJob.strPrototypeTag)
        return VIBuffer_Rect::Create(_device, _context);

    return nullptr;
}

bool ResourceLoader::Load_JsonFile(const wstring& strTablePath, json& JsonRoot)
{
    try
    {
        ifstream file(strTablePath);
        if (!file.is_open())
            return false;

        file >> JsonRoot;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

string ResourceLoader::Read_String(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, const string& strDefault)
{
    if (JsonItem.contains(strUpperKey))
        return JsonItem.value(strUpperKey, strDefault);

    if (JsonItem.contains(strLowerKey))
        return JsonItem.value(strLowerKey, strDefault);

    return strDefault;
}

uint32 ResourceLoader::Read_UInt(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, uint32 iDefault)
{
    if (JsonItem.contains(strUpperKey))
        return JsonItem.value(strUpperKey, iDefault);

    if (JsonItem.contains(strLowerKey))
        return JsonItem.value(strLowerKey, iDefault);

    return iDefault;
}

float ResourceLoader::Read_Float(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, float fDefault)
{
    if (JsonItem.contains(strUpperKey))
        return JsonItem.value(strUpperKey, fDefault);

    if (JsonItem.contains(strLowerKey))
        return JsonItem.value(strLowerKey, fDefault);

    return fDefault;
}

CollisionShape ResourceLoader::Get_Shape_FromPrototypeTag(const wstring& prototypeTag)
{
    // 끝 문자열로 판별
    if (prototypeTag.ends_with(L"_AABB"))
        return CollisionShape::AABB;

    if (prototypeTag.ends_with(L"_OBB"))
        return CollisionShape::OBB;

    if (prototypeTag.ends_with(L"_Sphere"))
        return CollisionShape::Sphere;

    return CollisionShape::AABB;
}

wstring ResourceLoader::Make_PrototypeTag(const string& strId, const wstring& strPrefix)
{
    const wstring strTag = String::ToWString(strId);
    if (0 == strTag.find(L"Prototype_"))
        return strTag;

    return strPrefix + strTag;
}

uint32 ResourceLoader::Get_LevelIndex_FromString(const string& strLevel)
{
    if ("MainTitle" == strLevel) // || "MainTitle" == strLevel)
        return ETOI(LevelType::MainTitle);

    if ("GamePlay" == strLevel || "Gameplay" == strLevel || "GamePlay" == strLevel)
        return ETOI(LevelType::GamePlay);

    if ("MapTest" == strLevel)
        return ETOI(LevelType::MapTest);

    if ("Stage01" == strLevel)
        return ETOI(LevelType::Stage01);

    if ("Loading" == strLevel) // || "Loading" == strLevel)
        return ETOI(LevelType::Loading);

    return ETOI(LevelType::Static);
}

wstring ResourceLoader::Get_DefaultJsonRootPath()
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
        return {};

    return (fs::path(assetRoot) / L"Data" / L"json").wstring();
}

wstring ResourceLoader::Resolve_RuntimePath(const wstring& path)
{
    if (path.empty())
        return {};

    const fs::path inputPath(path);
    if (inputPath.is_absolute())
        return inputPath.lexically_normal().wstring();

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
        return fs::absolute(inputPath).lexically_normal().wstring();

    const fs::path assetRootPath(assetRoot);
    const fs::path clientBinPath = assetRootPath.parent_path();
    const wstring normalized = ResourceLoaderPathHelpers::Normalize_PathText(path);

    const wstring resourcesRootPrefix = L"..\\resources\\";
    const wstring resourcesRootExact = L"..\\resources";
    const wstring resourcesPrefix = L"..\\bin\\resources\\";
    const wstring shaderPrefix = L"..\\bin\\shaderfiles\\";
    const wstring resourcesExact = L"..\\bin\\resources";
    const wstring shaderExact = L"..\\bin\\shaderfiles";

    if (normalized == resourcesRootExact)
        return assetRootPath.lexically_normal().wstring();

    if (ResourceLoaderPathHelpers::StartsWith(normalized, resourcesRootPrefix))
    {
        const wstring suffix = path.substr(resourcesRootPrefix.size());
        return (assetRootPath / fs::path(suffix)).lexically_normal().wstring();
    }

    if (normalized == resourcesExact)
        return assetRootPath.lexically_normal().wstring();

    if (ResourceLoaderPathHelpers::StartsWith(normalized, resourcesPrefix))
    {
        const wstring suffix = path.substr(resourcesPrefix.size());
        return (assetRootPath / fs::path(suffix)).lexically_normal().wstring();
    }

    if (normalized == shaderExact)
        return (clientBinPath / L"ShaderFiles").lexically_normal().wstring();

    if (ResourceLoaderPathHelpers::StartsWith(normalized, shaderPrefix))
    {
        const wstring suffix = path.substr(shaderPrefix.size());
        return (clientBinPath / L"ShaderFiles" / fs::path(suffix)).lexically_normal().wstring();
    }

    return fs::absolute(inputPath).lexically_normal().wstring();
}

Shared<ResourceLoader> ResourceLoader::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<ResourceLoader>(device, context);
    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : ResourceLoader");
        return nullptr;
    }

    return pInstance;
}

void ResourceLoader::Free()
{
    __super::Free();
}


