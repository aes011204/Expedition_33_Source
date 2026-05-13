#pragma once

#include "EffectMaterialAuthoring_Types.h"

NS_BEGIN(Engine)
class RenderTarget;
class ShaderCom;
class Texture;
class VIBufferCom_PointParticle;
NS_END

NS_BEGIN(EffectEditor)

class EffectMaterialPreviewRenderer final
{
public:
    explicit EffectMaterialPreviewRenderer(uint32 previewTextureSize = 512);
    ~EffectMaterialPreviewRenderer() = default;

public:
    void Render_Material(const EffectMaterialInstanceData& material);
    ID3D11ShaderResourceView* Get_SRV() const;
    const string& Get_StatusMessage() const;
    void Handle_ViewportInput(bool isHovered);

private: //## Data
    uint32 _previewTextureSize{ 512 };
    Engine::Shared<Engine::RenderTarget> _renderTarget{};
    Engine::Shared<Engine::ShaderCom> _shader{};
    Engine::Shared<Engine::Texture> _texture{};
    Engine::Shared<Engine::Texture> _noiseTexture{};
    Engine::Shared<Engine::VIBufferCom_PointParticle> _buffer{};
    string _loadedTextureId{};
    string _loadedNoiseTextureId{};
    string _statusMessage{};
    float _yawRadians{ 0.f };
    float _pitchRadians{ 0.15f };
    float _distance{ 3.5f };

private: //## Helpers
    bool Ready_RenderTarget();
    bool Ready_Shader();
    bool Ready_Texture(const string& textureId);
    bool Ready_NoiseTexture(const string& noiseTextureId);
    bool Ready_Buffer();
    HRESULT Render_Sprite(const EffectMaterialInstanceData& material) const;
};

NS_END
