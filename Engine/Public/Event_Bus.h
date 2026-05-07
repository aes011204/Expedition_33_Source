#pragma once
#include "Base.h"
#include "Engine_Define.h"
#include "Event_Struct.h"
#include <typeindex>

NS_BEGIN(Engine)

class Event_Bus : public Base
{
public:
    Event_Bus();
    virtual ~Event_Bus();

public:
    using AnyHandler = function<void(const void*)>;

public:
    struct QueueEvent {type_index _typeIndex; Shared<void> data;};
    struct HandlerEntry {EvtHandle handle; AnyHandler handler;};

public:
    template<typename EventT>
    EvtHandle Subscribe(function<void(const EventT&)> function)
    {
        EvtHandle handle { ++_nextHandleId };
        
        _handlers[typeid(EventT)].push_back
        (
            HandlerEntry{handle, [function](const void* event)
            {
                function(*static_cast<const EventT*>(event));
            }});
        return handle;
    }

    void Unsubscribe(EvtHandle handle)
    {
        if(!handle.IsValid())
            return;

        for(auto& [type, handlerList] : _handlers)
        {
            handlerList.erase(remove_if(handlerList.begin(), handlerList.end(),
                [handle](const HandlerEntry& entry){ return entry.handle.id == handle.id;}),handlerList.end());
        }
    }

    template<typename EventT>
    void Publish(const EventT& event)
    {
        auto iter = _handlers.find(typeid(EventT));

        if(iter == _handlers.end())
            return;

        for (const auto& entry : iter->second)
            entry.handler(&event);
    }

    template<typename EventT>
    void Enqueue(const EventT& event)
    {
        _eventQueue.push(QueueEvent{typeid(EventT), make_shared<EventT>(event)});
    }

    void Pump()
    {
        while(!_eventQueue.empty())
        {
            QueueEvent queuedEvent = _eventQueue.front();
            _eventQueue.pop();

             auto  iter = _handlers.find(queuedEvent._typeIndex);

            if(iter == _handlers.end())
                continue;

            for(const auto& entry : iter->second)
                entry.handler(queuedEvent.data.get());
        }
    }

private:
    unordered_map<type_index, vector<HandlerEntry>> _handlers;
    queue<QueueEvent> _eventQueue = {};
    size_t _nextHandleId = 0;
};
NS_END
