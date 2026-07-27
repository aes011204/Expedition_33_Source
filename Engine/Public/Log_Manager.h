#pragma once
#include "Base.h"

NS_BEGIN(Engine)

struct ConsoleLogEntry
{
    uint32 sequence{};
    LogLevel logLevel{ LogLevel::Info };
    std::string formattedLine{};
    std::string payload{};
};

struct Log_Manager_Impl;

class Log_Manager final : public Base
{
public:
    Log_Manager();
    ~Log_Manager() override;

public:
    HRESULT Initialize(const string& projectName);

public: //## Behavior::ConsoleBuffer
    uint32 Get_ConsoleLogCount() const;
    uint32 Get_ConsoleLogCapacity() const;
    void Set_ConsoleLogCapacity(uint32 capacity);
    uint32 Get_ConsoleLogRevision() const;

    void Visit_ConsoleLogs(const function<void(const ConsoleLogEntry&)>& visitor) const;
    void Clear_ConsoleLogs();

public: //## Behavior::FileOnlyLogging
    void Log_FileOnly(const spdlog::source_loc& sourceLoc, spdlog::level::level_enum level, const string& message) const;

private:
    Unique<Log_Manager_Impl> _impl{};

public:
    static Unique<Log_Manager> Create(const string& projectName);
    void Free() override;
};

NS_END
