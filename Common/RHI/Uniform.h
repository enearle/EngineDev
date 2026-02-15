#pragma once
#include <vector>

#include "RHIStructures.h"

class Uniform
{
    std::vector<uint64_t> Buffers;
public:
    Uniform() = default;
    Uniform(std::vector<RHIStructures::BufferDesc>& bufferDescs);
    std::vector<RHIStructures::DescriptorSetBinding> GetBindings();
    std::vector<uint64_t> GetBuffers() { return Buffers;}
};
