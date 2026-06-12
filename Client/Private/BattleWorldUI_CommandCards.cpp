#include "BattleWorldUI_CommandCards.h"

#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

NS_BEGIN(Client)

namespace
{
    constexpr char kBattleWorldAttackCardTextureGuid[] = "bec2ba8a-a265-40d8-9bd1-bc85420ebaa2";
    constexpr char kBattleWorldKeyFrameTextureGuid[] = "72de5c45-b20f-490d-9d69-396a830b514f";
    constexpr uint32 kBattleWorldCardPixelWidth = 768;
    constexpr uint32 kBattleWorldCardPixelHeight = 192;
    const Vec2 kBattleWorldCommandListWorldSize = Vec2(0.86f, 0.215f);
    const Vec2 kBattleWorldCommandTargetingWorldSize = Vec2(0.98f, 0.245f);
    const Vec3 kBattleWorldCommandTargetingOffset = Vec3(0.f, 0.62f, 0.f);
    constexpr float kBattleWorldCommandDefaultScale = 1.f;
    constexpr float kBattleWorldCommandSelectedScale = 1.06f;

    enum class BattleWorldCardImageKind
    {
        CommandBackground,
        KeyFrame,
    };

    enum class BattleWorldCommandKind
    {
        None,
        Item,
        Skill,
        Attack,
    };

    enum class BattleWorldCommandVisibility
    {
        AllCommands,
        SelectedCommandOnly,
    };

    class IWorldUICardSurface;

    struct BattleWorldCardTextDef
    {
        wstring text;
        wstring fontTag;
        Vec2 position{};
        float scale = 1.f;
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
        bool centered = false;
    };

    struct BattleWorldCardImageDef
    {
        BattleWorldCardImageKind kind = BattleWorldCardImageKind::CommandBackground;
        Vec2 center{};
        Vec2 size{};
        Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
    };

    struct BattleWorldCommandCardContent
    {
        wstring id;
        BattleWorldCommandKind command = BattleWorldCommandKind::None;
        wstring key;
        wstring title;
        wstring description;
    };

    struct BattleWorldCommandCardPlacement
    {
        Vec3 chooseActionOffset = Vec3::Zero;
        Vec3 targetingOffset = kBattleWorldCommandTargetingOffset;
        Vec2 chooseActionWorldSize = kBattleWorldCommandListWorldSize;
        Vec2 targetingWorldSize = kBattleWorldCommandTargetingWorldSize;
    };

    struct BattleWorldCommandCardDef
    {
        BattleWorldCommandCardContent content{};
        BattleWorldCommandCardPlacement placement{};
        vector<BattleWorldCardImageDef> images{};
        vector<BattleWorldCardTextDef> texts{};
    };

    struct BattleWorldCommandCardInstance
    {
        BattleWorldCommandCardDef def{};
        Unique<IWorldUICardSurface> surface{};
        Matrix worldMatrix = Matrix::Identity;
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        bool dirty = true;
    };

    struct BattleWorldCommandCardPresentation
    {
        bool visible = false;
        bool selected = false;
        bool dimmed = false;
        Vec3 worldOffset = Vec3::Zero;
        Vec2 worldSize = kBattleWorldCommandListWorldSize;
        float visualScale = kBattleWorldCommandDefaultScale;
    };

    BattleWorldCommandKind Resolve_SelectedCommand(BattleTurnState state)
    {
        switch (state)
        {
        case BattleTurnState::ChooseItem:
        case BattleTurnState::ItemTargeting:
            return BattleWorldCommandKind::Item;

        case BattleTurnState::ChooseSkill:
        case BattleTurnState::SkillTargeting:
            return BattleWorldCommandKind::Skill;

        case BattleTurnState::AttackTargeting:
            return BattleWorldCommandKind::Attack;

        case BattleTurnState::ChooseAction:
        case BattleTurnState::None:
        default:
            return BattleWorldCommandKind::None;
        }
    }

    bool Should_ShowBattleWorldCommandCards(BattleTurnState state)
    {
        switch (state)
        {
        case BattleTurnState::ChooseAction:
        case BattleTurnState::ItemTargeting:
        case BattleTurnState::SkillTargeting:
        case BattleTurnState::AttackTargeting:
            return true;

        default:
            return false;
        }
    }

    BattleWorldCommandVisibility Resolve_CommandVisibility(BattleTurnState state)
    {
        switch (state)
        {
        case BattleTurnState::ChooseAction:
            return BattleWorldCommandVisibility::AllCommands;

        case BattleTurnState::ItemTargeting:
        case BattleTurnState::SkillTargeting:
        case BattleTurnState::AttackTargeting:
            return BattleWorldCommandVisibility::SelectedCommandOnly;

        case BattleTurnState::ChooseItem:
        case BattleTurnState::ChooseSkill:
        default:
            return BattleWorldCommandVisibility::SelectedCommandOnly;
        }
    }

    BattleWorldCommandCardPresentation Resolve_CommandPresentation(
        const BattleWorldCommandCardDef& def,
        BattleTurnState state)
    {
        BattleWorldCommandCardPresentation presentation{};

        if (!Should_ShowBattleWorldCommandCards(state))
            return presentation;

        const BattleWorldCommandKind selectedCommand = Resolve_SelectedCommand(state);
        const BattleWorldCommandVisibility visibility = Resolve_CommandVisibility(state);

        if (visibility == BattleWorldCommandVisibility::AllCommands)
        {
            presentation.visible = true;
            presentation.worldOffset = def.placement.chooseActionOffset;
            presentation.worldSize = def.placement.chooseActionWorldSize;
            presentation.visualScale = kBattleWorldCommandDefaultScale;
            return presentation;
        }

        if (selectedCommand == BattleWorldCommandKind::None ||
            def.content.command != selectedCommand)
        {
            return presentation;
        }

        presentation.visible = true;
        presentation.selected = true;
        presentation.worldOffset = def.placement.targetingOffset;
        presentation.worldSize = def.placement.targetingWorldSize;
        presentation.visualScale = kBattleWorldCommandSelectedScale;
        return presentation;
    }

    Vec4 Resolve_ImageColor(const Vec4& color, bool selected, bool dimmed)
    {
        if (selected)
        {
            return Vec4(
                min(color.x * 1.22f + 0.08f, 1.f),
                min(color.y * 1.22f + 0.08f, 1.f),
                min(color.z * 1.22f + 0.08f, 1.f),
                color.w);
        }

        if (dimmed)
            return Vec4(color.x * 0.55f, color.y * 0.55f, color.z * 0.55f, color.w * 0.58f);

        return color;
    }

    Vec4 Resolve_TextColor(const Vec4& color, bool selected, bool dimmed)
    {
        if (selected)
        {
            return Vec4(
                min(color.x * 1.16f + 0.06f, 1.f),
                min(color.y * 1.16f + 0.06f, 1.f),
                min(color.z * 1.16f + 0.06f, 1.f),
                color.w);
        }

        if (dimmed)
            return Vec4(color.x * 0.68f, color.y * 0.68f, color.z * 0.68f, color.w * 0.62f);

        return color;
    }

    BattleWorldCommandCardDef Make_CommandCardDef(
        const wstring& id,
        BattleWorldCommandKind command,
        const wstring& key,
        const wstring& title,
        const wstring& description,
        const Vec3& chooseActionOffset,
        const Vec4& backgroundTint)
    {
        BattleWorldCommandCardDef def{};
        def.content.id = id;
        def.content.command = command;
        def.content.key = key;
        def.content.title = title;
        def.content.description = description;
        def.placement.chooseActionOffset = chooseActionOffset;
        def.placement.targetingOffset = kBattleWorldCommandTargetingOffset;
        def.placement.chooseActionWorldSize = kBattleWorldCommandListWorldSize;
        def.placement.targetingWorldSize = kBattleWorldCommandTargetingWorldSize;

        def.images.push_back(
            {
                BattleWorldCardImageKind::CommandBackground,
                Vec2(384.f, 96.f),
                Vec2(680.f, 150.f),
                backgroundTint
            });

        def.images.push_back(
            {
                BattleWorldCardImageKind::KeyFrame,
                Vec2(108.f, 96.f),
                Vec2(62.f, 62.f),
                Vec4(1.f, 1.f, 1.f, 0.95f)
            });

        def.texts.push_back(
            {
                def.content.key,
                L"33_SubKR_Font",
                Vec2(108.f, 95.f),
                1.15f,
                Vec4(236.f / 255.f, 207.f / 255.f, 165.f / 255.f, 1.f),
                true
            });

        def.texts.push_back(
            {
                def.content.title,
                L"33_MainKR_Font",
                Vec2(164.f, 52.f),
                1.45f,
                Vec4(1.f, 0.92f, 0.78f, 1.f),
                false
            });

        def.texts.push_back(
            {
                def.content.description,
                L"33_SubKR_Font",
                Vec2(166.f, 106.f),
                0.82f,
                Vec4(0.95f, 0.88f, 0.75f, 0.92f),
                false
            });

        return def;
    }

    vector<BattleWorldCommandCardDef> Build_CommandCardDefs()
    {
        return
        {
            Make_CommandCardDef(
                L"Command_Item",
                BattleWorldCommandKind::Item,
                L"W",
                L"아이템",
                L"아이템 사용",
                Vec3(0.f, 0.72f, 0.f),
                Vec4(118.f / 255.f, 152.f / 255.f, 104.f / 255.f, 1.f)),
            Make_CommandCardDef(
                L"Command_Skill",
                BattleWorldCommandKind::Skill,
                L"E",
                L"기술",
                L"스킬 선택",
                Vec3(0.f, 0.48f, 0.f),
                Vec4(84.f / 255.f, 128.f / 255.f, 188.f / 255.f, 1.f)),
            Make_CommandCardDef(
                L"Command_Attack",
                BattleWorldCommandKind::Attack,
                L"F",
                L"공격",
                L"기본 공격",
                Vec3(0.f, 0.24f, 0.f),
                Vec4(188.f / 255.f, 144.f / 255.f, 85.f / 255.f, 1.f)),
        };
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

    class IWorldUICardSurface
    {
    public:
        virtual ~IWorldUICardSurface() = default;

        virtual HRESULT Compose(
            const BattleWorldCommandCardDef& def,
            bool selected,
            bool dimmed) = 0;
        virtual HRESULT Bind_ShaderResource(ShaderCom* shader, const char* name) const = 0;
    };

    class RenderTargetComposedBattleWorldCardSurface final : public IWorldUICardSurface
    {
    public:
        RenderTargetComposedBattleWorldCardSurface(
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            Shared<Texture> backgroundTexture,
            Shared<Texture> keyFrameTexture)
            : _device{ device }
            , _context{ context }
            , _backgroundTexture{ std::move(backgroundTexture) }
            , _keyFrameTexture{ std::move(keyFrameTexture) }
        {
        }

        HRESULT Initialize(const wstring& id)
        {
            _renderTarget = RenderTarget::Create(
                L"BattleWorldCommandCardSurface_" + id,
                _device,
                _context,
                kBattleWorldCardPixelWidth,
                kBattleWorldCardPixelHeight,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                Vec4(0.f, 0.f, 0.f, 0.f),
                false);
            CHECK_NULL(_renderTarget, E_FAIL);

            _spriteBatch = make_unique<SpriteBatch>(_context.Get());
            CHECK_NULL(_spriteBatch, E_FAIL);

            return S_OK;
        }

        HRESULT Compose(
            const BattleWorldCommandCardDef& def,
            bool selected,
            bool dimmed) override
        {
            CHECK_NULL(_renderTarget, E_FAIL);
            CHECK_NULL(_backgroundTexture, E_FAIL);
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

        static Unique<IWorldUICardSurface> Create(
            const wstring& id,
            const ComPtr<Device>& device,
            const ComPtr<Context>& context,
            Shared<Texture> backgroundTexture,
            Shared<Texture> keyFrameTexture)
        {
            auto surface = make_unique<RenderTargetComposedBattleWorldCardSurface>(
                device,
                context,
                std::move(backgroundTexture),
                std::move(keyFrameTexture));
            if (FAILED(surface->Initialize(id)))
                return nullptr;

            return surface;
        }

    private:
        Shared<Texture> Resolve_CompositionTexture(BattleWorldCardImageKind kind) const
        {
            switch (kind)
            {
            case BattleWorldCardImageKind::CommandBackground:
                return _backgroundTexture;
            case BattleWorldCardImageKind::KeyFrame:
                return _keyFrameTexture;
            default:
                return nullptr;
            }
        }

        HRESULT Draw_CompositionImage(const BattleWorldCardImageDef& image, bool selected, bool dimmed)
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

        void Draw_CompositionText(const BattleWorldCardTextDef& text, bool selected, bool dimmed)
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
        Shared<Texture> _backgroundTexture{};
        Shared<Texture> _keyFrameTexture{};
        Unique<SpriteBatch> _spriteBatch{};
    };

    Shared<Texture> Load_TextureFromGuid(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const string& guid)
    {
        const wstring texturePath = GAME->Resolve_AssetPath(guid);
        if (texturePath.empty())
        {
            LOG_WARN("Battle world command card texture guid could not be resolved. guid='{}'", guid);
            return nullptr;
        }

        return Texture::Create(device, context, texturePath.c_str(), 1);
    }
}

struct BattleWorldUI_CommandCards::Impl
{
    Shared<ShaderCom> shader{};
    Shared<VIBuffer_Rect> viBuffer{};
    Shared<Texture> backgroundTexture{};
    Shared<Texture> keyFrameTexture{};
    vector<BattleWorldCommandCardInstance> cards{};
};

BattleWorldUI_CommandCards::BattleWorldUI_CommandCards(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI_CommandCards::~BattleWorldUI_CommandCards() = default;

HRESULT BattleWorldUI_CommandCards::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);

    _impl = make_unique<Impl>();
    CHECK_NULL(_impl, E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_UI"), _impl->shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _impl->viBuffer), E_FAIL);

    _impl->backgroundTexture = Load_TextureFromGuid(_device, _context, kBattleWorldAttackCardTextureGuid);
    CHECK_NULL(_impl->backgroundTexture, E_FAIL);

    _impl->keyFrameTexture = Load_TextureFromGuid(_device, _context, kBattleWorldKeyFrameTextureGuid);
    CHECK_NULL(_impl->keyFrameTexture, E_FAIL);

    _impl->cards.clear();

    for (auto& def : Build_CommandCardDefs())
    {
        BattleWorldCommandCardInstance card{};
        card.def = std::move(def);
        card.surface = RenderTargetComposedBattleWorldCardSurface::Create(
            card.def.content.id,
            _device,
            _context,
            _impl->backgroundTexture,
            _impl->keyFrameTexture);
        CHECK_NULL(card.surface, E_FAIL);

        _impl->cards.push_back(std::move(card));
    }

    Hide();
    return S_OK;
}

void BattleWorldUI_CommandCards::Show_ForBattleState(const Vec3& anchorWorldPos, BattleTurnState currentState)
{
    if (_impl == nullptr || !Should_ShowBattleWorldCommandCards(currentState))
    {
        Hide();
        return;
    }

    bool anyVisible = false;
    for (auto& card : _impl->cards)
    {
        card.visible = false;

        const BattleWorldCommandCardPresentation presentation =
            Resolve_CommandPresentation(card.def, currentState);

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
            if (FAILED(card.surface->Compose(card.def, card.selected, card.dimmed)))
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

void BattleWorldUI_CommandCards::Hide()
{
    Set_Visible(false);

    if (_impl == nullptr)
        return;

    for (auto& card : _impl->cards)
        card.visible = false;
}

void BattleWorldUI_CommandCards::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::UIEffect, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI_CommandCards::Render()
{
    if (!Is_Visible() || _impl == nullptr || _impl->shader == nullptr || _impl->viBuffer == nullptr)
        return S_OK;

    const Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
    const Vec2 defaultPatternOffset = Vec2(0.f, 0.f);
    const Vec2 defaultPatternScale = Vec2(1.f, 1.f);
    const Vec2 defaultHoleCenter = Vec2(0.5f, 0.5f);
    const float defaultHoleRadius = 0.f;
    const float defaultHoleFeather = 0.05f;
    const float defaultViewportAspect = 1.f;
    const float defaultFillRatio = 1.f;
    const bool disabled = false;
    const uint32 fillOrigin = 0u;

    for (const auto& card : _impl->cards)
    {
        if (!card.visible || card.surface == nullptr)
            continue;

        CHECK_FAILED(_impl->shader->Bind_Matrix("g_WorldMatrix", &card.worldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _impl->shader.get(), "g_ViewMatrix"), E_FAIL);
        CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _impl->shader.get(), "g_ProjMatrix"), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_Color", &color, sizeof(Vec4)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_PatternUvOffset", &defaultPatternOffset, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_PatternUvScale", &defaultPatternScale, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_UseGrayScale", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_IsOverlay", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_HoleCenterUV", &defaultHoleCenter, sizeof(Vec2)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_HoleRadius", &defaultHoleRadius, sizeof(float)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_HoleFeather", &defaultHoleFeather, sizeof(float)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_ViewportAspect", &defaultViewportAspect, sizeof(float)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_UseFillRatio", &disabled, sizeof(bool)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_FillRatio", &defaultFillRatio, sizeof(float)), E_FAIL);
        CHECK_FAILED(_impl->shader->Bind_RawValue("g_FillOrigin", &fillOrigin, sizeof(uint32)), E_FAIL);
        CHECK_FAILED(card.surface->Bind_ShaderResource(_impl->shader.get(), "g_Texture"), E_FAIL);
        CHECK_FAILED(_impl->shader->Begin(0), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Bind_Resources(), E_FAIL);
        CHECK_FAILED(_impl->viBuffer->Render(), E_FAIL);
    }

    return S_OK;
}

Shared<GameObject> BattleWorldUI_CommandCards::Clone(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    return nullptr;
}

void BattleWorldUI_CommandCards::Free()
{
    if (_impl)
    {
        _impl->cards.clear();
        _impl->backgroundTexture.reset();
        _impl->keyFrameTexture.reset();
        _impl->shader.reset();
        _impl->viBuffer.reset();
        _impl.reset();
    }

    __super::Free();
}

Shared<BattleWorldUI_CommandCards> BattleWorldUI_CommandCards::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI_CommandCards>(device, context);
    if (FAILED(instance->Initialize(nullptr)))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

NS_END
