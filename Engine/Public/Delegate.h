#pragma once

NS_BEGIN(Engine)

struct FDelegateHandle
{
    size_t ID = 0;

    bool IsValid() const { return ID != 0; }
    void Reset() { ID = 0; }

    bool operator == (const FDelegateHandle& other) const { return ID == other.ID; }
    bool operator != (const FDelegateHandle& other) const { return ID != other.ID; }
};

template<typename... Args>
class Delegate
{
public:
    using FunctionType = function<void(Args...)>;

public:
    Delegate() = default;
    ~Delegate() { _functions.clear(); }

public:
    FDelegateHandle Add(const FunctionType& func)
    {
        FDelegateHandle handle{ ++_nextId };
        _functions.push_back({ handle, func, nullptr, false, weak_ptr<void>() });

        return handle;
    }

    // Weak_Ptr 버전
    template<typename T>
    FDelegateHandle Add(T* obj, void (T::*memberFunc)(Args...))
    {
        auto sharedPtr = obj->shared_from_this();
        auto typedPtr = static_pointer_cast<T>(sharedPtr);

        FDelegateHandle handle{ ++_nextId };
        weak_ptr<T> weakObj = typedPtr;

        auto boundFunc = [weakObj, memberFunc](Args... args)
            {
                if (auto sp = weakObj.lock())
                {
                    (sp.get()->*memberFunc)(args...);
                }
            };

        _functions.push_back({ handle, boundFunc, nullptr, true, weakObj });

        return handle;
    }

    // Raw_Ptr 버전을 만들어야할지?

    // Remove
    void Remove(FDelegateHandle handle)
    {
        if (!handle.IsValid())
            return;

        if (_isBroadcasting)
        {
            _pendingRemovals.emplace_back(handle);
        }
        else
        {
            Remove_Internal(handle);
        }
    }

    // RemoveAll: 특정 객체의 모든 바인딩 제거
    void RemoveAll(void* object)
    {
        if (!object) return;
        if (_isBroadcasting)
        {
            for (const auto& entry : _functions)
            {
                if (entry.ownerRaw == object)
                {
                    _pendingRemovals.push_back(entry.handle);
                }
            }
        }
        else
        {
            _functions.erase(
                remove_if(_functions.begin(), _functions.end(),
                    [object](const FEntry& e)
                    {
                        return e.ownerRaw == object;
                    }),
                _functions.end());
        }
    }

    // Clear : 모든 바인딩 제거
    void Clear()
    {
        if (_isBroadcasting)
        {
            _clearRequested = true;
        }
        else
        {
            _functions.clear();
        }
    }

    // Broadcast : 등록된 모든 함수 호출
    void Broadcast(Args... args)
    {
        _isBroadcasting = true;

        for (const auto& entry : _functions)
        {
            if (entry.isWeakBound && entry.ownerWeak.expired())
            {
                _pendingRemovals.push_back(entry.handle);
                continue;
            }

            if (entry.func)
            {
                entry.func(args...);
            }
        }

        _isBroadcasting = false;
        ProcessPendingRemovals();
    }

    size_t  GetBindingCount() const { return _functions.size(); }
    bool    IsBound() const { return !_functions.empty(); }

private:
    void Remove_Internal(FDelegateHandle handle)
    {
        auto it = find_if(_functions.begin(), _functions.end(),
            [&](const FEntry& e)
            {
                return e.handle == handle;
            });

        if (it != _functions.end())
        {
            _functions.erase(it);
        }
    }

    void ProcessPendingRemovals()
    {
        if (_clearRequested)
        {
            _functions.clear();
            _pendingRemovals.clear();
            _clearRequested = false;
            return;
        }

        for (const auto& handle : _pendingRemovals)
        {
            Remove_Internal(handle);
        }

        _pendingRemovals.clear();
    }

private:
    struct FEntry
    {
        FDelegateHandle handle;
        FunctionType    func;
        void*           ownerRaw = nullptr; // 혹시 모를 Raw-Pointer 호환용
        bool            isWeakBound = false;
        weak_ptr<void>  ownerWeak;
    };

private:
    vector<FEntry>          _functions;
    vector<FDelegateHandle> _pendingRemovals;
    size_t                  _nextId = 0;
    bool                    _isBroadcasting = false;
    bool                    _clearRequested = false;

};

// 언리얼 스타일 매크로
#define DECLARE_DELEGATE(DelegateName, ...) \
    using DelegateName = Engine::Delegate<__VA_ARGS__>

NS_END
