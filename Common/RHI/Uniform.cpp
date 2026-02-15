#include "Uniform.h"

#include "BufferAllocator.h"

Uniform::Uniform(std::vector<BufferDesc>& bufferDescs)
{
    BufferAllocator* bufferAlloc = BufferAllocator::GetInstance();
    for (auto desc : bufferDescs)
        Buffers.push_back(bufferAlloc->CreateBuffer(desc));
    
}

std::vector<DescriptorSetBinding> Uniform::GetBindings()
{
    std::vector<DescriptorSetBinding> bindings;
    for (uint32_t i = 0; i < Buffers.size(); ++i)
        bindings.emplace_back(DescriptorSetBinding { .Binding = i, .ResourceID = Buffers[i] });
        
    
    return bindings;   
}
