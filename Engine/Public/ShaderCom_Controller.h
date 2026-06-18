#pragma once

#include "Base.h"
#include "ShaderCom.h"
#include <mutex>

NS_BEGIN(Engine)

class ENGINE_DLL ShaderCom_Controller final : public Base
{
public:
    ShaderCom_Controller(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~ShaderCom_Controller() override = default;

public:
    HRESULT Initialize();

    HRESULT Register_Shader(const Shared<ShaderCom>& shader);
    void    Unregister_Shader(const ShaderCom* shader);

    HRESULT Reload_VertexShaders();

public:
    static Unique<ShaderCom_Controller> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;

private:
    struct ShaderRecord
    {
        wstring                 shaderPath{};
        ShaderType              shaderType = ShaderType::VtxTex;
        vector<D3D11_INPUT_ELEMENT_DESC> inputElements{};
        uint32                  numElements = 0;
        vector<Weak<ShaderCom>>  shaders{};
    };

private:
    const ShaderRecord* Find_Record_Locked(const wstring& shaderPath) const;
    ShaderRecord*       Find_Record_Locked(const wstring& shaderPath);
    static bool         Is_ReloadTarget(ShaderType shaderType);
    HRESULT             Reload_Record(ShaderRecord& record);
    void                Prune_ExpiredShaders_Locked(ShaderRecord& record);

private:
    ComPtr<Device>       _device{};
    ComPtr<Context>      _context{};
    vector<ShaderRecord> _records{};
    mutable std::mutex   _mutex{};
};

NS_END
