#include "Level_MainTitle.h"

#include "GameInstance.h"
#include "Level_GameEntry.h"
#include "BackGround.h"
#include "Client_Constants.h"
#include "MainTitleMenuSound.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"
#include "UI_Button.h"
#include "ClientInstance.h"
#include "FreeCamera.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kLeafEffectName[] = L"Effects/Assets/Map/FX_FallingFire.effect.json";

    constexpr Vec4 kCameraEye = Vec4(0.f, 2.f, -8.f, 1.f);
    constexpr Vec4 kCameraAt = Vec4(0.f, 1.f, 0.f, 1.f);
}

Level_MainTitle::Level_MainTitle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_MainTitle::Initialize()
{
    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
        return E_FAIL;

    GAME->Set_RenderEnvironment(false);

    return S_OK;
}

void Level_MainTitle::BeginPlay()
{
    Level::BeginPlay();

    GAME->Play_BGM(Constants::SoundBGM::MainTitleBGM, 0.7f, true, 0.f);

    PlayEffectDesc leafDesc{};
    leafDesc.effectName = kLeafEffectName;
    leafDesc.worldPosition = Vec3(0.f, 0.f, 0.f);
    leafDesc.scale = Vec3(1.f, 1.f, 1.f);

    CLIENT->Play_Effect(leafDesc);
}

void Level_MainTitle::Update(float timeDelta)
{
    Handle_Button();
}

HRESULT Level_MainTitle::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"MainTitle Level");

    return S_OK;
}

void Level_MainTitle::Handle_Button()
{
    MainTitleMenuSound::Update_ButtonHoverSound(_continueButton, _wasContinueHovered);
    MainTitleMenuSound::Update_ButtonHoverSound(_exitButton, _wasExitHovered);

    if ((_continueButton && _continueButton->Was_Clicked()) || GAME->KeyDown(KEY_TYPE::KEY_1))
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_MainTitle_Loading);

        if (FAILED(GAME->Change_Level(ETOI(LevelType::GameEntry), Level_GameEntry::Create(_device.Get(), _context.Get()))))
            return;

        return; 
    }

    if (_exitButton && _exitButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_MainTitle_QuitGame);
        PostQuitMessage(0);

        return;
    }
}

HRESULT Level_MainTitle::Ready_Layer_UI(const wstring& strLayerTag)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = ETOI(LevelType::MainTitle);
    desc.jsonPath = Constants::UI::UI_MainTitle_Path;
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _continueButton = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_MainTitle_Continue",
        "Level_MainTitle"); 

    _exitButton = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_MainTitle_Exit",
        "Level_MainTitle"); 

    return S_OK;
}

HRESULT Level_MainTitle::Ready_Layer_Camera(const wstring& strLayerTag)
{
    FreeCamera::FREECAMERA_DESC cameraDesc{};
    cameraDesc.eye = kCameraEye;
    cameraDesc.at = kCameraAt;
    cameraDesc.fovy = XMConvertToRadians(60.f);
    cameraDesc.nearPlane = 0.1f;
    cameraDesc.farPlane = 500.f;
    cameraDesc.mouseSensor = 0.05f;

    Shared<GameObject> cameraObject;

    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_FreeCamera"),
            ETOI(LevelType::MainTitle),
            strLayerTag,
            cameraObject,
            &cameraDesc),
        E_FAIL);

    GAME->Change_ActiveCamera(dynamic_pointer_cast<Camera>(cameraObject));

    return S_OK;
}

Shared<Level_MainTitle> Level_MainTitle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_MainTitle>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_MainTitle");
        return nullptr;
    }
    return pInstance;
}

void Level_MainTitle::Free()
{
    GAME->Stop_SoundChannel(SoundChannel::BGM, 0.f);

    __super::Free();
}
