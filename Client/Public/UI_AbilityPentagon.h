#pragma once

#include "UI_Base.h"
#include "Vertex_Types.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(Client)

class UI_AbilityPentagon final : public Engine::UI_Base
{
public:
    struct UI_ABILITY_PENTAGON_DESC : public Engine::UI_Base::UI_DESC
    {
        wstring texturePath = L"../Bin/Resources/Textures/UI/Common/Icons/T_White_Pentagon.png";
        Vec4    color{ 196.f / 255.f, 165.f / 255.f, 1.f, 1.f };
        uint32  shaderPrototypeLevelIndex = 0;
        wstring shaderPrototypeTag = L"Shader_UI";
        float   maxStatValue = 99.f;
        float   baseInnerRatio = 0.18f;
    };

public:
    UI_AbilityPentagon() = default;
    UI_AbilityPentagon(const UI_AbilityPentagon& prototype);
    ~UI_AbilityPentagon() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    HRESULT Render() override;

    void Set_Color(const Vec4& color) override { _color = color; }
    Vec4 Get_Color() const override { return _color; }

    void Set_StatValues(const array<int32, 5>& statValues);
    void Set_NormalizedRatios(const array<float, 5>& ratios);

private:
    HRESULT Ready_RenderResources();
    HRESULT Load_TextureResource();
    HRESULT Create_GeometryBuffers();
    void    Rebuild_Geometry();

private:
    wstring _texturePath{};
    Vec4 _color{ 1.f, 1.f, 1.f, 1.f };
    uint32 _shaderPrototypeLevelIndex = 0;
    wstring _shaderPrototypeTag = L"Shader_UI";
    float _maxStatValue = 99.f;
    float _baseInnerRatio = 0.18f;

    array<float, 5> _normalizedRatios{};
    array<Engine::VTXTEX, 6> _vertices{};
    array<uint16, 15> _indices{ 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 1 };

    Shared<Engine::ShaderCom> _shader{};
    ComPtr<ShaderResourceView> _srv{};
    ComPtr<ID3D11Buffer> _vertexBuffer{};
    ComPtr<ID3D11Buffer> _indexBuffer{};

public:
    static Shared<UI_AbilityPentagon> Create();
    Shared<Engine::UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END
