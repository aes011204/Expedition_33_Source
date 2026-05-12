#pragma once

#include "Client_Defines.h"
#include "Base.h"

// 게임 플레이 시에 필요한 정적 테이블 데이터 담당.
// ResourceLoader는 리소스/프로토타입 생성,
// GameDataTable은 경험치, 스킬, 아이템 같은 데이터 담당

NS_BEGIN(Client)

class GameDataTable final : public Base
{
public:
    GameDataTable() = default;
    ~GameDataTable() override = default;

public:
    HRESULT Initialize();
    HRESULT Load_All();
    void Clear();

public:
    uint32 Get_NextExp(uint32 level) const;

private:
    HRESULT Load_ExpLevel(const wstring& jsonRootPath);

private:
    wstring Get_DefaultJsonRootPath() const;
    uint32  Get_FallbackNextExp(uint32 level) const;

private:
    // index == level. 0번은 사용하지 않는다.
    vector<uint32> _nextExpByLevel;
    bool _isLoaded = false;

public:
    static Unique<GameDataTable> Create();
    void Free() override;
};

NS_END
