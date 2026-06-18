#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ComputeShaderCom;

class ENGINE_DLL LightingPipeline : public Base
{
public:
    enum class FroxelDebugMode : uint8
    {
        Integrated = 0,
        RawInjection,
    };

public:
    explicit LightingPipeline(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~LightingPipeline() = default;

public:
    HRESULT Initialize();

    void Set_Params(const SSAOCB& params);
    const SSAOCB& Get_Params() const;
    SSAOCB& Get_Params();

    void Set_Enabled(bool enabled);
    bool Is_Enabled() const;
    
    void Set_DeferredLightingParams(const DeferredLightingCB& params);
    const DeferredLightingCB& Get_DeferredLightingParams() const;
    DeferredLightingCB& Get_DeferredLightingParams();

    // IBL
    void Set_IBLEnabled(bool enabled);
    bool Is_IBLEnabled() const;

    // Froxel
    HRESULT Update_FroxelZone(const VolumetricLightShaftZoneCB& vlsz);
    HRESULT Update_Froxel(const FroxelCB& forxel);
    HRESULT Update_FroxelShadow(const ShadowCB& shadowCB);
    void Bind_FroxelInputSRVs(ID3D11ShaderResourceView* depthSRV, ID3D11ShaderResourceView* shadowSRV);
    HRESULT Dispatch_FroxelInject();
    HRESULT Dispatch_FroxelAccumulate();
    HRESULT Reload_FroxelComputeShader();
    void Invalidate_FroxelFrameData();
    bool Has_FroxelFrameData() const;

    ID3D11ShaderResourceView* Get_DebugFroxelSRV() const;
    ID3D11ShaderResourceView* Get_FroxelIntegratedSRV() const;
    const FroxelCB& Get_FroxelParams() const;
    uint32 Get_DebugFroxelWidth() const;
    uint32 Get_DebugFroxelHeight() const;
    uint32 Get_DebugFroxelDepth() const;
    void Set_DebugFroxelSlice(uint32 debugSlice);
    uint32 Get_DebugFroxelSlice() const;
    void Set_DebugFroxelMode(FroxelDebugMode debugMode);
    FroxelDebugMode Get_DebugFroxelMode() const;

private :
    HRESULT Ready_FroxelTextures();
    HRESULT Ready_FroxelBuffer();
    HRESULT Ready_FroxelComputeShader();

private:
    ComPtr<Device>  _device;
    ComPtr<Context> _context;

    SSAOCB _ssaoCB{};
    DeferredLightingCB _deferredLightingCB{};

    const uint32    _froxelWidth = 160;
    const uint32    _froxelHeight = 90;
    const uint32    _froxelDepth = 64;

    Shared<ComputeShaderCom>            _froxelInjectComputeShader;
    Shared<ComputeShaderCom>            _froxelAccumulateComputeShader;

    ComPtr<ID3D11Texture3D>             _froxelInjectionTexture;
    ComPtr<ID3D11UnorderedAccessView>   _froxelInjectionUAV;
    ComPtr<ID3D11ShaderResourceView>    _froxelInjectionSRV;
    ComPtr<ID3D11Texture3D>             _froxelIntegratedTexture;
    ComPtr<ID3D11UnorderedAccessView>   _froxelIntegratedUAV;
    ComPtr<ID3D11ShaderResourceView>    _froxelIntegratedSRV;

    ComPtr<ID3D11Buffer>                _froxelZoneBuffer;
    ComPtr<ID3D11Buffer>                _froxelBuffer;
    ComPtr<ID3D11Buffer>                _froxelShadowBuffer;
    FroxelCB                            _froxelCB{};

    ID3D11ShaderResourceView*           _froxelDepthSRV = nullptr;
    ID3D11ShaderResourceView*           _froxelShadowSRV = nullptr;
    bool                                _hasFroxelFrameData = false;

    uint32                              _froxelDebugSlice = 0;
    FroxelDebugMode                     _froxelDebugMode = FroxelDebugMode::Integrated;

public:
    static Shared<LightingPipeline> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;

};

NS_END

