#pragma once

#include "Base.h"

NS_BEGIN(MainEditor)

class Component_Inspector;

class Inspector_Factory : public Base
{
public:
    Inspector_Factory() = default;
    ~Inspector_Factory() override = default;

public:
    void Initialize();

    void Register_Inspector(const string& key, const Shared<Component_Inspector>& inspector);
    Shared<Component_Inspector> Get_Inspector(const string& key) const;

private:
    map<string, Shared<Component_Inspector>> _inspectors{};

public:
    static Unique<Inspector_Factory> Create();
    void Free() override;

};

NS_END
