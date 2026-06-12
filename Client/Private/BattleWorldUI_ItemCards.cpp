#include "BattleWorldUI_ItemCards.h"

#include "BattleWorldUI_JsonTextureResolver.h"
#include "ClientInstance.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(Client)

namespace
{
    constexpr char kBattleWorldItemCardTextureGuid[] = "bec2ba8a-a265-40d8-9bd1-bc85420ebaa2";
    constexpr char kBattleWorldKeyFrameTextureGuid[] = "72de5c45-b20f-490d-9d69-396a830b514f";
    constexpr wchar_t kBattleWorldItemPanelJsonPath[] = L"UI/UI_Json/BattleSystem/Item/UI_BattleSystem_Item.json";
    constexpr uint32 kBattleWorldItemCardPixelWidth = 768;
    constexpr uint32 kBattleWorldItemCardPixelHeight = 192;
    constexpr uint32 kBattleWorldItemSlotCount = 3;
    const Vec2 kBattleWorldItemListWorldSize = Vec2(1.00f, 0.25f);
    const Vec3 kBattleWorldItemListOffsets[kBattleWorldItemSlotCount] =
    {
        Vec3(0.50f, 1.02f, 0.f),
        Vec3(0.50f, 0.75f, 0.f),
        Vec3(0.50f, 0.48f, 0.f),
    };
    constexpr float kBattleWorldItemDefaultScale = 1.f;

    enum class BattleWorldItemCardImageKind
    {
        CardBackground,
        KeyFrame,
    };

    struct BattleWorldItemSlotDef
    {
        BattleItemType itemType = BattleItemType::Invalid;
        const wchar_t* keyPrompt = L"";
    };

    const BattleWorldItemSlotDef kBattleWorldItemSlots[kBattleWorldItemSlotCount] =
    {
        { BattleItemType::HealingTint, L"Q" },
        { BattleItemType::EnergyTint, L"W" },
        { BattleItemType::ReviveTint, L"E" },
    };

    struct BattleWorldItemCardTextDef
    {
        wstring text;
        wstring fontTag;
        Vec2 position{};
        float scale = 1.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        bool centered = false;
    };

    struct BattleWorldItemCardImageDef
    {
        BattleWorldItemCardImageKind kind = BattleWorldItemCardImageKind::CardBackground;
        Vec2 center{};
        Vec2 size{};
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
    };

    struct BattleWorldItemCardContent
    {
        BattleItemType itemType = BattleItemType::Invalid;
        wstring keyPrompt;
        wstring title;
        wstring description;
        wstring subDescription;
        int32 count = 0;
        int32 primaryValue = 0;
    };

    struct BattleWorldItemCardDef
    {
        BattleWorldItemCardContent content{};
        vector<BattleWorldItemCardImageDef> images{};
        vector<BattleWorldItemCardTextDef> texts{};
    };

    class IWorldUIItemCardSurface;

    struct BattleWorldItemCardInstance
    {
        BattleWorldItemCardDef def{};
        Unique<IWorldUIItemCardSurface> surface{};
        Matrix worldMatrix = Matrix::Identity;
        bool hasContent = false;
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        bool dirty = true;
    };

    struct BattleWorldItemCardPresentation
    {
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        Vec3 worldOffset = Vec3::Zero;
        Vec2 worldSize = kBattleWorldItemListWorldSize;
        float visualScale = kBattleWorldItemDefaultScale;
    };

    bool Should_ShowBattleWorldItemCards(BattleTurnState state)
    {
        return state == BattleTurnState::ChooseItem;
    }

    wstring Resolve_ItemFallbackTitle(BattleItemType itemType)
    {
        switch (itemType)
        {
        case BattleItemType::HealingTint:
            return L"Healing Tint";
        case BattleItemType::EnergyTint:
            return L"Energy Tint";
        case BattleItemType::ReviveTint:
            return L"Revive Tint";
        case BattleItemType::Invalid:
        default:
            return L"Item";
        }
    }

    wstring Resolve_ItemTitle(const Item_Data& item)
    {
        if (!item.displayName.empty())
            return item.displayName;

        return Resolve_ItemFallbackTitle(item.itemType);
    }

    int32 Resolve_ItemPrimaryValue(const Item_Data& item)
    {
        switch (item.itemType)
        {
        case BattleItemType::HealingTint:
            return item.healHp;
        case BattleItemType::EnergyTint:
            return item.restoreAp;
        case BattleItemType::ReviveTint:
            return item.reviveHp;
        case BattleItemType::Invalid:
        default:
            return 0;
        }
    }

    wstring Resolve_ItemDescription(const Item_Data& item)
    {
        switch (item.itemType)
        {
        case BattleItemType::HealingTint:
            return L"Heal " + to_wstring(max(0, item.healHp)) + L"%";
        case BattleItemType::EnergyTint:
            return L"Restore " + to_wstring(max(0, item.restoreAp)) + L" AP";
        case BattleItemType::ReviveTint:
            return L"Revive " + to_wstring(max(0, item.reviveHp)) + L"% HP";
        case BattleItemType::Invalid:
        default:
            return L"";
        }
    }

    BattleWorldItemCardContent Build_ItemCardContent(
        const BattleWorldItemSlotDef& slot,
        const Item_Data& item)
    {
        BattleWorldItemCardContent content{};
        content.itemType = item.itemType;
        content.keyPrompt = slot.keyPrompt;
        content.title = Resolve_ItemTitle(item);
        content.description = Resolve_ItemDescription(item);
        content.subDescription = L"Count " + to_wstring(max(0, item.count));
        content.count = item.count;
        content.primaryValue = Resolve_ItemPrimaryValue(item);
        return content;
    }

    bool Is_SameContent(const BattleWorldItemCardContent& lhs, const BattleWorldItemCardContent& rhs)
    {
        return lhs.itemType == rhs.itemType &&
            lhs.keyPrompt == rhs.keyPrompt &&
            lhs.title == rhs.title &&
            lhs.description == rhs.description &&
            lhs.subDescription == rhs.subDescription &&
            lhs.count == rhs.count &&
            lhs.primaryValue == rhs.primaryValue;
    }

    Vec4 Resolve_ImageColor(const Vec4& color, bool selected, bool dimmed)
    {
        if (selected)
        {
            return Vec4(
                min(color.x * 1.18f + 0.08f, 1.f),
                min(color.y * 1.18f + 0.08f, 1.f),
                min(color.z * 1.18f + 0.08f, 1.f),
                color.w);
        }

        if (dimmed)
            return Vec4(color.x * 0.48f, color.y * 0.48f, color.z * 0.48f, color.w * 0.48f);

        return color;
    }

    Vec4 Resolve_TextColor(const Vec4& color, bool selected, bool dimmed)
    {
        if (selected)
        {
            return Vec4(
                min(color.x * 1.14f + 0.06f, 1.f),
                min(color.y * 1.14f + 0.06f, 1.f),
                min(color.z * 1.14f + 0.06f, 1.f),
                color.w);
        }

        if (dimmed)
            return Vec4(color.x * 0.60f, color.y * 0.60f, color.z * 0.60f, color.w * 0.55f);

        return color;
    }

    Vec4 Resolve_ItemTint(BattleItemType itemType)
    {
        switch (itemType)
        {
        case BattleItemType::HealingTint:
            return Vec4(118.f / 255.f, 152.f / 255.f, 104.f / 255.f, 1.f);
        case BattleItemType::EnergyTint:
            return Vec4(74.f / 255.f, 104.f / 255.f, 170.f / 255.f, 1.f);
        case BattleItemType::ReviveTint:
            return Vec4(172.f / 255.f, 132.f / 255.f, 91.f / 255.f, 1.f);
        case BattleItemType::Invalid:
        default:
            return Vec4(1.f, 1.f, 1.f, 1.f);
        }
    }

    BattleWorldItemCardDef Build_ItemCardDef(const BattleWorldItemCardContent& content)
    {
        BattleWorldItemCardDef def{};
        def.content = content;

        def.images.push_back(
            {
                BattleWorldItemCardImageKind::CardBackground,
                Vec2(388.f, 96.f),
                Vec2(690.f, 156.f),
                Resolve_ItemTint(content.itemType)
            });

        def.images.push_back(
            {
                BattleWorldItemCardImageKind::KeyFrame,
                Vec2(102.f, 96.f),
                Vec2(62.f, 62.f),
                Vec4(1.f, 1.f, 1.f, 0.95f)
            });

        def.texts.push_back(
            {
                def.content.keyPrompt,
                L"33_SubKR_Font",
                Vec2(102.f, 95.f),
                1.12f,
                Vec4(236.f / 255.f, 207.f / 255.f, 165.f / 255.f, 1.f),
                true
            });

        def.texts.push_back(
            {
                def.content.title,
                L"33_MainKR_Font",
                Vec2(158.f, 46.f),
                1.20f,
                Vec4(1.f, 0.92f, 0.78f, 1.f),
                false
            });

        def.texts.push_back(
            {
                def.content.description,
                L"33_SubKR_Font",
                Vec2(160.f, 94.f),
                0.80f,
                Vec4(0.95f, 0.90f, 0.80f, 0.96f),
                false
            });

        def.texts.push_back(
            {
                def.content.subDescription,
                L"33_SubKR_Font",
                Vec2(160.f, 128.f),
                0.72f,
                Vec4(0.86f, 0.92f, 1.f, 0.82f),
                false
            });

        return def;
    }

    BattleWorldItemCardPresentation Resolve_ItemPresentation(
        uint32 slotIndex,
        const BattleWorldItemCardDef& def,
        BattleTurnState state,
        BattleItemType selectedItemType)
    {
        UNREFERENCED_PARAMETER(selectedItemType);

        BattleWorldItemCardPresentation presentation{};

        if (!Should_ShowBattleWorldItemCards(state))
            return presentation;

        if (slotIndex >= kBattleWorldItemSlotCount)
            return presentation;

        presentation.visible = true;
        presentation.dimmed = def.content.count <= 0;
        presentation.worldOffset = kBattleWorldItemListOffsets[slotIndex];
        presentation.worldSize = kBattleWorldItemListWorldSize;
        presentation.visualScale = kBattleWorldItemDefaultScale;
        return presentation;
    }

    HRESULT Bind_WorldUIPlaneShaderState(
        ShaderCom* shader,
        const Matrix& worldMatrix,
        const Vec4& color)
    {
        CHECK_NULL(shader, E_FAIL);

        const Vec2 defaultPatternOffset = Vec2(0.f, 0.f);
        const Vec2 defaultPatternScale = Vec2(1.f, 1.f);
        const Vec2 defaultHoleCenter = Vec2(0.5f, 0.5f);
        const float defaultHoleRadius = 0.f;
        const float defaultHoleFeather = 0.05f;
        const float defaultViewportAspect = 1.f;
        const float defaultFillRatio = 1.f;
        const bool disabled = false;
        const uint32 fillOrigin = 0u;

        CHECK_FAILED(shader->Bind_Matrix("g_WorldMatrix", &worldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, shader, "g_ViewMatrix"), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, shader, "g_ProjMatrix"), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_Color", &color, sizeof(Vec4)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_PatternUvOffset", &defaultPatternOffset, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_PatternUvScale", &defaultPatternScale, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_UseGrayScale", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_IsOverlay", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleCenterUV", &defaultHoleCenter, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleRadius", &defaultHoleRadius, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_HoleFeather", &defaultHoleFeather, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_ViewportAspect", &defaultViewportAspect, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_UseFillRatio", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FillRatio", &defaultFillRatio, sizeof(float)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FillOrigin", &fillOrigin, sizeof(uint32)), E_FAIL);

        return S_OK;
    }

    class ScopedRenderTargetState final
    {
    public:
        explicit ScopedRenderTargetState(const ComPtr<Context>& context)
            : _context{ context }
        {
            if (_context == nullptr)
                return;

            _viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            _context->RSGetViewports(&_viewportCount, _viewports);
            _context->OMGetRenderTargets(1, _rtv.ReleaseAndGetAddressOf(), _dsv.ReleaseAndGetAddressOf());
            _context->OMGetBlendState(_blendState.ReleaseAndGetAddressOf(), _blendFactor, &_sampleMask);
            _context->OMGetDepthStencilState(_depthStencilState.ReleaseAndGetAddressOf(), &_stencilRef);
            _context->RSGetState(_rasterizerState.ReleaseAndGetAddressOf());
        }

        ~ScopedRenderTargetState()
        {
            if (_context == nullptr)
                return;

            ID3D11RenderTargetView* rtvs[] = { _rtv.Get() };
            _context->OMSetRenderTargets(1, rtvs, _dsv.Get());

            if (_viewportCount > 0)
                _context->RSSetViewports(_viewportCount, _viewports);

            _context->OMSetBlendState(_blendState.Get(), _blendFactor, _sampleMask);
            _context->OMSetDepthStencilState(_depthStencilState.Get(), _stencilRef);
            _context->RSSetState(_rasterizerState.Get());
        }

    private:
        ComPtr<Context> _context{};
        ComPtr<ID3D11RenderTargetView> _rtv{};
        ComPtr<ID3D11DepthStencilView> _dsv{};
        ComPtr<ID3D11BlendState> _blendState{};
        ComPtr<ID3D11DepthStencilState> _depthStencilState{};
        ComPtr<ID3D11RasterizerState> _rasterizerState{};
        D3D11_VIEWPORT _viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        uint32 _viewportCount = 0;
        float _blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
        uint32 _sampleMask = 0xffffffff;
        uint32 _stencilRef = 0;
    };

    class IWorldUIItemCardSurface
    {
    public:
        virtual ~IWorldUIItemCardSurface() = default;

        virtual HRESULT Compose(
            const BattleWorldItemCardDef& def,
            bool selected,
            bool dimmed) = 0;
        virtual HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const = 0;
    };

    class RenderTargetComposedBattleWorldItemCardSurface final : public IWorldUIItemCardSurface
    {
    public:
        RenderTargetComposedBattleWorldItemCardSurface(
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            Shared<Texture> cardTexture,
            Shared<Texture> keyFrameTexture)
            : _device{ device }
            , _context{ context }
            , _cardTexture{ std::move(cardTexture) }
            , _keyFrameTexture{ std::move(keyFrameTexture) }
        {
        }

        HRESULT Initialize(uint32 slotIndex)
        {
            _renderTarget = RenderTarget::Create(
                L"BattleWorldItemCardSurface_" + to_wstring(slotIndex),
                _device,
                _context,
                kBattleWorldItemCardPixelWidth,
                kBattleWorldItemCardPixelHeight,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                Vec4(0.f, 0.f, 0.f, 0.f),
                false);
            CHECK_NULL(_renderTarget, E_FAIL);

            _spriteBatch = make_unique<SpriteBatch>(_context.Get());
            CHECK_NULL(_spriteBatch, E_FAIL);

            return S_OK;
        }

        HRESULT Compose(
            const BattleWorldItemCardDef& def,
            bool selected,
            bool dimmed) override
        {
            CHECK_NULL(_renderTarget, E_FAIL);
            CHECK_NULL(_cardTexture, E_FAIL);
            CHECK_NULL(_keyFrameTexture, E_FAIL);

            ScopedRenderTargetState restoreState{ _context };

            ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            CHECK_FAILED(_renderTarget->Begin(true, false), E_FAIL);

            for (const auto& image : def.images)
                CHECK_FAILED(Draw_CompositionImage(image, selected, dimmed), E_FAIL);

            for (const auto& text : def.texts)
                Draw_CompositionText(text, selected, dimmed);

            _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
            _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

            return S_OK;
        }

        HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const override
        {
            CHECK_NULL(_renderTarget, E_FAIL);
            return _renderTarget->Bind_ShaderResource(shader, name);
        }

        static Unique<IWorldUIItemCardSurface> Create(
            uint32 slotIndex,
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            Shared<Texture> cardTexture,
            Shared<Texture> keyFrameTexture)
        {
            auto surface = make_unique<RenderTargetComposedBattleWorldItemCardSurface>(
                device,
                context,
                std::move(cardTexture),
                std::move(keyFrameTexture));
            if (FAILED(surface->Initialize(slotIndex)))
                return nullptr;

            return surface;
        }

    private:
        Shared<Texture> Resolve_CompositionTexture(BattleWorldItemCardImageKind kind) const
        {
            switch (kind)
            {
            case BattleWorldItemCardImageKind::CardBackground:
                return _cardTexture;
            case BattleWorldItemCardImageKind::KeyFrame:
                return _keyFrameTexture;
            default:
                return nullptr;
            }
        }

        HRESULT Draw_CompositionImage(const BattleWorldItemCardImageDef& image, bool selected, bool dimmed)
        {
            Shared<Texture> texture = Resolve_CompositionTexture(image.kind);
            CHECK_NULL(texture, E_FAIL);
            CHECK_NULL(_spriteBatch, E_FAIL);

            const auto& srvs = texture->Get_SRVs();
            if (srvs.empty() || srvs[0] == nullptr)
                return E_FAIL;

            RECT destRect{};
            destRect.left = static_cast<LONG>(roundf(image.center.x - image.size.x * 0.5f));
            destRect.top = static_cast<LONG>(roundf(image.center.y - image.size.y * 0.5f));
            destRect.right = static_cast<LONG>(roundf(image.center.x + image.size.x * 0.5f));
            destRect.bottom = static_cast<LONG>(roundf(image.center.y + image.size.y * 0.5f));

            _spriteBatch->Begin(
                SpriteSortMode_Deferred,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                XMMatrixIdentity());

            const Vec4 drawColor = Resolve_ImageColor(image.color, selected, dimmed);

            _spriteBatch->Draw(
                srvs[0].Get(),
                destRect,
                XMVectorSet(drawColor.x, drawColor.y, drawColor.z, drawColor.w));

            _spriteBatch->End();

            return S_OK;
        }

        void Draw_CompositionText(const BattleWorldItemCardTextDef& text, bool selected, bool dimmed)
        {
            if (text.text.empty() || text.fontTag.empty())
                return;

            Vec2 position = text.position;
            if (text.centered)
            {
                const Vec2 measured = GAME->Measure_Font(text.fontTag, text.text.c_str()) * text.scale;
                position -= measured * 0.5f;
            }

            const Vec4 drawColor = Resolve_TextColor(text.color, selected, dimmed);

            GAME->Draw_Font(
                text.fontTag,
                text.text.c_str(),
                position,
                XMVectorSet(drawColor.x, drawColor.y, drawColor.z, drawColor.w),
                0.f,
                Vec2(0.f, 0.f),
                text.scale);
        }

        ComPtr<Device> _device{};
        ComPtr<Context> _context{};
        Shared<RenderTarget> _renderTarget{};
        Shared<Texture> _cardTexture{};
        Shared<Texture> _keyFrameTexture{};
        Unique<SpriteBatch> _spriteBatch{};
    };

}

struct BattleWorldUI_ItemCards::Impl
{
    Shared<ShaderCom> shader{};
    Shared<VIBuffer_Rect> viBuffer{};
    Shared<Texture> cardTexture{};
    Shared<Texture> keyFrameTexture{};
    array<BattleWorldItemCardInstance, kBattleWorldItemSlotCount> cards{};
};

BattleWorldUI_ItemCards::BattleWorldUI_ItemCards(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI_ItemCards::~BattleWorldUI_ItemCards() = default;

HRESULT BattleWorldUI_ItemCards::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    _impl = make_unique<Impl>();
    CHECK_NULL(_impl, E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_UI"), _impl->shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _impl->viBuffer), E_FAIL);

    _impl->cardTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUIImage(
        _device,
        _context,
        kBattleWorldItemPanelJsonPath,
        "Image_Healing_Tint_Frame",
        kBattleWorldItemCardTextureGuid);
    CHECK_NULL(_impl->cardTexture, E_FAIL);

    _impl->keyFrameTexture = BattleWorldUI_JsonTextureResolver::Load_TextureFromUIImage(
        _device,
        _context,
        kBattleWorldItemPanelJsonPath,
        "Image_KeyFrame_Q",
        kBattleWorldKeyFrameTextureGuid);
    CHECK_NULL(_impl->keyFrameTexture, E_FAIL);

    for (uint32 slotIndex = 0; slotIndex < kBattleWorldItemSlotCount; ++slotIndex)
    {
        _impl->cards[slotIndex].surface = RenderTargetComposedBattleWorldItemCardSurface::Create(
            slotIndex,
            _device,
            _context,
            _impl->cardTexture,
            _impl->keyFrameTexture);
        CHECK_NULL(_impl->cards[slotIndex].surface, E_FAIL);
    }

    Hide();
    return S_OK;
}

void BattleWorldUI_ItemCards::Show_ForBattleState(
    const Vec3& anchorWorldPos,
    BattleTurnState currentState,
    BattleItemType selectedItemType)
{
    if (_impl == nullptr || !Should_ShowBattleWorldItemCards(currentState))
    {
        Hide();
        return;
    }

    bool anyVisible = false;
    for (uint32 slotIndex = 0; slotIndex < kBattleWorldItemSlotCount; ++slotIndex)
    {
        auto& card = _impl->cards[slotIndex];
        card.visible = false;

        const BattleWorldItemSlotDef& slot = kBattleWorldItemSlots[slotIndex];
        const Item_Data* item = CLIENT->Get_Item(slot.itemType);
        if (item == nullptr || !item->IsValid())
        {
            card.hasContent = false;
            continue;
        }

        const BattleWorldItemCardContent content = Build_ItemCardContent(slot, *item);
        if (!card.hasContent || !Is_SameContent(card.def.content, content))
        {
            card.def = Build_ItemCardDef(content);
            card.hasContent = true;
            card.dirty = true;
        }

        const BattleWorldItemCardPresentation presentation =
            Resolve_ItemPresentation(slotIndex, card.def, currentState, selectedItemType);

        if (card.selected != presentation.selected || card.dimmed != presentation.dimmed)
        {
            card.selected = presentation.selected;
            card.dimmed = presentation.dimmed;
            card.dirty = true;
        }

        if (!presentation.visible)
            continue;

        if (card.dirty)
        {
            if (card.surface == nullptr || FAILED(card.surface->Compose(card.def, card.selected, card.dimmed)))
                continue;

            card.dirty = false;
        }

        const Vec3 position = anchorWorldPos + presentation.worldOffset;
        card.worldMatrix =
            Matrix::CreateScale(
                presentation.worldSize.x * presentation.visualScale,
                presentation.worldSize.y * presentation.visualScale,
                1.f) *
            Matrix::CreateTranslation(position.x, position.y, position.z);
        card.visible = true;
        anyVisible = true;
    }

    Set_Visible(anyVisible);
}

void BattleWorldUI_ItemCards::Hide()
{
    Set_Visible(false);

    if (_impl == nullptr)
        return;

    for (auto& card : _impl->cards)
        card.visible = false;
}

void BattleWorldUI_ItemCards::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::UIEffect, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI_ItemCards::Render()
{
    if (!Is_Visible() || _impl == nullptr || _impl->shader == nullptr || _impl->viBuffer == nullptr)
        return S_OK;

    const Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);

    for (const auto& card : _impl->cards)
    {
        if (!card.visible || card.surface == nullptr)
            continue;

        CHECK_FAILED(Bind_WorldUIPlaneShaderState(_impl->shader.get(), card.worldMatrix, color), E_FAIL);
        CHECK_FAILED(card.surface->Bind_ShaderResource(_impl->shader.get(), "g_Texture"), E_FAIL);
        CHECK_FAILED(_impl->shader->Begin(0), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Bind_Resources(), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Render(), E_FAIL);
    }

    return S_OK;
}

Shared<GameObject> BattleWorldUI_ItemCards::Clone(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    return nullptr;
}

void BattleWorldUI_ItemCards::Free()
{
    if (_impl)
    {
        for (auto& card : _impl->cards)
            card.surface.reset();

        _impl->cardTexture.reset();
        _impl->keyFrameTexture.reset();
        _impl->shader.reset();
        _impl->viBuffer.reset();
        _impl.reset();
    }

    __super::Free();
}

Shared<BattleWorldUI_ItemCards> BattleWorldUI_ItemCards::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI_ItemCards>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

NS_END
