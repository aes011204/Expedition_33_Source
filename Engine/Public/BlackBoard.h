#pragma once
#include "Base.h"
#include <any>
NS_BEGIN(Engine)
class ENGINE_DLL  BlackBoard :
    public Base
{
public:
    BlackBoard();
    virtual ~BlackBoard();
public:
    template<typename T>
    void Set_Value(const string& key, T value)
    {
        _data[key] = value;
    }
    template<typename T>
    T Get_Value(const string& key)
    {

        auto it = _data.find(key);
        if (it == _data.end())
        {
            MSG_BOX("ERRER KEY NOT EXIST");
            assert("ERERERE");
           

        }

        return any_cast<T>(it->second);
    }

    bool Has_Value(const string& key) const
    {
        return _data.find(key) != _data.end();
    }

    const any* Find_Value(const string& key) const
    {
        auto it = _data.find(key);
        if (it == _data.end())
            return nullptr;

        return &it->second;
    }

private:
    unordered_map< string, any> _data;
    public:

    static Shared<BlackBoard> Create();
    //virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;

};

NS_END
