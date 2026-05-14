#pragma once
#include <functional>

using CommandCallback = std::function<void(double)>;

class CommandQueue {
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
