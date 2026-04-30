#include "ResourceLoader.h"

#include "GameInstance.h"
#include "Texture.h"
#include "Shader.h"
#include "Model.h"
#include "VIBuffer_Rect.h"
#include "VIBuffer_Cube.h"
#include "VIBuffer_Terrain.h"

#include "BackGround.h"
#include "Terrain.h"
#include "FreeCamera.h"
#include "Monster.h"
#include "Player.h"
#include "Body_Player.h"
#include "Weapon.h"
#include "Sky.h"

ResourceLoader::ResourceLoader(ComPtr<Device> device, ComPtr<Context> context)
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

	if (JsonRoot.contains("Shader"))
		return Load_Shaders(JsonRoot["Shader"]);

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
	const wstring strRootPath = strJsonRootPath.empty() ? TEXT("../Bin/Resources/Data/json/") : strJsonRootPath;

	if (FAILED(Build_TextureJobs(strRootPath + TEXT("DT_Texture.json"), LoadJobs)))
		return E_FAIL;

	if (FAILED(Build_ShaderJobs(strRootPath + TEXT("DT_Shader.json"), LoadJobs)))
		return E_FAIL;

	if (FAILED(Build_StaticLevelJobs(strRootPath + TEXT("DT_StaticLevel.json"), LoadJobs)))
		return E_FAIL;

	if (FAILED(Build_ModelJobs(strRootPath + TEXT("DT_Model.json"), LoadJobs)))
		return E_FAIL;

	if (FAILED(Build_GameObjectJobs(strRootPath + TEXT("DT_GameObject.json"), LoadJobs)))
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
		LoadJob.strPrototypeTag = ToWString(strId);
		LoadJob.strPath = ToWString(strPath);
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
	else if (JsonRoot.contains("Shader"))
		pShaderArray = &JsonRoot["Shader"];

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
		LoadJob.strPrototypeTag = ToWString(strId);
		LoadJob.strPath = ToWString(strPath);
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
	else if (JsonRoot.contains("Model"))
		pModelArray = &JsonRoot["Model"];

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
		LoadJob.strPrototypeTag = ToWString(strId);
		LoadJob.strPath = ToWString(strPath);
		LoadJob.strModelType = Read_String(JsonItem, "ModelType", "modelType", "NonAnim");
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

		if (strId.empty())
			continue;

		FLoadJob LoadJob = {};
		LoadJob.eType = ELoadJobType::GameObject;
		LoadJob.iLevelIndex = Get_LevelIndex_FromString(strLevel);
		LoadJob.strPrototypeTag = ToWString(strId);
		LoadJob.iCount = 1;

		LoadJobs.push_back(LoadJob);
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
		LoadJob.strPrototypeTag = ToWString(strId);
		LoadJob.strResourceType = strResourceType;
		LoadJob.iCount = Read_UInt(JsonItem, "Count", "count", 1);

		LoadJobs.push_back(LoadJob);
	}

	return S_OK;
}

ResourceLoader::FInputLayoutInfo ResourceLoader::Get_InputLayout(const string& strName)
{
	if ("VTXNORTEX" == strName || "NorTex" == strName || "VtxNorTex" == strName)
		return { VTXNORTEX::Elements, VTXNORTEX::iNumElements };

	if ("VTXMESH" == strName || "Mesh" == strName || "VtxMesh" == strName)
		return { VTXMESH::Elements, VTXMESH::iNumElements };

	if ("VTXANIMMESH" == strName || "AnimMesh" == strName || "VtxAnimMesh" == strName)
		return { VTXANIMMESH::Elements, VTXANIMMESH::iNumElements };

	if ("VTXCUBE" == strName || "Cube" == strName || "VtxCube" == strName)
		return { VTXCUBE::Elements, VTXCUBE::iNumElements };

	return { VTXTEX::Elements, VTXTEX::iNumElements };
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

		Shared<Texture> pTexture = Texture::Create(_device.Get(), _context.Get(), ToWString(strPath).c_str(), iCount);
		if (nullptr == pTexture)
			return E_FAIL;

		if (FAILED(GAME->Add_Prototype(Get_LevelIndex_FromString(strLevel), ToWString(strId), pTexture)))
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
		Shared<Shader> pShader = Shader::Create(_device.Get(), _context.Get(), ToWString(strPath).c_str(), InputLayout.pElements, InputLayout.iNumElements);
		if (nullptr == pShader)
			return E_FAIL;

		if (FAILED(GAME->Add_Prototype(Get_LevelIndex_FromString(strLevel), ToWString(strId), pShader)))
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
		Shared<Texture> pTexture = Texture::Create(_device.Get(), _context.Get(), LoadJob.strPath.c_str(), LoadJob.iCount);
		if (nullptr == pTexture)
			return E_FAIL;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, pTexture);
	}

	case ELoadJobType::Shader:
	{
		const FInputLayoutInfo InputLayout = Get_InputLayout(LoadJob.strInputLayout);
		Shared<Shader> pShader = Shader::Create(_device.Get(), _context.Get(), LoadJob.strPath.c_str(), InputLayout.pElements, InputLayout.iNumElements);
		if (nullptr == pShader)
			return E_FAIL;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, pShader);
	}

	case ELoadJobType::Model:
	{
		const ModelType eModelType = ("ANIM" == LoadJob.strModelType || "Anim" == LoadJob.strModelType) ? ModelType::Anim : ModelType::NonAnim;
		const XMMATRIX PreLocalTransformMatrix = XMMatrixScaling(LoadJob.fScaleX, LoadJob.fScaleY, LoadJob.fScaleZ) * XMMatrixRotationY(XMConvertToRadians(LoadJob.fRotY));
		const string strPath = ToString(LoadJob.strPath);

		Shared<Model> pModel = Model::Create(_device.Get(), _context.Get(), eModelType, strPath.c_str(), PreLocalTransformMatrix);
		if (nullptr == pModel)
			return E_FAIL;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, pModel);
	}

	case ELoadJobType::GameObject:
	{
		Shared<Engine::GameObject> pGameObject = Create_GameObjectPrototype(LoadJob.strPrototypeTag);
		if (nullptr == pGameObject)
		{
			const wstring strMessage = L"[ResourceLoader] Skipped unknown GameObject prototype: " + LoadJob.strPrototypeTag + L"\n";
			OutputDebugStringW(strMessage.c_str());
			return S_OK;
		}

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, pGameObject);
	}

	case ELoadJobType::StaticLevel:
	{
		Shared<Base> pStaticPrototype = Create_StaticPrototype(LoadJob);
		if (nullptr == pStaticPrototype)
			return S_OK;

		return GAME->Add_Prototype(LoadJob.iLevelIndex, LoadJob.strPrototypeTag, pStaticPrototype);
	}

	default:
		return S_OK;
	}
}

Shared<Engine::GameObject> ResourceLoader::Create_GameObjectPrototype(const wstring& strPrototypeTag)
{
	if (TEXT("GameObject_BackGround") == strPrototypeTag)
		return BackGround::Create(_device.Get(), _context.Get());

	if (TEXT("GameObject_Terrain") == strPrototypeTag)
		return Terrain::Create(_device.Get(), _context.Get());

	if (TEXT("GameObject_FreeCamera") == strPrototypeTag)
		return FreeCamera::Create(_device.Get(), _context.Get());

	if (TEXT("GameObject_Monster") == strPrototypeTag)
		return Monster::Create(_device.Get(), _context.Get());

	if (TEXT("GameObject_Player") == strPrototypeTag)
		return Player::Create(_device.Get(), _context.Get());

	if (TEXT("GameObject_Body_Player") == strPrototypeTag)
		return Body_Player::Create(_device.Get(), _context.Get());

	if (TEXT("GameObject_Weapon") == strPrototypeTag)
		return Weapon::Create(_device.Get(), _context.Get());

	if (TEXT("GameObject_Sky") == strPrototypeTag)
		return Sky::Create(_device.Get(), _context.Get());

	return nullptr;
}

Shared<Base> ResourceLoader::Create_StaticPrototype(const FLoadJob& LoadJob)
{
	if ("VIBuffer" != LoadJob.strResourceType)
		return nullptr;

	if (TEXT("VIBuffer_Terrain") == LoadJob.strPrototypeTag)
		return VIBuffer_Terrain::Create(_device.Get(), _context.Get(), TEXT("../Bin/Resources/Textures/Terrain/Height.bmp"));

	if (TEXT("VIBuffer_Cube") == LoadJob.strPrototypeTag)
		return VIBuffer_Cube::Create(_device.Get(), _context.Get());

	if (TEXT("VIBuffer_Rect") == LoadJob.strPrototypeTag)
		return VIBuffer_Rect::Create(_device.Get(), _context.Get());

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

wstring ResourceLoader::ToWString(const string& strValue)
{
	if (strValue.empty())
		return {};

	const int iSize = MultiByteToWideChar(CP_UTF8, 0, strValue.c_str(), -1, nullptr, 0);
	if (iSize <= 0)
		return {};

	wstring strResult(static_cast<size_t>(iSize - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, strValue.c_str(), -1, strResult.data(), iSize);
	return strResult;
}

string ResourceLoader::ToString(const wstring& strValue)
{
	if (strValue.empty())
		return {};

	const int iSize = WideCharToMultiByte(CP_UTF8, 0, strValue.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (iSize <= 0)
		return {};

	string strResult(static_cast<size_t>(iSize - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, strValue.c_str(), -1, strResult.data(), iSize, nullptr, nullptr);
	return strResult;
}

wstring ResourceLoader::Make_PrototypeTag(const string& strId, const wstring& strPrefix)
{
	const wstring strTag = ToWString(strId);
	if (0 == strTag.find(L"Prototype_"))
		return strTag;

	return strPrefix + strTag;
}

uint32 ResourceLoader::Get_LevelIndex_FromString(const string& strLevel)
{
	if ("Logo" == strLevel || "Logo" == strLevel)
		return ETOI(LevelType::Logo);

	if ("GamePlay" == strLevel || "Gameplay" == strLevel || "GamePlay" == strLevel)
		return ETOI(LevelType::GamePlay);

	if ("Loading" == strLevel || "Loading" == strLevel)
		return ETOI(LevelType::Loading);

	return ETOI(LevelType::Static);
}

Shared<ResourceLoader> ResourceLoader::Create(ComPtr<Device> device, ComPtr<Context> context)
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


