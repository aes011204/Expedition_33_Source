#include "ResourceLoader.h"

#include "Component.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "Particle_Types.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Cube.h"
#include "VIBuffer_Rect.h"
#include "VIBuffer_Terrain.h"

namespace fs = std::filesystem;

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
		return E_FAIL;

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
		return E_FAIL;

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
		return E_FAIL;

	for (const FLoadJob& LoadJob : LoadJobs)
	{
		if (FAILED(Register_LoadJob(LoadJob)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT ResourceLoader::Load_GameObjectTable(const wstring& strTablePath)
{
	vector<FLoadJob> LoadJobs;
	if (FAILED(Build_GameObjectJobs(strTablePath, LoadJobs)))
		return E_FAIL;

	for (const FLoadJob& LoadJob : LoadJobs)
	{
		if (FAILED(Register_LoadJob(LoadJob)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT ResourceLoader::Load_StaticLevelTable(const wstring& strTablePath)
{
	vector<FLoadJob> LoadJobs;
	if (FAILED(Build_StaticLevelJobs(strTablePath, LoadJobs)))
		return E_FAIL;

	for (const FLoadJob& LoadJob : LoadJobs)
	{
		if (FAILED(Register_LoadJob(LoadJob)))
			return E_FAIL;
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
		return E_FAIL;

	if (FAILED(Build_ShaderJobs((jsonRootPath / L"DT_Shader.json").wstring(), LoadJobs)))
		return E_FAIL;

    if (FAILED(Build_ComponentJobs((jsonRootPath / L"DT_Component.json").wstring(), LoadJobs)))
        return E_FAIL;

	if (FAILED(Build_StaticLevelJobs((jsonRootPath / L"DT_StaticLevel.json").wstring(), LoadJobs)))
		return E_FAIL;

	if (FAILED(Build_ModelJobs((jsonRootPath / L"DT_Model.json").wstring(), LoadJobs)))
		return E_FAIL;

	if (FAILED(Build_GameObjectJobs((jsonRootPath / L"DT_GameObject.json").wstring(), LoadJobs)))
		return E_FAIL;

	const uint32 iTargetLevel = ETOI(eLevel);
	for (const FLoadJob& LoadJob : LoadJobs)
	{
		if (LoadJob.iLevelIndex != iTargetLevel)
			continue;

		if (FAILED(Register_LoadJob(LoadJob)))
			return E_FAIL;
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

		if (strId.empty() || strPath.empty())
			continue;

		FLoadJob LoadJob = {};
		LoadJob.eType = ELoadJobType::Shader;
		LoadJob.iLevelIndex = Get_LevelIndex_FromString(strLevel);
		LoadJob.strPrototypeTag = String::ToWString(strId);
		LoadJob.strPath = Resolve_RuntimePath(String::ToWString(strPath));
		LoadJob.strInputLayout = strInputLayout;
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
		LoadJob.strModelType = Read_String(JsonItem, "ModelType", "modelType", "NONANIM");
		LoadJob.fScaleX = Read_Float(JsonItem, "ScaleX", "scaleX", 1.f);
		LoadJob.fScaleY = Read_Float(JsonItem, "ScaleY", "scaleY", 1.f);
		LoadJob.fScaleZ = Read_Float(JsonItem, "ScaleZ", "scaleZ", 1.f);
		LoadJob.fRotY = Read_Float(JsonItem, "RotY", "rotY", 0.f);
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
		LoadJob.strResourceType = strResourceType;
		LoadJob.iCount = Read_UInt(JsonItem, "Count", "count", 1);

		LoadJobs.push_back(LoadJob);
	}

	return S_OK;
}

ResourceLoader::FInputLayoutInfo ResourceLoader::Get_InputLayout(const string& strName)
{
	if ("VTXNORTEX" == strName || "NorTex" == strName || "VtxNorTex" == strName)
		return { VTXNORTEX::Elements, VTXNORTEX::numElements };

	if ("VTXMESH" == strName || "Mesh" == strName || "VtxMesh" == strName)
		return { VTXMESH::Elements, VTXMESH::numElements };

	if ("VTXANIMMESH" == strName || "AnimMesh" == strName || "VtxAnimMesh" == strName)
		return { VTXANIMMESH::Elements, VTXANIMMESH::numElements };

	if ("VTXCUBE" == strName || "Cube" == strName || "VtxCube" == strName)
		return { VTXCUBE::Elements, VTXCUBE::numElements };

    // Particle instance용 InputLayout
    if ("ParticleInstance" == strName || "PARTICLEINSTANCE" == strName)
		return { ParticleInstanceLayout::Elements, ParticleInstanceLayout::numElements };

	return { VTXTEX::Elements, VTXTEX::numElements };
}

HRESULT ResourceLoader::Load_Textures(const json& JsonData)
{
	if (!JsonData.is_array())
		return E_FAIL;

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
			return E_FAIL;

		if (FAILED(GAME->Add_Prototype(Get_LevelIndex_FromString(strLevel), String::ToWString(strId), pTexture)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT ResourceLoader::Load_Shaders(const json& JsonData)
{
	if (!JsonData.is_array())
		return E_FAIL;

	for (const json& JsonItem : JsonData)
	{
		const string strId = Read_String(JsonItem, "Id", "id");
		const string strPath = Read_String(JsonItem, "Path", "path");
		const string strLevel = Read_String(JsonItem, "Level", "level", "Static");
		const string strInputLayout = Read_String(JsonItem, "Type", "type", "VTXTEX");

		if (strId.empty() || strPath.empty())
			continue;

		const FInputLayoutInfo InputLayout = Get_InputLayout(strInputLayout);
		const wstring resolvedPath = Resolve_RuntimePath(String::ToWString(strPath));
		Shared<ShaderCom> pShader = ShaderCom::Create(_device, _context, resolvedPath.c_str(), InputLayout.pElements, InputLayout.iNumElements);
		if (nullptr == pShader)
			return E_FAIL;

		if (FAILED(GAME->Add_Prototype(Get_LevelIndex_FromString(strLevel), String::ToWString(strId), pShader)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT ResourceLoader::Register_LoadJob(const FLoadJob& LoadJob)
{
	switch (LoadJob.eType)
	{
	case ELoadJobType::TextureCreate:
	{
		Shared<Texture> pTexture = Texture::Create(_device, _context, LoadJob.strPath.c_str(), LoadJob.iCount);
		if (nullptr == pTexture)
			return E_FAIL;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, pTexture);
	}

	case ELoadJobType::Shader:
	{
		const FInputLayoutInfo InputLayout = Get_InputLayout(LoadJob.strInputLayout);
		Shared<ShaderCom> pShader = ShaderCom::Create(_device, _context, LoadJob.strPath.c_str(), InputLayout.pElements, InputLayout.iNumElements);
		if (nullptr == pShader)
			return E_FAIL;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, pShader);
	}

	case ELoadJobType::Model:
	{
		const ModelType eModelType = ("ANIM" == LoadJob.strModelType || "Anim" == LoadJob.strModelType) ? ModelType::Anim : ModelType::NonAnim;
		const XMMATRIX PreLocalTransformMatrix = XMMatrixScaling(LoadJob.fScaleX, LoadJob.fScaleY, LoadJob.fScaleZ) * XMMatrixRotationY(XMConvertToRadians(LoadJob.fRotY));
		const string strPath = String::ToString(LoadJob.strPath);

		Shared<ModelCom> model = ModelCom::Create(_device, _context, eModelType, strPath.c_str(), PreLocalTransformMatrix);
		if (nullptr == model)
			return E_FAIL;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, model);
	}

	case ELoadJobType::GameObject:
	{
		Shared<GameObject> gameObject = GAME->Create_GameObjectPrototype(LoadJob.strClassName);
		if (nullptr == gameObject)
			return E_FAIL;

		return GAME->Add_Prototype(
            LoadJob.iLevelIndex,
            LoadJob.strPrototypeTag,
            static_pointer_cast<Base>(gameObject));
	}

    case ELoadJobType::Component:
    {
        Shared<Component> component = GAME->Create_ComponentPrototype(LoadJob.strClassName);
        if (nullptr == component)
            return E_FAIL;

        return GAME->Add_Prototype(
            LoadJob.iLevelIndex,
            LoadJob.strPrototypeTag,
            static_pointer_cast<Base>(component));
    }

	case ELoadJobType::StaticLevel:
	{
		Shared<Base> staticPrototype = Create_StaticPrototype(LoadJob);
		if (nullptr == staticPrototype)
			return S_OK;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, staticPrototype);
	}

	default:
		return S_OK;
	}
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

wstring ResourceLoader::Make_PrototypeTag(const string& strId, const wstring& strPrefix)
{
	const wstring strTag = String::ToWString(strId);
	if (0 == strTag.find(L"Prototype_"))
		return strTag;

	return strPrefix + strTag;
}

uint32 ResourceLoader::Get_LevelIndex_FromString(const string& strLevel)
{
	if ("Logo" == strLevel) // || "Logo" == strLevel)
		return ETOI(LevelType::Logo);

	if ("GamePlay" == strLevel || "Gameplay" == strLevel || "GamePlay" == strLevel)
		return ETOI(LevelType::GamePlay);

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


