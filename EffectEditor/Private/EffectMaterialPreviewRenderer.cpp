#include "EffectMaterialPreviewRenderer.h"

#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_PointParticle.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const char* kFallbackTextureId = "Texture_Effect_DefaultTexture";
    constexpr const wchar_t* kEffectSpriteShaderId = L"Shader_EffectSprite";
    constexpr const wchar_t* kPointParticleBufferId = L"Com_VIBuffer_PointParticle";
    constexpr int kOpacitySourceAlpha = 0;
    constexpr int kOpacitySourceRed = 1;
    constexpr int kOpacitySourceLuminance = 2;

    int Resolve_OpacitySourceIndex(const string& opacitySource)
    {
        if (opacitySource == "Alpha" || opacitySource == "alpha")
            return kOpacitySourceAlpha;

        if (opacitySource == "Red" || opacitySource == "red")
            return kOpacitySourceRed;

        if (opacitySource == "Luminance" || opacitySource == "luminance")
            return kOpacitySourceLuminance;

        return kOpacitySourceAlpha;
    }

    template <typename T>
    Shared<T> Clone_Component(uint32 levelIndex, const wstring& prototypeTag, void* arg = nullptr)
    {
        if (GAME == nullptr)
            return nullptr;

        return dynamic_pointer_cast<T>(
            GAME->Clone_Prototype(Prototype::Comopnent, levelIndex, prototypeTag, arg)
        );
    }
}

EffectMaterialPreviewRenderer::EffectMaterialPreviewRenderer(uint32 previewTextureSize)
    : _previewTextureSize{ max(1u, previewTextureSize) }
{
}

void EffectMaterialPreviewRenderer::Render_Material(const EffectMaterialInstanceData& material)
{
    _statusMessage.clear();

    if (EDITOR == nullptr || GAME == nullptr)
    {
        _statusMessage = "Preview renderer is not ready.";
        return;
    }

    if (!Ready_RenderTarget() ||
        !Ready_Shader() ||
        !Ready_Texture(material.mainTextureId) ||
        !Ready_NoiseTexture(material.noiseTextureId) ||
        !Ready_Buffer())
        return;

    ID3D11ShaderResourceView* nullSrvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);

    if (FAILED(_renderTarget->Begin(true, true)))
    {
        _statusMessage = "Preview render target begin failed.";
        return;
    }

    const HRESULT hr = Render_Sprite(material);
    GAME->Bind_BackBuffer();

    if (FAILED(hr))
        _statusMessage = "Preview draw failed.";
}

ID3D11ShaderResourceView* EffectMaterialPreviewRenderer::Get_SRV() const
{
    return _renderTarget == nullptr ? nullptr : _renderTarget->Get_SRV();
}

const string& EffectMaterialPreviewRenderer::Get_StatusMessage() const
{
    return _statusMessage;
}

void EffectMaterialPreviewRenderer::Handle_ViewportInput(bool isHovered)
{
    if (!isHovered)
        return;

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        _yawRadians += delta.x * 0.01f;
        _pitchRadians = clamp(_pitchRadians + delta.y * 0.01f, -1.2f, 1.2f);
    }

    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.f)
        _distance = clamp(_distance - wheel * 0.25f, 1.5f, 8.f);
}

bool EffectMaterialPreviewRenderer::Ready_RenderTarget()
{
    if (_renderTarget != nullptr)
        return true;

    _renderTarget = RenderTarget::Create(
        EDITOR->Get_Device(),
        EDITOR->Get_Context(),
        _previewTextureSize,
        _previewTextureSize,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4{ 0.03f, 0.035f, 0.045f, 1.f },
        true
    );

    if (_renderTarget == nullptr)
    {
        _statusMessage = "Preview render target creation failed.";
        return false;
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_Shader()
{
    if (_shader != nullptr)
        return true;

    _shader = Internal::Clone_Component<ShaderCom>(ETOI(LevelType::Static), Internal::kEffectSpriteShaderId);
    if (_shader == nullptr)
    {
        _statusMessage = "Shader_EffectSprite prototype is missing.";
        return false;
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_Texture(const string& textureId)
{
    const string requestedTextureId = textureId.empty() ? Internal::kFallbackTextureId : textureId;
    if (_texture != nullptr && _loadedTextureId == requestedTextureId)
        return true;

    _texture = Internal::Clone_Component<Texture>(ETOI(LevelType::GamePlay), String::ToWString(requestedTextureId));
    _loadedTextureId = requestedTextureId;

    if (_texture != nullptr)
        return true;

    if (requestedTextureId == Internal::kFallbackTextureId)
    {
        _statusMessage = "Preview texture prototype is missing.";
        return false;
    }

    _texture = Internal::Clone_Component<Texture>(ETOI(LevelType::GamePlay), String::ToWString(Internal::kFallbackTextureId));
    _loadedTextureId = Internal::kFallbackTextureId;

    if (_texture == nullptr)
    {
        _statusMessage = "Preview texture and fallback texture are missing.";
        return false;
    }

    _statusMessage = "Preview texture missing. Fallback texture is shown.";
    return true;
}

bool EffectMaterialPreviewRenderer::Ready_NoiseTexture(const string& noiseTextureId)
{
    if (noiseTextureId.empty())
    {
        _noiseTexture = nullptr;
        _loadedNoiseTextureId.clear();
        return true;
    }

    if (_noiseTexture != nullptr && _loadedNoiseTextureId == noiseTextureId)
        return true;

    _noiseTexture = Internal::Clone_Component<Texture>(ETOI(LevelType::GamePlay), String::ToWString(noiseTextureId));
    _loadedNoiseTextureId = noiseTextureId;

    if (_noiseTexture == nullptr)
    {
        _loadedNoiseTextureId.clear();
        _statusMessage = "Noise texture missing. Preview continues without noise.";
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_Buffer()
{
    if (_buffer != nullptr)
        return true;

    PointParticleDesc desc{};
    desc.instanceCount = 1;
    desc.center = Vec3{ 0.f, 0.f, 0.f };
    desc.range = Vec3{ 0.f, 0.f, 0.f };
    desc.scale = Vec2{ 2.f, 2.f };
    desc.pivot = Vec3{ 0.f, 0.f, 0.f };
    desc.speed = Vec2{ 0.f, 0.f };
    desc.lifeTime = Vec2{ 100.f, 100.f };
    desc.loop = true;
    desc.drawMode = PointParticleDrawMode::Direct;

    _buffer = Internal::Clone_Component<VIBufferCom_PointParticle>(
        ETOI(LevelType::Static),
        Internal::kPointParticleBufferId,
        &desc
    );

    if (_buffer == nullptr)
    {
        _statusMessage = "Point particle preview buffer prototype is missing.";
        return false;
    }

    return true;
}

HRESULT EffectMaterialPreviewRenderer::Render_Sprite(const EffectMaterialInstanceData& material) const
{
    const float cosPitch = cosf(_pitchRadians);
    const Vec3 eye{
        sinf(_yawRadians) * cosPitch * _distance,
        sinf(_pitchRadians) * _distance,
        -cosf(_yawRadians) * cosPitch * _distance
    };

    const Matrix viewMatrix = XMMatrixLookAtLH(
        XMVectorSet(eye.x, eye.y, eye.z, 1.f),
        XMVectorSet(0.f, 0.f, 0.f, 1.f),
        XMVectorSet(0.f, 1.f, 0.f, 0.f)
    );
    const Matrix projMatrix = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(35.f),
        1.f,
        0.1f,
        100.f
    );

    const Vec4 billboardTarget = Vec4(eye.x, eye.y, eye.z, 1.f);
    const Vec4 spriteParams = Vec4(
        material.intensity,
        max(material.opacityPower, 0.0001f),
        material.noiseStrength,
        _noiseTexture != nullptr ? 1.f : 0.f
    );
    const int opacitySource = Internal::Resolve_OpacitySourceIndex(material.opacitySource);

    CHECK_FAILED(_shader->Bind_Matrix("g_ViewMatrix", &viewMatrix), E_FAIL);
    CHECK_FAILED(_shader->Bind_Matrix("g_ProjMatrix", &projMatrix), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_vBillboardTarget", &billboardTarget, sizeof(billboardTarget)), E_FAIL);
    CHECK_FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);

    if (_noiseTexture != nullptr)
        CHECK_FAILED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);

    CHECK_FAILED(_shader->Bind_RawValue("g_vTint", &material.tint, sizeof(material.tint)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_vEffectSpriteParams", &spriteParams, sizeof(spriteParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_iOpacitySource", &opacitySource, sizeof(opacitySource)), E_FAIL);
    CHECK_FAILED(_shader->Begin(0), E_FAIL);
    CHECK_FAILED(_buffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED(_buffer->Render(), E_FAIL);

    return S_OK;
}

NS_END
