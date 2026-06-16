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

    if (FAILED(Ready_FroxelTexture()))
        return E_FAIL;

    if (FAILED(Ready_FroxelBuffer()))
        return E_FAIL;

    _froxelComputeShader = ComputeShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_Froxel.hlsl",
        "InJectFroxelBuffer");
    if (nullptr == _froxelComputeShader)
        return E_FAIL;

    CHECK_NULL(_froxelComputeShader, E_FAIL);

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

HRESULT LightingPipeline::Update_Froxel(const FroxelCB& forxel)
{
    D3D11_MAPPED_SUBRESOURCE mapped{};

    if (FAILED(_context->Map(_froxelBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return E_FAIL;

    auto* bufferData = reinterpret_cast<FroxelCB*>(mapped.pData);
    *bufferData = forxel;

    _context->Unmap(_froxelBuffer.Get(), 0);

    return S_OK;
}

HRESULT LightingPipeline::Dispatch_Froxel()
{
    CHECK_NULL(_froxelComputeShader, E_FAIL);
    CHECK_NULL(_froxelBuffer, E_FAIL);
    CHECK_NULL(_froxelUAV, E_FAIL);

    CHECK_FAILED(_froxelComputeShader->Bind_ConstantBuffer(0, _froxelBuffer.Get()), E_FAIL);
    CHECK_FAILED(_froxelComputeShader->Bind_UAV(0, _froxelUAV.Get()), E_FAIL);

    const uint32 groupX = (_froxelWidth + 7u) / 8u;
    const uint32 groupY = (_froxelHeight + 7u) / 8u;
    const uint32 groupZ = _froxelDepth;

    // 컴퓨트 셰이더는 draw 콜이 아니라 디스패치로 실행한다.
    CHECK_FAILED(_froxelComputeShader->Dispatch(groupX, groupY, groupZ), E_FAIL);

    return S_OK;
}

ID3D11ShaderResourceView* LightingPipeline::Get_DebugFroxelSRV() const
{
    return _froxelSRV.Get();
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

HRESULT LightingPipeline::Ready_FroxelTexture()
{
    _froxelSRV.Reset();
    _froxelUAV.Reset();
    _froxelTexture.Reset();

    D3D11_TEXTURE3D_DESC desc{};
    desc.Width = _froxelWidth;
    desc.Height = _froxelHeight;
    desc.Depth = _froxelDepth;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    if (FAILED(_device->CreateTexture3D(&desc, nullptr, _froxelTexture.GetAddressOf())))
        return E_FAIL;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = desc.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
    uavDesc.Texture3D.MipSlice = 0;
    uavDesc.Texture3D.FirstWSlice = 0;
    uavDesc.Texture3D.WSize = _froxelDepth;

    if (FAILED(_device->CreateUnorderedAccessView(_froxelTexture.Get(), &uavDesc, _froxelUAV.GetAddressOf())))
        return E_FAIL;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = 1;

    if (FAILED(_device->CreateShaderResourceView(_froxelTexture.Get(), &srvDesc, _froxelSRV.GetAddressOf())))
        return E_FAIL;

    return S_OK;
}

HRESULT LightingPipeline::Ready_FroxelBuffer()
{
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(FroxelCB);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(_device->CreateBuffer(&desc, nullptr, _froxelBuffer.GetAddressOf())))
        return E_FAIL;

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
