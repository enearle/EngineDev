#pragma once
#include <functional>
#include "../ENGINE_API_Macro.h"
using CommandCallback = std::function<void(double)>;

class ENGINE_API CommandQueue {
    struct Impl;
    Impl* mImpl;

public:
    CommandQueue();
    ~CommandQueue();
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;

    void push(const CommandCallback& cmd);
    CommandCallback pop();
    bool isEmpty() const;
    void clear();
};
