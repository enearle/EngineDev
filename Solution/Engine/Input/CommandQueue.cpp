#include "CommandQueue.h"
#include <queue>

struct CommandQueue::Impl {
    std::queue<CommandCallback> mQueue;
};

CommandQueue::CommandQueue() : mImpl(new Impl()) {}
CommandQueue::~CommandQueue() { delete mImpl; }

void CommandQueue::push(const CommandCallback& cmd)
{
    mImpl->mQueue.push(cmd);
}

CommandCallback CommandQueue::pop()
{
    CommandCallback cmd = mImpl->mQueue.front();
    mImpl->mQueue.pop();
    return cmd;
}

bool CommandQueue::isEmpty() const
{
    return mImpl->mQueue.empty();
}

void CommandQueue::clear()
{
    while (!mImpl->mQueue.empty())
        mImpl->mQueue.pop();
}
