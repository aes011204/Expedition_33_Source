#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;

class ENGINE_DLL ShaderResourceRegistry final : public Base
{
public:
    ShaderResourceRegistry(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~ShaderResourceRegistry() override = default;

public:
    HRESULT Initialize(const wstring& shaderTextureRootPath);

    uint32 Get_Handle(const string& name) const;
    // 핸들로 접근
    ID3D11ShaderResourceView* Get_SRV(uint32 handle) const;
    // 이름으로 접근
    ID3D11ShaderResourceView* Get_SRV(const string& name) const;

    // 바로 바인딩까지
    HRESULT Bind_ByHandle(ShaderCom* shader, const char* shaderVariableName, uint32 handle) const;
    HRESULT Bind_ByName(ShaderCom* shader, const char* shaderVariableName, const string& name) const;

    uint32 Get_Count() const { return static_cast<uint32>(_srvs.size()); }

public:
    static Unique<ShaderResourceRegistry> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& shaderTextureRootPath);
    void Free() override;

private:
    static bool IsSupportedTextureExt(const fs::path& path);

private:
    ComPtr<Device> _device{};
    ComPtr<Context> _context{};

    vector<ComPtr<ID3D11ShaderResourceView>> _srvs{};       // 핸들(인덱스) -> SRV 
    umap<string, uint32> _nameToHandle{};                   // 파일명 -> 핸들
};

NS_END
