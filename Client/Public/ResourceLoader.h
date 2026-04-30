#pragma once

#include "Client_Defines.h"
#include "Base.h"

namespace Engine
{
	class GameInstance;
	class GameObject;
}

NS_BEGIN(Client)

enum class ELoadJobType
{
	TextureCreate,
	TextureAppend,
	Shader,
	Model,
	GameObject,
	StaticLevel,
	Unknown
};

typedef struct tagLoadJob
{
	ELoadJobType eType = ELoadJobType::Unknown;
	uint32 iLevelIndex = {};
	wstring strPrototypeTag = {};
	wstring strPath = {};
	string strInputLayout = {};
	string strResourceType = {};
	string strModelType = {};
	float fScaleX = 1.f;
	float fScaleY = 1.f;
	float fScaleZ = 1.f;
	float fRotY = 0.f;
	uint32 iCount = 1;
}FLoadJob;

class ResourceLoader final : public Base
{
public:
	typedef struct tagInputLayoutInfo
	{
		const D3D11_INPUT_ELEMENT_DESC* pElements = nullptr;
		uint32 iNumElements = {};
	}FInputLayoutInfo;

public:
	ResourceLoader(ComPtr<Device> device, ComPtr<Context> context);
	virtual ~ResourceLoader() { Free(); }

public:
	HRESULT Initialize();

	HRESULT Load_TextureTable(const wstring& strTablePath);

	HRESULT Load_ShaderTable(const wstring& strTablePath);

	HRESULT Load_ModelTable(const wstring& strTablePath);

	HRESULT Load_GameObjectTable(const wstring& strTablePath);

	HRESULT Load_StaticLevelTable(const wstring& strTablePath);

	HRESULT Load_LevelTables(LevelType eLevel, const wstring& strJsonRootPath = TEXT("../Bin/Resources/Data/json/"));

	HRESULT Build_TextureJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

	HRESULT Build_ShaderJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

	HRESULT Build_ModelJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

	HRESULT Build_GameObjectJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

	HRESULT Build_StaticLevelJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

	static FInputLayoutInfo Get_InputLayout(const string& strName);

private:
	HRESULT Load_Textures(const json& JsonData);

	HRESULT Load_Shaders(const json& JsonData);

	HRESULT Register_LoadJob(const FLoadJob& LoadJob);

	Shared<Engine::GameObject> Create_GameObjectPrototype(const wstring& strPrototypeTag);

	Shared<Base> Create_StaticPrototype(const FLoadJob& LoadJob);

	static bool Load_JsonFile(const wstring& strTablePath, json& JsonRoot);

	static string Read_String(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, const string& strDefault = "");

	static uint32 Read_UInt(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, uint32 iDefault = 0);

	static float Read_Float(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, float fDefault = 0.f);

	static wstring ToWString(const string& strValue);

	static string ToString(const wstring& strValue);

	static wstring Make_PrototypeTag(const string& strId, const wstring& strPrefix);

	static uint32 Get_LevelIndex_FromString(const string& strLevel);

private:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };

public:
	static Shared<ResourceLoader> Create(ComPtr<Device> device, ComPtr<Context> context);

	virtual void Free() override;
};

NS_END

