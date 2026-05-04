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
        _showBlock = !_showBlock;
        return _showBlock;
    }
    virtual void Update(float timeDelta) {};
    virtual void Render() {};

protected:
    bool _showBlock{ true };
};

NS_END
