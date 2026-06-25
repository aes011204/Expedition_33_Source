#include "LightingPipeline.h"
#include "ComputeShaderCom.h"

NS_BEGIN(Engine)

LightingPipeline::LightingPipeline(ComPtr<Device> device, ComPtr<Context> context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT LightingPipeline::Initialize()
{
    _ssaoCB = SSAOCB{};
    _deferredLightingCB = DeferredLightingCB{};

    if (FAILED(Ready_FroxelTextures()))
        return E_FAIL;

    if (FAILED(Ready_FroxelBuffer()))
        return E_FAIL;

    if (FAILED(Ready_FroxelComputeShader()))
        return E_FAIL;

    return S_OK;
}

void LightingPipeline::Set_Params(const SSAOCB& params)
{
    _ssaoCB = params;
}

const SSAOCB& LightingPipeline::Get_Params() const
{
    return _ssaoCB;
}

SSAOCB& LightingPipeline::Get_Params()
{
    return _ssaoCB;
}

void LightingPipeline::Set_Enabled(bool enabled)
{
    _ssaoCB.SSAOEnabled = enabled ? 1u : 0u;
}

bool LightingPipeline::Is_Enabled() const
{
    return _ssaoCB.SSAOEnabled != 0u;
}

void LightingPipeline::Set_DeferredLightingParams(const DeferredLightingCB& params)
{
    _deferredLightingCB = params;
}

const DeferredLightingCB& LightingPipeline::Get_DeferredLightingParams() const
{
    return _deferredLightingCB;
}

DeferredLightingCB& LightingPipeline::Get_DeferredLightingParams()
{
    return _deferredLightingCB;
}

void LightingPipeline::Set_IBLEnabled(bool enabled)
{
    _deferredLightingCB.iblEnabled = enabled ? 1u : 0u;
}

bool LightingPipeline::Is_IBLEnabled() const
{
    return _deferredLightingCB.iblEnabled != 0u;
}

HRESULT LightingPipeline::Update_FroxelZone(const VolumetricLightShaftZoneCB& vlsz)
{
    CHECK_NULL(_froxelZoneBuffer, E_FAIL);

    D3D11_MAPPED_SUBRESOURCE mapped{};

    if (FAILED(_context->Map(_froxelZoneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return E_FAIL;

    *reinterpret_cast<VolumetricLightShaftZoneCB*>(mapped.pData) = vlsz;

    _context->Unmap(_froxelZoneBuffer.Get(), 0);

    return S_OK;
}

HRESULT LightingPipeline::Update_Froxel(const FroxelCB& forxel)
{
    CHECK_NULL(_froxelBuffer, E_FAIL);
    _froxelCB = forxel;

    D3D11_MAPPED_SUBRESOURCE mapped{};

    if (FAILED(_context->Map(_froxelBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return E_FAIL;

    auto* bufferData = reinterpret_cast<FroxelCB*>(mapped.pData);
    *bufferData = forxel;

    _context->Unmap(_froxelBuffer.Get(), 0);

    return S_OK;
}

HRESULT LightingPipeline::Update_FroxelShadow(const ShadowCB& shadowCB)
{
    CHECK_NULL(_froxelShadowBuffer, E_FAIL);

    D3D11_MAPPED_SUBRESOURCE mapped{};

    if (FAILED(_context->Map(_froxelShadowBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return E_FAIL;

    *reinterpret_cast<ShadowCB*>(mapped.pData) = shadowCB;

    _context->Unmap(_froxelShadowBuffer.Get(), 0);

    return S_OK;
}

void LightingPipeline::Bind_FroxelInputSRVs(ID3D11ShaderResourceView* depthSRV, ID3D11ShaderResourceView* shadowSRV)
{
    _froxelDepthSRV = depthSRV;
    _froxelShadowSRV = shadowSRV;
}

HRESULT LightingPipeline::Dispatch_FroxelInject()
{
    CHECK_NULL(_froxelInjectComputeShader, E_FAIL);
    CHECK_NULL(_froxelBuffer, E_FAIL);
    CHECK_NULL(_froxelZoneBuffer, E_FAIL);
    CHECK_NULL(_froxelShadowBuffer, E_FAIL);
    CHECK_NULL(_froxelInjectionUAV, E_FAIL);
    CHECK_NULL(_froxelDepthSRV, E_FAIL);
    CHECK_NULL(_froxelShadowSRV, E_FAIL);

    CHECK_FAILED(_froxelInjectComputeShader->Bind_ConstantBuffer(0, _froxelBuffer.Get()), E_FAIL);
    CHECK_FAILED(_froxelInjectComputeShader->Bind_ConstantBuffer(1, _froxelZoneBuffer.Get()), E_FAIL);
    CHECK_FAILED(_froxelInjectComputeShader->Bind_ConstantBuffer(2, _froxelShadowBuffer.Get()), E_FAIL);

    CHECK_FAILED(_froxelInjectComputeShader->Bind_SRV(0, _froxelDepthSRV), E_FAIL);
    CHECK_FAILED(_froxelInjectComputeShader->Bind_SRV(1, _froxelShadowSRV), E_FAIL);
    CHECK_FAILED(_froxelInjectComputeShader->Bind_UAV(0, _froxelInjectionUAV.Get()), E_FAIL);

    const uint32 groupX = (_froxelWidth + 7u) / 8u;
    const uint32 groupY = (_froxelHeight + 7u) / 8u;
    const uint32 groupZ = _froxelDepth;

    HRESULT hr = _froxelInjectComputeShader->Dispatch(groupX, groupY, groupZ);

    // Compute Shader에 물린 SRV/UAV를 바로 해제한다.
    // 다음 패스에서 같은 Texture3D를 SRV/UAV로 바꿔 물 때 hazard 방지.
    ID3D11ShaderResourceView* nullSRVs[3] = {};
    _context->CSSetShaderResources(0, 3, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[1] = {};
    UINT nullCounts[1] = { 0 };
    _context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullCounts);

    CHECK_FAILED(hr, E_FAIL);

    return S_OK;
}

HRESULT LightingPipeline::Dispatch_FroxelAccumulate()
{
    CHECK_NULL(_froxelAccumulateComputeShader, E_FAIL);
    CHECK_NULL(_froxelBlurSRV, E_FAIL);
    CHECK_NULL(_froxelIntegratedUAV, E_FAIL);
    CHECK_NULL(_froxelBuffer, E_FAIL);

    CHECK_FAILED(_froxelAccumulateComputeShader->Bind_ConstantBuffer(0, _froxelBuffer.Get()), E_FAIL);
    CHECK_FAILED(_froxelAccumulateComputeShader->Bind_SRV(2, _froxelBlurSRV.Get()), E_FAIL);
    CHECK_FAILED(_froxelAccumulateComputeShader->Bind_UAV(0, _froxelIntegratedUAV.Get()), E_FAIL);

    const uint32 groupX = (_froxelWidth + 7u) / 8u;
    const uint32 groupY = (_froxelHeight + 7u) / 8u;
    const uint32 groupZ = 1u;

    HRESULT hr = _froxelAccumulateComputeShader->Dispatch(groupX, groupY, groupZ);

    // Accumulate에서 읽은 g_FroxelInjectionTexture(t2),
    // 쓴 g_FroxelIntegratedTexture(u0)를 바로 해제한다.
    // 안 그러면 다음 프레임 CopyResource / Inject UAV 바인딩과 충돌할 수 있다.
    ID3D11ShaderResourceView* nullSRVs[3] = {};
    _context->CSSetShaderResources(0, 3, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[1] = {};
    UINT nullCounts[1] = { 0 };
    _context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullCounts);

    CHECK_FAILED(hr, E_FAIL);

    return S_OK;
}

HRESULT LightingPipeline::Dispatch_FroxelBlur()
{
    CHECK_NULL(_froxelBlurComputeShader, E_FAIL);
    CHECK_NULL(_froxelBuffer, E_FAIL);
    CHECK_NULL(_froxelInjectionSRV, E_FAIL);
    CHECK_NULL(_froxelBlurUAV, E_FAIL);

    CHECK_FAILED(_froxelBlurComputeShader->Bind_ConstantBuffer(0, _froxelBuffer.Get()), E_FAIL);
    CHECK_FAILED(_froxelBlurComputeShader->Bind_SRV(2, _froxelInjectionSRV.Get()), E_FAIL);
    CHECK_FAILED(_froxelBlurComputeShader->Bind_UAV(0, _froxelBlurUAV.Get()), E_FAIL);

    const uint32 groupX = (_froxelWidth + 7u) / 8u;
    const uint32 groupY = (_froxelHeight + 7u) / 8u;
    const uint32 groupZ = _froxelDepth;

    HRESULT hr = _froxelBlurComputeShader->Dispatch(groupX, groupY, groupZ);

    ID3D11ShaderResourceView* nullSRVs[3] = {};
    _context->CSSetShaderResources(0, 3, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[1] = {};
    UINT nullCounts[1] = { 0 };
    _context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullCounts);

    CHECK_FAILED(hr, E_FAIL);

    return S_OK;
}

HRESULT LightingPipeline::Dispatch_FroxelTemporal()
{
    CHECK_NULL(_froxelTemporalComputeShader, E_FAIL);
    CHECK_NULL(_froxelBuffer, E_FAIL);
    CHECK_NULL(_froxelIntegratedSRV, E_FAIL);
    CHECK_NULL(_froxelHistorySRV, E_FAIL);
    CHECK_NULL(_froxelTemporalUAV, E_FAIL);
    CHECK_NULL(_froxelHistoryTexture, E_FAIL);
    CHECK_NULL(_froxelTemporalTexture, E_FAIL);

    CHECK_FAILED(_froxelTemporalComputeShader->Bind_ConstantBuffer(0, _froxelBuffer.Get()), E_FAIL);
    CHECK_FAILED(_froxelTemporalComputeShader->Bind_SRV(2, _froxelIntegratedSRV.Get()), E_FAIL);
    CHECK_FAILED(_froxelTemporalComputeShader->Bind_SRV(3, _froxelHistorySRV.Get()), E_FAIL);
    CHECK_FAILED(_froxelTemporalComputeShader->Bind_UAV(0, _froxelTemporalUAV.Get()), E_FAIL);

    const uint32 groupX = (_froxelWidth + 7u) / 8u;
    const uint32 groupY = (_froxelHeight + 7u) / 8u;
    const uint32 groupZ = _froxelDepth;

    HRESULT hr = _froxelTemporalComputeShader->Dispatch(groupX, groupY, groupZ);

    ID3D11ShaderResourceView* nullSRVs[4] = {};
    _context->CSSetShaderResources(0, 4, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[1] = {};
    UINT nullCounts[1] = { 0 };
    _context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullCounts);

    CHECK_FAILED(hr, E_FAIL);

    _context->CopyResource(_froxelHistoryTexture.Get(), _froxelTemporalTexture.Get());
    _hasFroxelFrameData = true;

    return S_OK;
}

HRESULT LightingPipeline::Reload_FroxelComputeShader()
{
    if (FAILED(Ready_FroxelComputeShader()))
        return E_FAIL;

    return S_OK;
}

ID3D11ShaderResourceView* LightingPipeline::Get_DebugFroxelSRV() const
{
    return (_froxelDebugMode == FroxelDebugMode::RawInjection)
        ? _froxelInjectionSRV.Get()
        : (_froxelTemporalSRV ? _froxelTemporalSRV.Get() : _froxelIntegratedSRV.Get());
}

ID3D11ShaderResourceView* LightingPipeline::Get_FroxelIntegratedSRV() const
{
    return _froxelTemporalSRV ? _froxelTemporalSRV.Get() : _froxelIntegratedSRV.Get();
}

const FroxelCB& LightingPipeline::Get_FroxelParams() const
{
    return _froxelCB;
}

void LightingPipeline::Invalidate_FroxelFrameData()
{
    _hasFroxelFrameData = false;
}

bool LightingPipeline::Has_FroxelFrameData() const
{
    return _hasFroxelFrameData;
}

uint32 LightingPipeline::Get_DebugFroxelWidth() const
{
    return _froxelWidth;
}

uint32 LightingPipeline::Get_DebugFroxelHeight() const
{
    return _froxelHeight;
}

uint32 LightingPipeline::Get_DebugFroxelDepth() const
{
    return _froxelDepth;
}

HRESULT LightingPipeline::Ready_FroxelTextures()
{
    auto createTextureSet = [this](
        ComPtr<ID3D11Texture3D>& texture,
        ComPtr<ID3D11UnorderedAccessView>& uav,
        ComPtr<ID3D11ShaderResourceView>& srv) -> HRESULT
        {
            texture.Reset();
            uav.Reset();
            srv.Reset();

            D3D11_TEXTURE3D_DESC desc{};
            desc.Width = _froxelWidth;
            desc.Height = _froxelHeight;
            desc.Depth = _froxelDepth;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

            if (FAILED(_device->CreateTexture3D(&desc, nullptr, texture.GetAddressOf())))
                return E_FAIL;

            D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
            uavDesc.Format = desc.Format;
            uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
            uavDesc.Texture3D.MipSlice = 0;
            uavDesc.Texture3D.FirstWSlice = 0;
            uavDesc.Texture3D.WSize = _froxelDepth;

            if (FAILED(_device->CreateUnorderedAccessView(texture.Get(), &uavDesc, uav.GetAddressOf())))
                return E_FAIL;

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MostDetailedMip = 0;
            srvDesc.Texture3D.MipLevels = 1;

            if (FAILED(_device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf())))
                return E_FAIL;

            return S_OK;
        };

    return createTextureSet(_froxelInjectionTexture, _froxelInjectionUAV, _froxelInjectionSRV) == S_OK &&
        createTextureSet(_froxelBlurTexture, _froxelBlurUAV, _froxelBlurSRV) == S_OK &&
        createTextureSet(_froxelIntegratedTexture, _froxelIntegratedUAV, _froxelIntegratedSRV) == S_OK &&
        createTextureSet(_froxelHistoryTexture, _froxelHistoryUAV, _froxelHistorySRV) == S_OK &&
        createTextureSet(_froxelTemporalTexture, _froxelTemporalUAV, _froxelTemporalSRV) == S_OK
        ? S_OK
        : E_FAIL;
}

HRESULT LightingPipeline::Ready_FroxelBuffer()
{
    D3D11_BUFFER_DESC froxelDesc{};
    froxelDesc.ByteWidth = sizeof(FroxelCB);
    froxelDesc.Usage = D3D11_USAGE_DYNAMIC;
    froxelDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    froxelDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(_device->CreateBuffer(&froxelDesc, nullptr, _froxelBuffer.GetAddressOf())))
        return E_FAIL;

    D3D11_BUFFER_DESC froxelZoneDesc{};
    froxelZoneDesc.ByteWidth = sizeof(VolumetricLightShaftZoneCB);
    froxelZoneDesc.Usage = D3D11_USAGE_DYNAMIC;
    froxelZoneDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    froxelZoneDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(_device->CreateBuffer(&froxelZoneDesc, nullptr, _froxelZoneBuffer.GetAddressOf())))
        return E_FAIL;

    D3D11_BUFFER_DESC froxelShadowDesc{};
    froxelShadowDesc.ByteWidth = sizeof(ShadowCB);
    froxelShadowDesc.Usage = D3D11_USAGE_DYNAMIC;
    froxelShadowDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    froxelShadowDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(_device->CreateBuffer(&froxelShadowDesc, nullptr, _froxelShadowBuffer.GetAddressOf())))
        return E_FAIL;

    return S_OK;
}

HRESULT LightingPipeline::Ready_FroxelComputeShader()
{
    Shared<ComputeShaderCom> froxelInjectComputeShader = ComputeShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_Froxel.hlsl",
        "InjectFroxelBuffer");
    if (nullptr == froxelInjectComputeShader)
        return E_FAIL;

    Shared<ComputeShaderCom> froxelBlurComputeShader = ComputeShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_Froxel.hlsl",
        "BlurFroxelBuffer");
    if (nullptr == froxelBlurComputeShader)
        return E_FAIL;

    Shared<ComputeShaderCom> froxelAccumulateComputeShader = ComputeShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_Froxel.hlsl",
        "AccumulateFroxelBuffer");
    if (nullptr == froxelAccumulateComputeShader)   
        return E_FAIL;

    Shared<ComputeShaderCom> froxelTemporalComputeShader = ComputeShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_Froxel.hlsl",
        "TemporalFroxelIntegration");
    if (nullptr == froxelTemporalComputeShader)
        return E_FAIL;

    _froxelInjectComputeShader = std::move(froxelInjectComputeShader);
    _froxelBlurComputeShader = std::move(froxelBlurComputeShader);
    _froxelAccumulateComputeShader = std::move(froxelAccumulateComputeShader);
    _froxelTemporalComputeShader = std::move(froxelTemporalComputeShader);

    CHECK_NULL(_froxelInjectComputeShader, E_FAIL);
    CHECK_NULL(_froxelBlurComputeShader, E_FAIL);
    CHECK_NULL(_froxelAccumulateComputeShader, E_FAIL);
    CHECK_NULL(_froxelTemporalComputeShader, E_FAIL);

    return S_OK;
}

void LightingPipeline::Set_DebugFroxelSlice(uint32 debugSlice)
{
    _froxelDebugSlice = min(debugSlice, _froxelDepth > 0u ? (_froxelDepth - 1u) : 0u);
}

uint32 LightingPipeline::Get_DebugFroxelSlice() const
{
    return _froxelDebugSlice;
}

void LightingPipeline::Set_DebugFroxelMode(FroxelDebugMode debugMode)
{
    _froxelDebugMode = debugMode;
}

LightingPipeline::FroxelDebugMode LightingPipeline::Get_DebugFroxelMode() const
{
    return _froxelDebugMode;
}

Shared<LightingPipeline> LightingPipeline::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    Shared<LightingPipeline> instance = make_shared<LightingPipeline>(device, context);
    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Create : LightingPipeline");
        return nullptr;
    }

    return instance;
}

void LightingPipeline::Free()
{
}

NS_END
