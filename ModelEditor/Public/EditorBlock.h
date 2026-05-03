#pragma once
#include "Base.h"

NS_BEGIN(ModelEditor)

class EditorBlock abstract : public Base
{
public:
    EditorBlock() = default;
    virtual ~EditorBlock() override = default;

protected:
    virtual HRESULT Initialize() { return S_OK; };

public:
    bool ToggleBlock()
    {
        _showLog = !_showLog;
        return _showLog;
    }
    virtual void Update(float timeDelta) {};
    virtual void Render() {};

protected:
    bool _showLog{ true };
};

NS_END
