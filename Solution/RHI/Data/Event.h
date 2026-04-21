#pragma once
#include <functional>
#include <unordered_map>

template<typename... Args>
class Event
{
public:
    using Delegate = std::function<void(Args...)>;
    using Handle = size_t;
private:
    std::unordered_map<Handle, Delegate> Delegates;
    Handle nextHandle = 0;
public:
    Handle Subscribe(Delegate d)
    {
        Delegates[nextHandle] = std::move(d);
        return nextHandle++;
    }

    void Unsubscribe(Handle h)
    {
        Delegates.erase(h);
    }

    void Invoke(Args... args)
    {
        for(auto& d : Delegates)
            d.second(args...);
    }

    void operator()(Args... args)
    {
        Invoke(args...);
    }
};
