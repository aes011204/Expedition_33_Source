#include "pch.h"
#include "Log_Manager.h"

namespace Engine::Internal
{
namespace
{
    LogLevel To_EngineLogLevel(const spdlog::level::level_enum level)
    {
        switch (level)
        {
        case spdlog::level::trace:
            return LogLevel::Trace;
        case spdlog::level::debug:
            return LogLevel::Debug;
        case spdlog::level::info:
            return LogLevel::Info;
        case spdlog::level::warn:
            return LogLevel::Warn;
        case spdlog::level::err:
            return LogLevel::Error;
        case spdlog::level::critical:
            return LogLevel::Critical;
        case spdlog::level::off:
        default:
            return LogLevel::Off;
        }
    }
}

// 에디터 Console 창에서 소비할 실시간 로그 버퍼를 관리하는 커스텀 Sink
class ConsoleSink final : public spdlog::sinks::base_sink<mutex>
{
public:
    explicit ConsoleSink(uint32 capacity = 5000u)
        : m_capacity(max(capacity, 1u))
    {
    }

public:
    void Visit_Logs(const function<void(const ConsoleLogEntry&)>& fnVisitor) const
    {
        if (!fnVisitor)
            return;

        lock_guard lock(m_LogMutex);
        for (const auto& tLog : m_Logs)
            fnVisitor(tLog);
    }

    void Clear_Logs()
    {
        lock_guard lock(m_LogMutex);
        m_Logs.clear();
        ++m_revision;
    }

    void Set_Capacity(uint32 capacity)
    {
        lock_guard lock(m_LogMutex);
        m_capacity = max(capacity, 1u);
        Trim_ToCapacity();
        ++m_revision;
    }

    uint32 Get_Capacity() const
    {
        lock_guard lock(m_LogMutex);
        return m_capacity;
    }

    uint32 Get_Count() const
    {
        lock_guard lock(m_LogMutex);
        return static_cast<uint32>(m_Logs.size());
    }

    uint32 Get_Revision() const
    {
        lock_guard lock(m_LogMutex);
        return m_revision;
    }

private:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);

        ConsoleLogEntry tNewLog{};
        tNewLog.sequence = m_nextSequence++;
        tNewLog.logLevel = To_EngineLogLevel(msg.level);
        tNewLog.formattedLine.assign(formatted.data(), formatted.size());
        tNewLog.payload.assign(msg.payload.data(), msg.payload.size());

        lock_guard lock(m_LogMutex);
        m_Logs.push_back(move(tNewLog));
        Trim_ToCapacity();
        ++m_revision;
    }

    void flush_() override
    {
    }

    void Trim_ToCapacity()
    {
        while (m_Logs.size() > m_capacity)
        {
            m_Logs.pop_front();
        }
    }

private:
    mutable mutex m_LogMutex{};
    deque<ConsoleLogEntry> m_Logs{};
    uint32 m_capacity{ 5000 };
    uint32 m_nextSequence{ 1 };
    uint32 m_revision{ 0 };
};
}

struct Engine::Log_Manager_Impl
{
    Shared<spdlog::logger> logger{};
    Shared<spdlog::logger> fileOnlyLogger{};
    Shared<Internal::ConsoleSink> consoleSink{};
};

Log_Manager::Log_Manager()
    : _impl(make_unique<Log_Manager_Impl>())
{
}

Log_Manager::~Log_Manager() = default;

HRESULT Log_Manager::Initialize(const string& projectName)
{
    //!! spdlog는 내부적으로 string 기반이므로 strProjectName으로 wstring 대신 string 사용

    try
    {
        namespace fs = filesystem;
        fs::path pathDir = "../../../logs/";

#ifdef _DEBUG
        pathDir /= "Debug/";
#else
        pathDir /= "Release/";
#endif

        // filesystem을 사용해서, 폴더가 없으면 생성
        if (!fs::exists(pathDir))
            fs::create_directories(pathDir);

        // filesystem::path -> _string 변환 (spdlog sink는 _string 경로 요구)
        const string logFileName = (pathDir / (projectName + ".log")).string();

        vector<spdlog::sink_ptr> sinks;

        // 1. Editor/Tool 실시간 Console sink
        _impl->consoleSink = make_shared<Internal::ConsoleSink>(5000u);
        _impl->consoleSink->set_level(spdlog::level::trace);
        sinks.push_back(_impl->consoleSink);

#ifdef _DEBUG
        // 2. Rotating File Sink (개발 기록용: 5MB, 10개)
        const auto spFileSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFileName,
            1024 * 1024 * 5,
            10
        );
        spFileSink->set_level(spdlog::level::trace);
        sinks.push_back(spFileSink);

        // 로거 생성
        _impl->logger = make_shared<spdlog::logger>(projectName, begin(sinks), end(sinks));
        _impl->fileOnlyLogger = make_shared<spdlog::logger>(projectName + ".FileOnly", spFileSink);
        _impl->fileOnlyLogger->set_level(spdlog::level::trace);

        // 전역 설정
        spdlog::set_default_logger(_impl->logger);
        spdlog::set_level(spdlog::level::trace);
#else
        // 1. Release: Rotating File Sink (5MB, 5개)
        const auto spFileSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFileName,
            1024 * 1024 * 5,
            5
        );
        spFileSink->set_level(spdlog::level::info);
        sinks.push_back(spFileSink);

        _impl->logger = make_shared<spdlog::logger>(projectName, begin(sinks), end(sinks));
        _impl->fileOnlyLogger = make_shared<spdlog::logger>(projectName + ".FileOnly", spFileSink);
        _impl->fileOnlyLogger->set_level(spdlog::level::info);

        // 전역 설정
        spdlog::set_default_logger(_impl->logger);
        spdlog::set_level(spdlog::level::info);
#endif

        // 패턴 설정: [시간] [로그레벨] [스레드ID] [소스 위치] 메시지
        constexpr auto logPattern = "[%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%!:%#] %v";
        spdlog::set_pattern(logPattern);
        _impl->fileOnlyLogger->set_pattern(logPattern);
        LOG_INFO("{:-^50}", " [ Logging Initialized: " + projectName + ".exe ] ");
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        OutputDebugStringA("Log initialization failed: ");
        OutputDebugStringA(ex.what());
        OutputDebugStringA("\n");
        return E_FAIL;
    }

    return S_OK;
}

uint32 Log_Manager::Get_ConsoleLogCount() const
{
    if (_impl && _impl->consoleSink)
        return _impl->consoleSink->Get_Count();

    return 0;
}

uint32 Log_Manager::Get_ConsoleLogCapacity() const
{
    if (_impl && _impl->consoleSink)
        return _impl->consoleSink->Get_Capacity();

    return 0;
}

void Log_Manager::Set_ConsoleLogCapacity(uint32 capacity)
{
    if (_impl && _impl->consoleSink)
        _impl->consoleSink->Set_Capacity(capacity);
}

uint32 Log_Manager::Get_ConsoleLogRevision() const
{
    if (_impl && _impl->consoleSink)
        return _impl->consoleSink->Get_Revision();

    return 0;
}

void Log_Manager::Visit_ConsoleLogs(const function<void(const ConsoleLogEntry&)>& visitor) const
{
    if (_impl && _impl->consoleSink)
        _impl->consoleSink->Visit_Logs(visitor);
}

void Log_Manager::Clear_ConsoleLogs()
{
    if (_impl && _impl->consoleSink)
        _impl->consoleSink->Clear_Logs();
}

void Log_Manager::Log_FileOnly(const spdlog::source_loc& sourceLoc, spdlog::level::level_enum level, const string& message) const
{
    if (_impl && _impl->fileOnlyLogger)
        _impl->fileOnlyLogger->log(sourceLoc, level, message);
}

Unique<Log_Manager> Log_Manager::Create(const string& projectName)
{
    auto instance = make_unique<Log_Manager>();

    if (FAILED(instance->Initialize(projectName)))
    {
        LOG_CRITICAL("Failed to Create : Log_Manager");
        MSG_BOX("Failed to Create : Log_Manager");
        return nullptr;
    }

    return instance;
}

void Log_Manager::Free()
{
    if (_impl && _impl->logger)
    {
        _impl->logger->flush();
        LOG_INFO("{:-^50}", " [ Logging Shutdown ] ");
    }

    if (_impl && _impl->fileOnlyLogger)
        _impl->fileOnlyLogger->flush();

    spdlog::shutdown();
    if (_impl)
    {
        _impl->consoleSink.reset();
        _impl->fileOnlyLogger.reset();
        _impl->logger.reset();
    }
}
