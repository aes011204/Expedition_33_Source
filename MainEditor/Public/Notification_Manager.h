#pragma once

#include "Base.h"

#include <format>       
#include <string_view>

NS_BEGIN(MainEditor)

class Notification_Manager :public Base
{
    public:
    explicit Notification_Manager();
    virtual ~Notification_Manager();

public:
    void    Initialize();
    void    Update(float timeDelta);
    void    Render();

public:
    template<typename... Args>
    void Add_Notification(string_view format, Args&&... args)
    {
        Add_Notification_With_Type(NotifyType::Info, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Add_Notification_With_Type(NotifyType type, string_view format, Args&&... args)
    {
        string finalMessage;
        try {
            if constexpr (sizeof...(args) > 0)
                finalMessage = std::vformat(format, std::make_format_args(args...));
            else
                finalMessage = string(format);
        }
        catch (...) {
            finalMessage = "Format Error: " + string(format);
        }

        Add_Internal(finalMessage, type);
    }

private:
    void    Add_Internal(const string& message, NotifyType type = NotifyType::Info);

private:
    list<FNotification> _notifications;

public:
    static unique_ptr<Notification_Manager> Create();
    
};
NS_END
