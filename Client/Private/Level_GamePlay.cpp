#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "PointParticlePreview.h"
#include "UI_Image.h"
#include "UI_Text.h"

namespace
{
    constexpr wchar_t kGamePlayUIJsonPath[] = L"UI/UI_Json/Json_SaveLoad_Test.json";

    string Read_String(const json& item, const char* key, const string& defaultValue = "")
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_string())
            return defaultValue;

        return iter->get<string>();
    }

    int32 Read_Int(const json& item, const char* key, int32 defaultValue = 0)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_number_integer())
            return defaultValue;

        return iter->get<int32>();
    }

    float Read_Float(const json& item, const char* key, float defaultValue = 0.f)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_number())
            return defaultValue;

        return iter->get<float>();
    }

    Vec2 Read_Vec2(const json& item, const char* key, const Vec2& defaultValue)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 2)
            return defaultValue;

        return Vec2(
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y);
    }

    Vec4 Read_Color(const json& item, const char* key, const Vec4& defaultValue)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 4)
            return defaultValue;

        Vec4 color(
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y,
            iter->at(2).is_number() ? iter->at(2).get<float>() : defaultValue.z,
            iter->at(3).is_number() ? iter->at(3).get<float>() : defaultValue.w);

        if (color.x > 1.f || color.y > 1.f || color.z > 1.f || color.w > 1.f)
            color *= 1.f / 255.f;

        return color;
    }

    bool StartsWith(const string& value, const string& prefix)
    {
        return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
    }

    wstring Resolve_RuntimeResourcePath(const string& runtimePath)
    {
        if (runtimePath.empty())
            return {};

        string normalizedPath = runtimePath;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        const fs::path inputPath = fs::path(String::ToWString(normalizedPath));
        if (inputPath.is_absolute())
            return inputPath.lexically_normal().wstring();

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return fs::absolute(inputPath).lexically_normal().wstring();

        const string loweredPath = String::ToLowerCopy(normalizedPath);
        string resourceRelativePath = normalizedPath;

        constexpr const char* resourcePrefix = "../bin/resources/";
        constexpr const char* binResourcePrefix = "bin/resources/";
        constexpr const char* resourcesPrefix = "resources/";

        if (StartsWith(loweredPath, resourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcePrefix));
        else if (StartsWith(loweredPath, binResourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(binResourcePrefix));
        else if (StartsWith(loweredPath, resourcesPrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcesPrefix));

        return (fs::path(assetRoot) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    wstring Resolve_JsonAssetPath(const json& item, const char* guidKey, const char* pathKey)
    {
        const string guid = Read_String(item, guidKey);
        if (!guid.empty())
        {
            const wstring guidPath = GAME->Resolve_AssetPath(guid);
            if (!guidPath.empty() && fs::exists(guidPath))
                return guidPath;
        }

        return Resolve_RuntimeResourcePath(Read_String(item, pathKey));
    }

    UI_Text::TextAlign Read_TextAlign(const json& item)
    {
        string align = Read_String(item, "align");
        if (align.empty())
            align = Read_String(item, "textAlign");

        align = String::ToLowerCopy(align);
        if ("center" == align)
            return UI_Text::TextAlign::Center;

        if ("right" == align)
            return UI_Text::TextAlign::Right;

        return UI_Text::TextAlign::Left;
    }
}

Level_GamePlay::Level_GamePlay(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Level { device, context }
{
}

HRESULT Level_GamePlay::Initialize()	
{
	if (FAILED(Ready_Lights()))
		return E_FAIL;

    if (FAILED(Ready_UI_FromJson()))
        return E_FAIL;

	if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_Player(TEXT("Layer_Player"))))
		return E_FAIL;

    if (FAILED(Ready_Layer_Monster(TEXT("Layer_Monster"))))
        return E_FAIL;

	return S_OK;
}

void Level_GamePlay::BeginPlay()
{
    Level::BeginPlay();

}

void Level_GamePlay::Update(float fTimeDelta)
{
    int a = 10;
}

HRESULT Level_GamePlay::Render()
{
#ifdef _DEBUG
	SetWindowText(g_hWnd, TEXT("Level GamePlay"));
#endif
	return S_OK;
}

HRESULT Level_GamePlay::Ready_Lights()
{
	LIGHT_DESC			LightDesc{};

	LightDesc.type          = LightType::Directional;
	LightDesc.direction     = XMFLOAT4(1.f, -1.f, 1.f, 0.f);
	LightDesc.diffuse       = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	LightDesc.ambient       = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	LightDesc.specular      = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

	if (FAILED(GAME->Add_Light(LightDesc)))
		return E_FAIL;


	return S_OK;
}

HRESULT Level_GamePlay::Ready_UI_FromJson()
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
        return E_FAIL;

    const fs::path uiJsonPath = (fs::path(assetRoot) / kGamePlayUIJsonPath).lexically_normal();
    if (!fs::exists(uiJsonPath))
    {
        LOG_WARN("Gameplay UI json not found: {}", String::ToString(uiJsonPath.wstring()));
        return S_OK;
    }

    json root{};
    try
    {
        ifstream file(uiJsonPath);
        if (!file.is_open())
            return E_FAIL;

        file >> root;
    }
    catch (const exception& exception)
    {
        LOG_ERROR("Gameplay UI json load failed: {} ({})", exception.what(), String::ToString(uiJsonPath.wstring()));
        return E_FAIL;
    }

    const auto objectsIter = root.find("objects");
    if (objectsIter == root.end() || !objectsIter->is_array())
    {
        LOG_WARN("Gameplay UI json has no objects array: {}", String::ToString(uiJsonPath.wstring()));
        return S_OK;
    }

    vector<json> objects;
    objects.reserve(objectsIter->size());
    for (const json& item : *objectsIter)
    {
        if (item.is_object())
            objects.push_back(item);
    }

    stable_sort(objects.begin(), objects.end(), [](const json& lhs, const json& rhs)
    {
        const int32 lhsSort = Read_Int(lhs, "sortOrder", 0);
        const int32 rhsSort = Read_Int(rhs, "sortOrder", 0);
        if (lhsSort != rhsSort)
            return lhsSort < rhsSort;

        return Read_Int(lhs, "id", 0) < Read_Int(rhs, "id", 0);
    });

    constexpr uint32 levelIndex = ETOI(LevelType::GamePlay);

    for (const json& item : objects)
    {
        const string type = Read_String(item, "type");
        const int32 id = Read_Int(item, "id", 0);

        if ("Image" == type)
        {
            const wstring texturePath = Resolve_JsonAssetPath(item, "textureGuid", "texturePath");
            if (texturePath.empty() || !fs::exists(texturePath))
            {
                LOG_WARN("Gameplay UI image skipped: missing texture. id={}", id);
                continue;
            }

            UI_Image::UI_IMAGE_DESC desc{};
            desc.texturePath = texturePath;
            desc.color = Read_Color(item, "color", Vec4(1.f, 1.f, 1.f, 1.f));
            desc.transformDesc.position = Read_Vec2(item, "position", Vec2(0.f, 0.f));
            desc.transformDesc.size = Read_Vec2(item, "size", Vec2(100.f, 100.f));
            desc.transformDesc.scale = Read_Vec2(item, "scale", Vec2(1.f, 1.f));
            desc.transformDesc.rotationRadians = Read_Float(item, "rotation", Read_Float(item, "rotationRadians", 0.f));
            desc.sortOrder = Read_Int(item, "sortOrder", 0);
            desc.isOpenOnCreate = true;

            const wstring prototypeTag = L"UIJson_Image_" + to_wstring(id);
            if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Image::Create())))
                return E_FAIL;

            if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc)))
                return E_FAIL;
        }
        else if ("Text" == type)
        {
            const wstring fontPath = Resolve_JsonAssetPath(item, "fontGuid", "fontPath");
            if (fontPath.empty() || !fs::exists(fontPath))
            {
                LOG_WARN("Gameplay UI text skipped: missing font. id={}", id);
                continue;
            }

            const float fontSize = max(4.f, Read_Float(item, "fontSize", 32.f));
            Vec2 scale = Read_Vec2(item, "scale", Vec2(1.f, 1.f));
            scale.x *= fontSize / 32.f;

            const wstring fontTag = L"UIJson_Font_" + to_wstring(id);
            if (FAILED(GAME->Add_Font(fontTag, fontPath.c_str())))
                return E_FAIL;

            UI_Text::UI_TEXT_DESC desc{};
            desc.text = String::ToWString(Read_String(item, "text"));
            desc.fontTag = fontTag;
            desc.color = Read_Color(item, "color", Vec4(1.f, 1.f, 1.f, 1.f));
            desc.textAlign = Read_TextAlign(item);
            desc.transformDesc.position = Read_Vec2(item, "position", Vec2(0.f, 0.f));
            desc.transformDesc.size = Read_Vec2(item, "size", Vec2(300.f, 50.f));
            desc.transformDesc.scale = scale;
            desc.transformDesc.rotationRadians = Read_Float(item, "rotation", Read_Float(item, "rotationRadians", 0.f));
            desc.sortOrder = Read_Int(item, "sortOrder", 0);
            desc.isOpenOnCreate = true;

            const wstring prototypeTag = L"UIJson_Text_" + to_wstring(id);
            if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Text::Create())))
                return E_FAIL;

            if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc)))
                return E_FAIL;
        }
        else
        {
            LOG_WARN("Gameplay UI object type skipped: {} id={}", type, id);
        }
    }

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Camera(const wstring& strLayerTag)
{
	FreeCamera::FREECAMERA_DESC		FreeCameraDesc{};

	FreeCameraDesc.speedPerSec = 10.f;
	FreeCameraDesc.degreePerSec = 180.f;
	FreeCameraDesc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
	FreeCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
	FreeCameraDesc.fovy = XMConvertToRadians(60.f) ;
    FreeCameraDesc.pitch = 0.f;
    FreeCameraDesc.yaw = 0.f;
	FreeCameraDesc.nearPlane = 0.1f;
	FreeCameraDesc.farPlane = 500.f;
	FreeCameraDesc.mouseSensor = 0.05f;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_FreeCamera"),
		ETOI(LevelType::GamePlay), strLayerTag, &FreeCameraDesc)))
		return E_FAIL;

    TargetCamera::TARGETCAMERA_DESC		TargetCameraDesc{};

    TargetCameraDesc.speedPerSec = 10.f;
    TargetCameraDesc.degreePerSec = 180.f;
    TargetCameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    TargetCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    TargetCameraDesc.fovy = XMConvertToRadians(60.f) ;
    TargetCameraDesc.pitch = 0.f;
    TargetCameraDesc.yaw = 0.f;
    TargetCameraDesc.nearPlane = 0.1f;
    TargetCameraDesc.farPlane = 500.f;
    TargetCameraDesc.mouseSensor = 0.2f;
    TargetCameraDesc.followSpeed = 5.f;
    TargetCameraDesc.offset = Vec3 {0.5f, 1.f, 0.f};
    TargetCameraDesc.distance = 2.f;
    TargetCameraDesc.sideOffset = 0.5;
    TargetCameraDesc.heightOffset = 1.f;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_TargetCamera"),
        ETOI(LevelType::GamePlay), strLayerTag, &TargetCameraDesc)))
            return E_FAIL;


	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_BackGround(const wstring& strLayerTag)
{
	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Terrain"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Sky"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_PointParticlePreview"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Monster(const wstring& strLayerTag)
{
	for (size_t i = 0; i < 10; i++)
	{
		if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Monster"),
			ETOI(LevelType::GamePlay), strLayerTag)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Player"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;



	return S_OK;
}

Shared<Level_GamePlay> Level_GamePlay::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto pInstance = make_shared<Level_GamePlay>(device, context);

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : Level_GamePlay");
		return nullptr;
	}
	return pInstance;
}


void Level_GamePlay::Free()
{
	__super::Free();

}

