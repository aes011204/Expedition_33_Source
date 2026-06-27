#include "DebugMemoryTracker.h"
#include "Helper_String.h"

NS_BEGIN(Engine)

namespace
{
    constexpr size_t kMaxRecentEvents = 512;

    struct PathKey
    {
        string category{};
        string owner{};
        string path{};

        bool operator==(const PathKey& rhs) const
        {
            return category == rhs.category && owner == rhs.owner && path == rhs.path;
        }
    };

    struct PathKeyHash
    {
        size_t operator()(const PathKey& key) const
        {
            const size_t h1 = hash<string>{}(key.category);
            const size_t h2 = hash<string>{}(key.owner);
            const size_t h3 = hash<string>{}(key.path);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    struct PathStats
    {
        size_t estimatedBytes{};
        size_t countedBytes{};
        uint32 eventCount{};
    };

    struct TrackerState
    {
        mutex lock{};
        bool enabled{};
        uint64 sequence{};
        unordered_set<void*> countedResources{};
        unordered_map<string, DebugMemoryTraceCategory> categories{};
        unordered_map<PathKey, PathStats, PathKeyHash> paths{};
        deque<DebugMemoryTraceEvent> recentEvents{};
    };

    TrackerState& Get_TrackerState()
    {
        static TrackerState state{};
        return state;
    }

    bool Is_BlockCompressed(DXGI_FORMAT format, size_t& outBlockBytes)
    {
        switch (format)
        {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            outBlockBytes = 8;
            return true;

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            outBlockBytes = 16;
            return true;

        default:
            outBlockBytes = 0;
            return false;
        }
    }

    size_t Bits_Per_Pixel(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 128;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 96;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return 64;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return 32;

        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            return 16;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 8;

        default:
            return 0;
        }
    }

    size_t Estimate_Texture2D_Bytes(const D3D11_TEXTURE2D_DESC& desc)
    {
        size_t blockBytes = 0;
        const bool compressed = Is_BlockCompressed(desc.Format, blockBytes);
        const size_t bitsPerPixel = Bits_Per_Pixel(desc.Format);
        const uint32 mipLevels = max<uint32>(1, desc.MipLevels);
        size_t totalBytes = 0;

        for (uint32 mip = 0; mip < mipLevels; ++mip)
        {
            const size_t width = max<size_t>(1, static_cast<size_t>(desc.Width) >> mip);
            const size_t height = max<size_t>(1, static_cast<size_t>(desc.Height) >> mip);

            if (compressed)
            {
                const size_t blocksWide = max<size_t>(1, (width + 3) / 4);
                const size_t blocksHigh = max<size_t>(1, (height + 3) / 4);
                totalBytes += blocksWide * blocksHigh * blockBytes * desc.ArraySize;
            }
            else if (bitsPerPixel > 0)
            {
                totalBytes += width * height * bitsPerPixel / 8 * desc.ArraySize;
            }
        }

        return totalBytes;
    }

    string Normalize_Path(const wstring& path)
    {
        if (path.empty())
            return {};

        return fs::path(path).lexically_normal().generic_string();
    }

    string Lower_Path(const wstring& path)
    {
        return String::ToLowerCopy(Normalize_Path(path));
    }

    void Add_Record(const string& category, const string& owner, const string& path, size_t estimatedBytes, size_t countedBytes)
    {
        TrackerState& state = Get_TrackerState();
        lock_guard<mutex> lock(state.lock);

        if (!state.enabled)
            return;

        const uint64 sequence = ++state.sequence;

        DebugMemoryTraceCategory& categoryStats = state.categories[category];
        categoryStats.category = category;
        categoryStats.eventCount++;
        categoryStats.estimatedBytes += estimatedBytes;
        categoryStats.countedBytes += countedBytes;

        PathKey key{ category, owner, path };
        PathStats& pathStats = state.paths[key];
        pathStats.estimatedBytes += estimatedBytes;
        pathStats.countedBytes += countedBytes;
        pathStats.eventCount++;

        state.recentEvents.push_back(DebugMemoryTraceEvent{
            category,
            owner,
            path,
            estimatedBytes,
            countedBytes,
            sequence });

        while (state.recentEvents.size() > kMaxRecentEvents)
            state.recentEvents.pop_front();
    }
}

bool DebugMemoryTracker::Is_Enabled()
{
    TrackerState& state = Get_TrackerState();
    lock_guard<mutex> lock(state.lock);
    return state.enabled;
}

void DebugMemoryTracker::Set_Enabled(bool enabled)
{
    TrackerState& state = Get_TrackerState();
    lock_guard<mutex> lock(state.lock);
    state.enabled = enabled;
}

void DebugMemoryTracker::Clear()
{
    TrackerState& state = Get_TrackerState();
    lock_guard<mutex> lock(state.lock);
    state.sequence = 0;
    state.countedResources.clear();
    state.categories.clear();
    state.paths.clear();
    state.recentEvents.clear();
}

void DebugMemoryTracker::Record_TextureSRV(
    const char* category,
    const char* owner,
    const wstring& path,
    ID3D11ShaderResourceView* srv)
{
    if (srv == nullptr || !Is_Enabled())
        return;

    const size_t estimatedBytes = Estimate_TextureSRV_Bytes(srv);
    size_t countedBytes = 0;

    {
        TrackerState& state = Get_TrackerState();
        lock_guard<mutex> lock(state.lock);

        if (!state.enabled)
            return;

        if (state.countedResources.insert(srv).second)
            countedBytes = estimatedBytes;
    }

    const string fallbackCategory = Guess_TextureCategory(path);
    Add_Record(
        category != nullptr && category[0] != '\0' ? category : fallbackCategory,
        owner != nullptr ? owner : "",
        Normalize_Path(path),
        estimatedBytes,
        countedBytes);
}

void DebugMemoryTracker::Record_TextureSRV(
    const char* category,
    const char* owner,
    const fs::path& path,
    ID3D11ShaderResourceView* srv)
{
    Record_TextureSRV(category, owner, path.wstring(), srv);
}

void DebugMemoryTracker::Record_CPUBytes(
    const char* category,
    const char* owner,
    const wstring& path,
    size_t bytes)
{
    if (!Is_Enabled())
        return;

    const string fallbackCategory = Guess_ModelCategory(path);
    Add_Record(
        category != nullptr && category[0] != '\0' ? category : fallbackCategory,
        owner != nullptr ? owner : "",
        Normalize_Path(path),
        bytes,
        bytes);
}

void DebugMemoryTracker::Record_CPUBytes(
    const char* category,
    const char* owner,
    const fs::path& path,
    size_t bytes)
{
    Record_CPUBytes(category, owner, path.wstring(), bytes);
}

DebugMemoryTraceSnapshot DebugMemoryTracker::Build_Snapshot(size_t topPathLimit, size_t recentEventLimit)
{
    TrackerState& state = Get_TrackerState();
    lock_guard<mutex> lock(state.lock);

    DebugMemoryTraceSnapshot snapshot{};
    snapshot.enabled = state.enabled;
    snapshot.sequence = state.sequence;

    snapshot.categories.reserve(state.categories.size());
    for (const auto& [_, stats] : state.categories)
        snapshot.categories.push_back(stats);

    sort(snapshot.categories.begin(), snapshot.categories.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.countedBytes != rhs.countedBytes)
            return lhs.countedBytes > rhs.countedBytes;
        return lhs.category < rhs.category;
    });

    snapshot.topPaths.reserve(state.paths.size());
    for (const auto& [key, stats] : state.paths)
    {
        snapshot.topPaths.push_back(DebugMemoryTracePath{
            key.category,
            key.owner,
            key.path,
            stats.estimatedBytes,
            stats.countedBytes,
            stats.eventCount });
    }

    sort(snapshot.topPaths.begin(), snapshot.topPaths.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.countedBytes != rhs.countedBytes)
            return lhs.countedBytes > rhs.countedBytes;
        if (lhs.estimatedBytes != rhs.estimatedBytes)
            return lhs.estimatedBytes > rhs.estimatedBytes;
        return lhs.path < rhs.path;
    });

    if (snapshot.topPaths.size() > topPathLimit)
        snapshot.topPaths.resize(topPathLimit);

    const size_t copyCount = min(recentEventLimit, state.recentEvents.size());
    snapshot.recentEvents.reserve(copyCount);

    auto iter = state.recentEvents.end();
    for (size_t i = 0; i < copyCount; ++i)
    {
        --iter;
        snapshot.recentEvents.push_back(*iter);
    }

    return snapshot;
}

size_t DebugMemoryTracker::Estimate_TextureSRV_Bytes(ID3D11ShaderResourceView* srv)
{
    if (srv == nullptr)
        return 0;

    ComPtr<ID3D11Resource> resource{};
    srv->GetResource(resource.GetAddressOf());
    if (resource == nullptr)
        return 0;

    ComPtr<ID3D11Texture2D> texture2D{};
    if (SUCCEEDED(resource.As(&texture2D)) && texture2D != nullptr)
    {
        D3D11_TEXTURE2D_DESC desc{};
        texture2D->GetDesc(&desc);
        return Estimate_Texture2D_Bytes(desc);
    }

    return 0;
}

string DebugMemoryTracker::Guess_ModelCategory(const wstring& path)
{
    const string lower = Lower_Path(path);

    if (lower.find("/map/") != string::npos || lower.find("/maps/") != string::npos)
        return "map.model";

    if (lower.find("/heros/") != string::npos ||
        lower.find("/heroes/") != string::npos ||
        lower.find("/weapons/") != string::npos)
    {
        return "player.model";
    }

    return "model";
}

string DebugMemoryTracker::Guess_TextureCategory(const wstring& path)
{
    const string lower = Lower_Path(path);

    if (lower.find("/map/") != string::npos || lower.find("/maps/") != string::npos)
        return "map.texture";

    if (lower.find("/ui/") != string::npos)
        return "ui.texture";

    if (lower.find("/heros/") != string::npos ||
        lower.find("/heroes/") != string::npos ||
        lower.find("/weapons/") != string::npos)
    {
        return "player.texture";
    }

    if (lower.ends_with(".png"))
        return "texture.png";

    if (lower.ends_with(".dds"))
        return "texture.dds";

    return "texture";
}

NS_END
