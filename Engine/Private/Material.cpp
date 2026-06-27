#include "Material.h"
#include "ShaderCom.h"
#include "ShaderPassFinder.h"

namespace
{
    mutex g_TextureSrvCacheMutex;
    unordered_map<wstring, ComPtr<ID3D11ShaderResourceView>> g_TextureSrvCache;
    uint64 g_TextureSrvCacheCreateCount = 0;
}

Material::Material(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }, _context{ context } {}

HRESULT Material::Initialize(const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize)
{
    _materialName = materialData.name;

    _maxTextureTypeSize = static_cast<uint32>(MaterialTextureSlot::END);
    _materials.resize(_maxTextureTypeSize);

    fs::path rootPath = fs::path(modelPath).parent_path();

    for (const auto& slot : slots)
    {
        TextureSlot texSlot{};
        texSlot.slot = ResolveMaterialTextureSlot(static_cast<MaterialRawTextureType>(slot.typeIndex));
        if (texSlot.slot == MaterialTextureSlot::END)
            continue;

        texSlot.textureIndex = slot.textureIndex;

        texSlot.uvScale = slot.uvScale;
        texSlot.uvOffset = slot.uvOffset;
        texSlot.uvRotation = slot.uvRotation;
        texSlot.blendFactor = slot.blendFactor;
        texSlot.uvChannel = slot.uvChannel;
        texSlot.wrapMode = slot.wrapMode;

        string texFileName = slot.path;

        if (!texFileName.empty())
        {
            wstring wFoundPath = Find_TextureFile(rootPath, texFileName);

            if (!wFoundPath.empty())
            {
                // 원본 경로가 png/jpg여도 같은 이름의 dds가 있으면, 실제 로드는 dds로 진행
                // resolvePath도 실제 로드한 경로로 저장해야 이후 동일 텍스처 비교/캐시 hit 기준이 맞는다.
                const fs::path loadPath = Resolve_TextureLoadPath(fs::path(wFoundPath));
                texSlot.resolvedPath = loadPath.lexically_normal().wstring();

                ID3D11ShaderResourceView* srv = nullptr;
                HRESULT hr = Create_TextureSRV(loadPath, &srv);

                if (SUCCEEDED(hr))
                    texSlot.shaderResourceView.Attach(srv);
            }
        }

        _materials[static_cast<uint32>(texSlot.slot)].push_back(texSlot);
    }

    // 모델(.model)에 포함된 baseColor 슬롯의 UV를 기본값으로 한 번만 복사해둔다.
    // 이후에는 Editor/Bundle에서 Set_CB()로 덮어쓴 값이 유지되어야 한다.
    if (const TextureSlot* baseColorSlot = Find_TextureSlot(MaterialTextureSlot::BaseColor, 0))
    {
        _cb.uvScale = baseColorSlot->uvScale;
        _cb.uvOffset = baseColorSlot->uvOffset;
    }

    Update_MaterialState();

    _passDirty = true;

    return S_OK;
}

HRESULT Material::Bind_Material(ShaderCom* shader, const char* constantName, MaterialTextureSlot slot, uint32 textureIndex)
{
    const uint32 slotIndex = static_cast<uint32>(slot);
    if (slotIndex >= _maxTextureTypeSize || textureIndex >= _materials[slotIndex].size())
        return E_FAIL;

    if (!_materials[slotIndex][textureIndex].shaderResourceView)
        return E_FAIL;

    // Bind_Material이 Begin보다 반드시 먼저 실행된다.
    _shaderType = shader->Get_Type();

    return shader->Bind_SRV(constantName, _materials[slotIndex][textureIndex].shaderResourceView.Get());
}

HRESULT Material::Override_TextureSlot(MaterialTextureSlot slot, uint32 slotIndex, const wstring& texturePath)
{
    const uint32 materialSlotIndex = static_cast<uint32>(slot);
    if (materialSlotIndex >= _maxTextureTypeSize)
        return E_FAIL;

    if (slotIndex >= _materials[materialSlotIndex].size())
        _materials[materialSlotIndex].resize(slotIndex + 1);

    // 빈 경로는 해당 슬롯을 비우는 용도로 사용한다.
    // 모델에 원래 없던 슬롯을 추가하거나, 원래 있던 슬롯을 제거하고 싶을 때 사용한다.
    if (texturePath.empty())
    {
        TextureSlot& textureSlot = _materials[materialSlotIndex][slotIndex];
        // 이미 비어있다면 그대로 둔다. (Apply 버튼 반복 클릭 시 불필요한 업데이트/히치 방지)
        if (textureSlot.resolvedPath.empty() && textureSlot.shaderResourceView == nullptr)
            return S_OK;

        textureSlot.slot = slot;
        textureSlot.textureIndex = slotIndex;
        textureSlot.isOverride = true;
        textureSlot.resolvedPath.clear();
        textureSlot.shaderResourceView.Reset();

        Update_MaterialState();
        return S_OK;
    }

    const fs::path requestedPath(texturePath);
    if (!fs::exists(requestedPath) || !fs::is_regular_file(requestedPath))
        return E_FAIL;

    // 원본 override 경로가 png/jpg여도 같은 이름의 dds가 있으면 실제 로드는 dds로 한다.
    // 아래 비교/저장도 requestedPath가 아니라 loadPath 기준이어야 같은 텍스처 재적용 시 cache hit가 난다.
    const fs::path loadPath = Resolve_TextureLoadPath(requestedPath);
    const wstring normalized = loadPath.lexically_normal().wstring();

    // 동일 경로가 이미 적용되어 있고 SRV도 살아있으면 재로딩하지 않는다.
    // (Editor에서 Apply를 여러 번 눌렀을 때 매번 WIC/DDS 로드를 다시 하는 비용이 매우 크다)
    {
        TextureSlot& existingSlot = _materials[materialSlotIndex][slotIndex];
        if (!existingSlot.resolvedPath.empty() &&
            existingSlot.resolvedPath == normalized &&
            existingSlot.shaderResourceView != nullptr)
        {
            return S_OK;
        }
    }

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(Create_TextureSRV(loadPath, &srv)))
        return E_FAIL;

    TextureSlot& textureSlot = _materials[materialSlotIndex][slotIndex];
    textureSlot.slot = slot;
    textureSlot.textureIndex = slotIndex;
    textureSlot.isOverride = true;

    // Inspector/debug에서도 “요청 경로”가 아니라 실제 SRV를 만든 경로를 보이게 저장한다.
    textureSlot.resolvedPath = normalized;
    textureSlot.shaderResourceView.Reset();
    textureSlot.shaderResourceView.Attach(srv);


    Update_MaterialState();

    return S_OK;
}

// 텍스처 존재 여부를 materialMask에 반영한다.
void Material::Update_MaterialState()
{
    _cb.materialMask = 0;

    for (uint32 i = 0; i < ETOI(MaterialTextureSlot::END); ++i)
    {
        const MaterialTextureSlot slot = static_cast<MaterialTextureSlot>(i);
        const TextureSlot* textureSlot = Find_TextureSlot(slot, 0);

        if (textureSlot != nullptr && textureSlot->shaderResourceView != nullptr)
            _cb.materialMask |= (1u << i);
    }
}

uint32 Material::Get_TextureCount(MaterialTextureSlot slot) const
{
    const uint32 slotIndex = static_cast<uint32>(slot);
    if (slotIndex >= _materials.size())
        return 0;

    return static_cast<uint32>(_materials[slotIndex].size());
}

const Material::TextureSlot* Material::Find_TextureSlot(MaterialTextureSlot slot, uint32 textureIndex) const
{
    const uint32 slotIndex = static_cast<uint32>(slot);
    if (slotIndex >= _materials.size())
        return nullptr;

    const auto& slots = _materials[slotIndex];
    if (textureIndex >= slots.size())
        return nullptr;
    return &slots[textureIndex];
}

void Material::Set_RenderState(const MaterialRenderState& renderState)
{
    _renderState = renderState;
    _passDirty = true;
}

void Material::Set_CB(const MaterialCB& cb)
{
    _cb = cb;
    Update_MaterialState();
}

void Material::Set_ShaderType(ShaderType shaderType)
{
    _shaderType = shaderType;
    _passDirty = true;
}

void Material::Set_ForcedPassIndex(int32 forcedPassIndex)
{
    if (_forcedPassIndex == forcedPassIndex)
        return;

    _forcedPassIndex = forcedPassIndex;

    // 강제 패스를 해제하면 다음 Get_PassIndex()에서 정상 계산되도록 한다.
    if (_forcedPassIndex < 0)
        _passDirty = true;
}

// 메쉬의 머테리얼 별로 pass를 세분화할 때 사용한다.
uint32 Material::Get_PassIndex()
{
    if (_forcedPassIndex >= 0)
        return static_cast<uint32>(_forcedPassIndex);

    if (_passDirty)
        Refresh_PassIndex();

    return _passIndex;
}

wstring Material::Find_TextureFile(const fs::path& searchRoot, const string& fileName)
{
    /* 1차: 같은 디렉토리에서 직접 확인 */
    fs::path directPath = searchRoot / fileName;
    if (fs::exists(directPath))
        return directPath.lexically_normal().wstring();

    /* 2차: 형제 + 하위 폴더를 재귀 탐색하여 파일명 일치 검색 */
    for (const auto& entry : fs::recursive_directory_iterator(searchRoot))
    {
        if (entry.is_regular_file() && entry.path().filename().string() == fileName)
            return entry.path().lexically_normal().wstring();
    }

    return L"";
}

fs::path Material::Resolve_TextureLoadPath(const fs::path& texturePath) const
{
    fs::path normalizedPath = texturePath.lexically_normal();

    const string genericPath = normalizedPath.generic_string();
    const string fileName = normalizedPath.filename().string();
    const string lowerPath = String::ToLowerCopy(genericPath);
    const string lowerFileName = String::ToLowerCopy(fileName);
    const fs::path ext = normalizedPath.extension();

    //const bool isHairTexture =
    //    lowerPath.find("hair") != string::npos ||
    //    lowerFileName.find("hair") != string::npos;

    //const bool isEyelashOrEyebrowTexture =
    //    lowerFileName.find("eyelash") != string::npos ||
    //    lowerFileName.find("eyebrow") != string::npos;

    //if (isHairTexture || isEyelashOrEyebrowTexture)
    //{
    //    if (ext == ".dds" || ext == ".DDS")
    //    {
    //        fs::path pngPath = normalizedPath;
    //        pngPath.replace_extension(".png");

    //        if (fs::exists(pngPath) && fs::is_regular_file(pngPath))
    //        {
    //            return pngPath.lexically_normal();
    //        }

    //        fs::path upperPngPath = normalizedPath;
    //        upperPngPath.replace_extension(".PNG");

    //        if (fs::exists(upperPngPath) && fs::is_regular_file(upperPngPath))
    //        {
    //            return upperPngPath.lexically_normal();
    //        }
    //    }

    //    // 헤어 카드 계열은 DDS 밉/알파 coverage 변화로 멀리서 듬성듬성해질 수 있어 원본을 유지한다.
    //    return normalizedPath;
    //}

    if (ext == ".dds" || ext == ".DDS")
        return normalizedPath;

    fs::path ddsPath = normalizedPath;
    ddsPath.replace_extension(".dds");

    if (fs::exists(ddsPath) && fs::is_regular_file(ddsPath))
    {
        return ddsPath.lexically_normal();
    }

    fs::path upperDdsPath = normalizedPath;
    upperDdsPath.replace_extension(".DDS");

    if (fs::exists(upperDdsPath) && fs::is_regular_file(upperDdsPath))
    {
        return upperDdsPath.lexically_normal();
    }

    return normalizedPath;
}

HRESULT Material::Create_TextureSRV(const fs::path& texturePath, ID3D11ShaderResourceView** outSrv)
{
    if (outSrv == nullptr)
        return E_INVALIDARG;

    *outSrv = nullptr;

    const fs::path loadPath = Resolve_TextureLoadPath(texturePath);
    if (!fs::exists(loadPath) || !fs::is_regular_file(loadPath))
        return E_FAIL;

    const wstring cacheKey = loadPath.lexically_normal().wstring();

    {
        lock_guard<mutex> lock(g_TextureSrvCacheMutex);

        auto it = g_TextureSrvCache.find(cacheKey);
        if (it != g_TextureSrvCache.end() && it->second != nullptr)
        {
            return it->second.CopyTo(outSrv);
        }
    }

    ComPtr<ID3D11ShaderResourceView> createdSrv = nullptr;

    const fs::path ext = loadPath.extension();
    HRESULT hr = S_OK;

    if (ext == ".dds" || ext == ".DDS")
    {
        hr = DirectX::CreateDDSTextureFromFile(
            _device.Get(),
            loadPath.c_str(),
            nullptr,
            createdSrv.GetAddressOf());
    }
    else
    {
        hr = DirectX::CreateWICTextureFromFile(
            _device.Get(),
            loadPath.c_str(),
            nullptr,
            createdSrv.GetAddressOf());
    }

    if (FAILED(hr))
    {
        return hr;
    }

    bool shouldLogCacheCreate = false;
    size_t textureSrvCacheSize = 0;
    uint64 textureSrvCacheCreateCount = 0;
    wstring textureSrvCacheLastKey = {};

    {
        lock_guard<mutex> lock(g_TextureSrvCacheMutex);

        auto it = g_TextureSrvCache.find(cacheKey);
        if (it != g_TextureSrvCache.end() && it->second != nullptr)
            return it->second.CopyTo(outSrv);

        g_TextureSrvCache.emplace(cacheKey, createdSrv);
        ++g_TextureSrvCacheCreateCount;

        if (g_TextureSrvCache.size() == 1 || g_TextureSrvCache.size() % 64 == 0)
        {
            shouldLogCacheCreate = true;
            textureSrvCacheSize = g_TextureSrvCache.size();
            textureSrvCacheCreateCount = g_TextureSrvCacheCreateCount;
            textureSrvCacheLastKey = cacheKey;
        }
    }

    if (shouldLogCacheCreate)
    {
        LOG_INFO(
            "[Material][LeakWatch] Texture SRV cache size={} createdTotal={} last='{}'",
            textureSrvCacheSize,
            textureSrvCacheCreateCount,
            String::ToString(textureSrvCacheLastKey));
    }

    return createdSrv.CopyTo(outSrv);
}

void Material::Refresh_PassIndex()
{
    _passDirty = false;

    if (_forcedPassIndex > 0)
        return;

    _passIndex = ShaderPassFinder::Find_PassIndex(_shaderType, _renderState);
}

Shared<Material> Material::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize)
{
    auto instance = make_shared<Material>(device, context);

    if (FAILED(instance->Initialize(modelPath, materialData, slots, maxTextureTypeSize)))
    {
        LOG_ERROR("[MODEL] Failed to Created : Material");
        return nullptr;
    }

    return instance;
}

void Material::Free()
{
    __super::Free();

    _materials.clear();
}

void Material::Clear_TextureSRVCache()
{
    size_t textureSrvCacheSize = 0;
    uint64 textureSrvCacheCreateCount = 0;

    {
        lock_guard<mutex> lock(g_TextureSrvCacheMutex);
        textureSrvCacheSize = g_TextureSrvCache.size();
        textureSrvCacheCreateCount = g_TextureSrvCacheCreateCount;
        g_TextureSrvCache.clear();
    }

    LOG_INFO(
        "[Material][LeakWatch] Clearing Texture SRV cache. size={} createdTotal={}",
        textureSrvCacheSize,
        textureSrvCacheCreateCount);
}

size_t Material::Get_TextureSRVCacheSize()
{
    lock_guard<mutex> lock(g_TextureSrvCacheMutex);
    return g_TextureSrvCache.size();
}
