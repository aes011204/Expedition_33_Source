#pragma once

namespace MainEditor
{
    #define EDITOR GET_SINGLE(EditorInstance)

    // Notification
    #define NOTIFY(...) EditorInstance::GetInstance()->Get_Notification()->Add_Notification(__VA_ARGS__)
    #define NOTIFY_WARN(...) EditorInstance::GetInstance()->Get_Notification()->Add_Notification_With_Type(ENotifyType::Warning, __VA_ARGS__)
    
    #define U8(str) (const char*)u8##str
}
