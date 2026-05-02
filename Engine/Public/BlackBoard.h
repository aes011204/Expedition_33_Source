#pragma once
#include "Base.h"
#include <any>
NS_BEGIN(Engine)
class BlackBoard :
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



private:
    unordered_map< string, any> _data;


};

NS_END
