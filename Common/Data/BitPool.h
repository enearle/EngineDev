#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

// Bitpool pattern
// For fast O(n/64) iteration over contiguous elements
class BitPool
{
    std::vector<uint64_t> FreeBitmap;
    size_t StartingOffset;
    size_t Stride;
    uint32_t PoolSize;
    uint32_t AvailableCount;

public:
    BitPool() = default;

    void Initialize(size_t startingOffset, size_t stride, uint32_t poolSize)
    {
        if (stride == 0)
            throw std::invalid_argument("Stride cannot be zero");
    
        if (poolSize > (SIZE_MAX - startingOffset) / stride)
            throw std::invalid_argument("Offset overflow detected");
        
        StartingOffset = startingOffset;
        Stride = stride;
        PoolSize = poolSize;
        AvailableCount = poolSize;
        
        uint32_t bitmapSize = (poolSize + 63) / 64;
        FreeBitmap.resize(bitmapSize, 0xFFFFFFFFFFFFFFFFULL);
        
        uint32_t bitsUsed = poolSize % 64;
        if (bitsUsed > 0)
        {
            uint64_t mask = (1ULL << bitsUsed) - 1;
            FreeBitmap.back() = mask;
        }
    }

    size_t Allocate()
    {
        if (AvailableCount == 0) 
            throw std::runtime_error("BitPool exhausted");
        
        uint32_t index = FindFirstSetBit();
        if (index == UINT32_MAX)
            throw std::runtime_error("BitPool corrupted");
        
        FreeBitmap[index / 64] &= ~(1ULL << (index % 64));
        AvailableCount--;
        
        return StartingOffset + (index * Stride);
    }

    void Free(size_t offset)
    {
        if (offset < StartingOffset || offset >= StartingOffset + (PoolSize * Stride))
            throw std::invalid_argument("Offset out of pool range");
        
        uint32_t index = (offset - StartingOffset) / Stride;
        
        if ((FreeBitmap[index / 64] & (1ULL << (index % 64))) != 0)
            throw std::logic_error("Double-free detected");
        
        FreeBitmap[index / 64] |= (1ULL << (index % 64));
        AvailableCount++;
    }

    uint32_t GetAvailableCount() const { return AvailableCount; }
    uint32_t GetTotalCount() const { return PoolSize; }

private:
    uint32_t FindFirstSetBit()
    {
        for (uint32_t i = 0; i < FreeBitmap.size(); ++i) 
            if (FreeBitmap[i] != 0) 
                return i * 64 + std::countr_zero(FreeBitmap[i]);

        return UINT32_MAX;
    }
};
