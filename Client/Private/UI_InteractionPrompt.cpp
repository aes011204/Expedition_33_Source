#include "UI_InteractionPrompt.h"

#include "Client_Constants.h"
#include "ResourceLoader.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kInteractionPromptImagePrototypeTag[] = L"InteractionPrompt_Key_E_Image";
}

HRESULT UI_InteractionPrompt::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    if (FAILED(Create_ImageUI(levelIndex)))
        LOG_WARN("UI_InteractionPrompt disabled: failed to create key image UI.");

    Set_EditorPositionDrivenByCode(true);
    Close();

    return S_OK;
}

void UI_InteractionPrompt::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_InteractionPrompt::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();
}

void UI_InteractionPrompt::Close()
{
    _isOpen = false;

    if (_root)
        _root->Close();
}

void UI_InteractionPrompt::Show_AtWorldPosition(const Vec3& worldPosition)
{
    if (_root == nullptr)
        return;

    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(worldPosition, screenPos))
    {
        Clear_Target();
        return;
    }

    Apply_EditorOffsets(screenPos);

    if (!Is_Open())
        Open();

    Apply_ScreenPlacement(screenPos);
}

void UI_InteractionPrompt::Clear_Target()
{
    Close();
}

HRESULT UI_InteractionPrompt::Create_ImageUI(uint32 levelIndex)
{
    const wstring texturePath = ResourceLoader::Resolve_RuntimePath(Constants::UI::UI_Interaction_Key_E_Texture_Path);
    if (texturePath.empty() || !fs::exists(texturePath))
    {
        LOG_WARN("UI_InteractionPrompt texture missing. path='{}'", String::ToString(texturePath));
        return S_FALSE;
    }

    if (!GAME->Has_Prototype(levelIndex, kInteractionPromptImagePrototypeTag))
        CHECK_FAILED(GAME->Add_Prototype(levelIndex, kInteractionPromptImagePrototypeTag, UI_Image::Create()), E_FAIL);

    UI_Image::UI_IMAGE_DESC desc{};
    desc.texturePath = texturePath;
    desc.transformDesc.size = Vec2(100.f, 100.f);
    desc.transformDesc.scale = Vec2(0.4f, 0.4f);
    desc.transformDesc.pivot = Vec2(0.5f, 0.5f);
    desc.sortOrder = 3;
    desc.renderGroup = UI_RenderGroup::Overlay;
    desc.isOpenOnCreate = false;

    CHECK_FAILED(GAME->Add_UI(levelIndex, kInteractionPromptImagePrototypeTag, &desc, &_root), E_FAIL);

    return S_OK;
}

void UI_InteractionPrompt::Apply_ScreenPlacement(const Vec2& screenPos)
{
    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);
}

Unique<UI_InteractionPrompt> UI_InteractionPrompt::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_InteractionPrompt>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_InteractionPrompt");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_InteractionPrompt::Free()
{
    Close();

    _root.reset();
    _uiByName.clear();

    UI_RuntimePanelBase::Free();
}
