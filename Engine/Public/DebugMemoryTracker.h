#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

struct DebugMemoryTraceEvent
{
    string category{};
    string owner{};
    string path{};
    size_t estimatedBytes{};
    size_t countedBytes{};
    uint64 sequence{};
};

struct DebugMemoryTraceCategory
{
    string category{};
    uint32 eventCount{};
    size_t estimatedBytes{};
    size_t countedBytes{};
};

struct DebugMemoryTracePath
{
    string category{};
    string owner{};
    string path{};
    size_t estimatedBytes{};
    size_t countedBytes{};
    uint32 eventCount{};
};

struct DebugMemoryTraceSnapshot
{
    bool enabled{};
    uint64 sequence{};
    vector<DebugMemoryTraceCategory> categories{};
    vector<DebugMemoryTracePath> topPaths{};
    vector<DebugMemoryTraceEvent> recentEvents{};
};

class ENGINE_DLL DebugMemoryTracker final
{
public:
    static bool Is_Enabled();
    static void Set_Enabled(bool enabled);
    static void Clear();

    static void Record_TextureSRV(
        const char* category,
        const char* owner,
        const wstring& path,
        ID3D11ShaderResourceView* srv);

    static void Record_TextureSRV(
        const char* category,
        const char* owner,
        const fs::path& path,
        ID3D11ShaderResourceView* srv);

    static void Record_CPUBytes(
        const char* category,
        const char* owner,
        const wstring& path,
        size_t bytes);

    static void Record_CPUBytes(
        const char* category,
        const char* owner,
        const fs::path& path,
        size_t bytes);

    static DebugMemoryTraceSnapshot Build_Snapshot(size_t topPathLimit, size_t recentEventLimit);

    static size_t Estimate_TextureSRV_Bytes(ID3D11ShaderResourceView* srv);
    static string Guess_ModelCategory(const wstring& path);
    static string Guess_TextureCategory(const wstring& path);

private:
    DebugMemoryTracker() = delete;
};

NS_END
