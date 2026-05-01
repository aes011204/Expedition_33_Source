#pragma once
#include "Base.h"

NS_BEGIN(Engine)

// Editor 콘솔 출력용 로그 1건을 표현하는 전송 데이터
struct ConsoleLogEntry
{
    uint32 sequence{};
    LogLevel logLevel{ LogLevel::Info };
    std::string formattedLine{};
    std::string payload{};
};

namespace Internal
{
    class ConsoleSink;
}

struct Log_Manager_Impl;

// spdlog 라이브러리를 래핑하여 프레임워크 전역 로깅 환경을 설정하는 관리 클래스
class Log_Manager final : public Base
{
public:
    Log_Manager();
    ~Log_Manager() override;

public:
    /**
     * @brief 로깅 시스템을 초기화하고 전역 로거를 등록한다
     * @param projectName 실행 프로젝트 이름 ("GameLauncher", "MainEditor" 등), 로그 파일명으로 사용됨
     * @return S_OK: 초기화 성공 / E_FAIL: spdlog 예외 발생 시 실패
    */
    HRESULT Initialize(const string& projectName);

public: //## Accessors
    // 현재 Console 로그 버퍼에 보관 중인 엔트리 개수를 반환한다.
    uint32 Get_ConsoleLogCount() const;
    // Console 로그 버퍼의 최대 보관 개수를 반환한다.
    uint32 Get_ConsoleLogCapacity() const;
    // Console 로그 버퍼 최대 보관 개수를 변경한다.
    void Set_ConsoleLogCapacity(uint32 capacity);
    // Console 로그 버퍼 리비전 값을 반환한다.
    uint32 Get_ConsoleLogRevision() const;

public: //## Helpers
    // Console 로그 버퍼를 람다 함수로 순회한다.
    void Visit_ConsoleLogs(const function<void(const ConsoleLogEntry&)>& visitor) const;
    // Console 로그 버퍼를 비운다.
    void Clear_ConsoleLogs();

private:
    Unique<Log_Manager_Impl> _impl{};

public:
    static Unique<Log_Manager> Create(const string& projectName);
    void Free() override;
};

NS_END
