#include "Print_Manager.h"
#include "GameInstance.h"

void Print_Manager::Initialize()
{
    _messages.clear();
}

void Print_Manager::Update(float timeDelta)
{
    if (_messages.empty())
        return;

    for (auto iter = _messages.begin(); iter != _messages.end();)
    {
        if (iter->persistent)
        {
            ++iter;
            continue;
        }

        iter->remainingTime -= timeDelta;

        if (iter->remainingTime <= 0.f)
            iter = _messages.erase(iter);
        else
            ++iter;
    }
}

void Print_Manager::Render()
{
    if (_messages.empty())
        return;

    Vec2 drawPosition = _startPosition;

    for (const RuntimePrint& message : _messages)
    {
        GAME->Draw_Font(
            _fontTag,
            message.text.c_str(),
            drawPosition,
            XMVectorSet(message.color.x, message.color.y, message.color.z, message.color.w));

        drawPosition.y += _lineHeight;
    }
}

void Print_Manager::Push_PrintRecord(
    const wstring& slot,
    const wstring& message,
    float duration,
    const Vec4& color,
    bool persistent)
{
    if (message.empty())
        return;

    if (persistent)
    {
        for (RuntimePrint& entry : _messages)
        {
            if (entry.persistent && entry.slot == slot)
            {
                entry.text = message;
                entry.color = color;
                return;
            }
        }
    }

    RuntimePrint print{};
    print.slot = slot;
    print.text = message;
    print.remainingTime = persistent ? 0.f : max(duration, 0.01f);
    print.color = color;
    print.persistent = persistent;

    _messages.push_back(print);

    size_t transientCount = 0;
    for (const RuntimePrint& entry : _messages)
    {
        if (!entry.persistent)
            ++transientCount;
    }

    while (transientCount > _maxTransientMessageCount)
    {
        for (auto iter = _messages.begin(); iter != _messages.end(); ++iter)
        {
            if (!iter->persistent)
            {
                _messages.erase(iter);
                --transientCount;
                break;
            }
        }
    }
}

void Print_Manager::Clear()
{
    _messages.clear();
}

void Print_Manager::Clear(const wstring& slot)
{
    for (auto iter = _messages.begin(); iter != _messages.end();)
    {
        if (iter->persistent && iter->slot == slot)
            iter = _messages.erase(iter);
        else
            ++iter;
    }
}

Unique<Print_Manager> Print_Manager::Create()
{
    auto instance = make_unique<Print_Manager>();
    instance->Initialize();
    return instance;
}
