#pragma once
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <vector>

template<typename Tag>
struct Handle
{
    uint32_t Index = UINT32_MAX;
    uint32_t Generation = 0;

    bool IsValid() const { return Index != UINT32_MAX; }
    uint64_t ToBits() const { return (static_cast<uint64_t>(Generation) << 32) | Index; }
    bool operator==(const Handle& other) const = default;
    bool operator<(const Handle& other) const { return ToBits() < other.ToBits(); }
};

// Slots live in a deque so a reference from Get() stays valid while other slots are claimed.
template<typename ValueType, typename HandleType>
class HandlePool
{
    struct Slot
    {
        ValueType Value = {};
        uint32_t Generation = 1;
        bool IsLive = false;
    };

    std::deque<Slot> Slots;
    std::vector<uint32_t> FreeIndices;
    uint32_t LiveCount = 0;

public:

    HandleType Claim(ValueType value)
    {
        uint32_t index;
        if (FreeIndices.empty())
        {
            index = static_cast<uint32_t>(Slots.size());
            Slots.emplace_back();
        }
        else
        {
            index = FreeIndices.back();
            FreeIndices.pop_back();
        }

        Slot& slot = Slots[index];
        slot.Value = std::move(value);
        slot.IsLive = true;
        LiveCount++;

        return HandleType{ index, slot.Generation };
    }

    void Release(HandleType handle)
    {
        Slot& slot = SlotFor(handle);
        slot.Value = {};
        slot.IsLive = false;
        slot.Generation++;
        FreeIndices.push_back(handle.Index);
        LiveCount--;
    }

    bool IsLive(HandleType handle) const
    {
        return handle.Index < Slots.size()
            && Slots[handle.Index].IsLive
            && Slots[handle.Index].Generation == handle.Generation;
    }

    ValueType& Get(HandleType handle) { return SlotFor(handle).Value; }
    const ValueType& Get(HandleType handle) const { return const_cast<HandlePool*>(this)->SlotFor(handle).Value; }
    uint32_t GetLiveCount() const { return LiveCount; }

    template<typename Function>
    void ForEachLive(Function&& function)
    {
        for (Slot& slot : Slots)
            if (slot.IsLive)
                function(slot.Value);
    }

    void Clear()
    {
        Slots.clear();
        FreeIndices.clear();
        LiveCount = 0;
    }

private:

    Slot& SlotFor(HandleType handle)
    {
        if (!IsLive(handle))
            throw std::runtime_error("Stale or invalid handle.");
        return Slots[handle.Index];
    }
};
