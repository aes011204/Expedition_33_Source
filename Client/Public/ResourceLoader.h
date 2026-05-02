#pragma once

#include "Client_Defines.h"
#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
class GameObject;
class Shader;
class Component;
NS_END

NS_BEGIN(Client)

enum class ELoadJobType
{
    TextureCreate,
    TextureAppend,
    Shader,
    Model,
    GameObject,
    Component,
    StaticLevel,
    Unknown
};

using FLoadJob = struct tagLoadJob
{
    ELoadJobType eType = ELoadJobType::Unknown;
    uint32 iLevelIndex = {};

    wstring strPrototypeTag = {};
    wstring strPath = {};

    string strInputLayout = {};
    string strResourceType = {};
    string strModelType = {};
    string strClassName = {};

    float fScaleX = 1.f;
    float fScaleY = 1.f;
    float fScaleZ = 1.f;
    float fRotY = 0.f;
    uint32 iCount = 1;
};

class ResourceLoader final : public Base
{
public:
    using FInputLayoutInfo = struct tagInputLayoutInfo
    {
        const D3D11_INPUT_ELEMENT_DESC* pElements = nullptr;
        uint32 iNumElements = {};
    };

public:
    ResourceLoader(const ComPtr<Device>& device, const ComPtr<Context>& context);
	~ResourceLoader() override = default;

public:
    HRESULT Initialize();

    HRESULT Load_TextureTable(const wstring& strTablePath);

    HRESULT Load_ShaderTable(const wstring& strTablePath);

    HRESULT Load_ModelTable(const wstring& strTablePath);

    HRESULT Load_GameObjectTable(const wstring& strTablePath);

    HRESULT Load_StaticLevelTable(const wstring& strTablePath);

    HRESULT Load_LevelTables(LevelType eLevel, const wstring& strJsonRootPath = L"");

    HRESULT Build_TextureJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

    HRESULT Build_ShaderJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

    HRESULT Build_ModelJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

    HRESULT Build_GameObjectJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);
    HRESULT Build_ComponentJobs(const wstring& strTablePath, vector<FLoadJob>& loadJobs);

    HRESULT Build_StaticLevelJobs(const wstring& strTablePath, vector<FLoadJob>& LoadJobs);

    static FInputLayoutInfo Get_InputLayout(const string& strName);

private:
    HRESULT Load_Textures(const json& JsonData);

    HRESULT Load_Shaders(const json& JsonData);

    HRESULT Register_LoadJob(const FLoadJob& LoadJob);

    Shared<Base> Create_StaticPrototype(const FLoadJob& LoadJob);

    static bool Load_JsonFile(const wstring& strTablePath, json& JsonRoot);

    static string Read_String(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, const string& strDefault = "");

    static uint32 Read_UInt(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, uint32 iDefault = 0);

    static float Read_Float(const json& JsonItem, const string& strUpperKey, const string& strLowerKey, float fDefault = 0.f);

    static wstring Make_PrototypeTag(const string& strId, const wstring& strPrefix);

    static uint32 Get_LevelIndex_FromString(const string& strLevel);
    static wstring Get_DefaultJsonRootPath();
    static wstring Resolve_RuntimePath(const wstring& path);

private:
    ComPtr<Device> _device = { nullptr };
    ComPtr<Context> _context = { nullptr };

public:
    static Shared<ResourceLoader> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);

    void Free() override;
};

NS_END
