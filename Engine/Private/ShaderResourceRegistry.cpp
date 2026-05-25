#include "ShaderResourceRegistry.h"

#include "ShaderCom.h"

ShaderResourceRegistry::ShaderResourceRegistry(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device(device)
    , _context(context)
{
}

HRESULT ShaderResourceRegistry::Initialize(const wstring& shaderTextureRootPath)
{
    if (shaderTextureRootPath.empty())
        return E_INVALIDARG;

    _srvs.clear();
    _nameToHandle.clear();

    std::error_code ec{};
    const fs::path rootPath = fs::path(shaderTextureRootPath).lexically_normal();
    if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec))
        return E_FAIL;

    vector<fs::path> texturePaths{};
    for (const auto& entry : fs::recursive_directory_iterator(rootPath, ec))
    {
        if (ec)
            return E_FAIL;

        if (!entry.is_regular_file(ec))
            continue;

        const fs::path& filePath = entry.path();
        if (!IsSupportedTextureExt(filePath))
            continue;

        texturePaths.emplace_back(filePath);
    }

    _srvs.resize(texturePaths.size());

    // 초기화 시 1회 루트 폴더 아래의 .png/.dds 파일을 읽어 SRV를 생성하고 저장한다.
    for (uint32 i = 0; i < static_cast<uint32>(texturePaths.size()); ++i)
    {
        const fs::path& path = texturePaths[i];
        const string name = String::ToString(path.stem().wstring());

        if (_nameToHandle.contains(name))
            return E_FAIL;

        ComPtr<ID3D11ShaderResourceView> srv{};
        const fs::path ext = path.extension();
        HRESULT hr = E_FAIL;

        if (ext == ".dds" || ext == ".DDS")
        {
            hr = DirectX::CreateDDSTextureFromFile(
                _device.Get(),
                path.c_str(),
                nullptr,
                srv.GetAddressOf());
        }
        else
        {
            hr = DirectX::CreateWICTextureFromFile(
                _device.Get(),
                path.c_str(),
                nullptr,
                srv.GetAddressOf());
        }

        if (FAILED(hr))
            return E_FAIL;

        _srvs[i] = std::move(srv);
        _nameToHandle.emplace(name, i);
    }

    return S_OK;
}

uint32 ShaderResourceRegistry::Get_Handle(const string& name) const
{
    const auto it = _nameToHandle.find(name);
    if (it == _nameToHandle.end())
        return InvalidSRVHandle;

    return it->second;
}

ID3D11ShaderResourceView* ShaderResourceRegistry::Get_SRV(uint32 handle) const
{
    if (handle == InvalidSRVHandle || handle >= _srvs.size())
        return nullptr;

    return _srvs[handle].Get();
}

ID3D11ShaderResourceView* ShaderResourceRegistry::Get_SRV(const string& name) const
{
    return Get_SRV(Get_Handle(name));
}

HRESULT ShaderResourceRegistry::Bind_ByHandle(ShaderCom* shader, const char* shaderVariableName, uint32 handle) const
{
    if (shader == nullptr || shaderVariableName == nullptr)
        return E_INVALIDARG;

    ID3D11ShaderResourceView* srv = Get_SRV(handle);
    if (srv == nullptr)
        return E_FAIL;

    return shader->Bind_SRV(shaderVariableName, srv);
}

HRESULT ShaderResourceRegistry::Bind_ByName(ShaderCom* shader, const char* shaderVariableName, const string& name) const
{
    if (name.empty())
        return E_INVALIDARG;

    return Bind_ByHandle(shader, shaderVariableName, Get_Handle(name));
}

Unique<ShaderResourceRegistry> ShaderResourceRegistry::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const wstring& shaderTextureRootPath)
{
    auto instance = make_unique<ShaderResourceRegistry>(device, context);

    if (FAILED(instance->Initialize(shaderTextureRootPath)))
        return nullptr;

    return instance;
}

void ShaderResourceRegistry::Free()
{
    __super::Free();

    _nameToHandle.clear();
    _srvs.clear();
    _context.Reset();
    _device.Reset();
}

bool ShaderResourceRegistry::IsSupportedTextureExt(const fs::path& path)
{
    const fs::path ext = path.extension();
    return ext == ".dds" || ext == ".DDS" ||
        ext == ".png" || ext == ".PNG" ||
        ext == ".jpg" || ext == ".JPG" ||
        ext == ".jpeg" || ext == ".JPEG" ||
        ext == ".tif" || ext == ".TIF" ||
        ext == ".tiff" || ext == ".TIFF" ||
        ext == ".bmp" || ext == ".BMP";
}
