#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ComputeShaderCom;

class ENGINE_DLL LightingPipeline : public Base
{
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
    HRESULT Update_Froxel(const FroxelCB& forxel);
    HRESULT Dispatch_Froxel();

    ID3D11ShaderResourceView* Get_DebugFroxelSRV() const;
    uint32 Get_DebugFroxelWidth() const;
    uint32 Get_DebugFroxelHeight() const;
    uint32 Get_DebugFroxelDepth() const;
    void Set_DebugFroxelSlice(uint32 debugSlice);
    uint32 Get_DebugFroxelSlice() const;

private :
    HRESULT Ready_FroxelTexture();
    HRESULT Ready_FroxelBuffer();

private:
    ComPtr<Device>  _device;
    ComPtr<Context> _context;

    SSAOCB _ssaoCB{};
    DeferredLightingCB _deferredLightingCB{};

    const uint32    _froxelWidth = 160;
    const uint32    _froxelHeight = 90;
    const uint32    _froxelDepth = 64;

    Shared<ComputeShaderCom>            _froxelComputeShader;

    ComPtr<ID3D11Texture3D>             _froxelTexture;
    ComPtr<ID3D11UnorderedAccessView>   _froxelUAV;
    ComPtr<ID3D11ShaderResourceView>    _froxelSRV;

    ComPtr<ID3D11Buffer>                _froxelBuffer;

    uint32                              _froxelDebugSlice = 0;

public:
    static Shared<LightingPipeline> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;

};

NS_END

