#pragma once
#include "EditorBlock.h"

NS_BEGIN(ModelEditor)

class LogWindow final : public EditorBlock
{
public:
    LogWindow() = default;
    ~LogWindow() override = default;

private:
    HRESULT Initialize() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

public:
    static Shared<LogWindow> Create();

};

NS_END
