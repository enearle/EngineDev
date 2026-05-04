#include "Uploader.h"

#include "RHI/BufferAllocator.h"
#include "RHI/RHIConstants.h"

Uploader::BufferID Uploader::UploadDynamic(size_t dataSize, void* data)
{
    BufferDesc bufferDesc = RHIConstants::DefaultDynamicBufferDesc;
    bufferDesc.Size = dataSize;
    bufferDesc.InitialData = data;
    return BufferAllocator::GetInstance()->CreateBuffer(bufferDesc);
}

Uploader::BufferID Uploader::UploadStatic(size_t dataSize, void* data)
{
    BufferDesc bufferDesc = RHIConstants::DefaultStaticUploadBufferDesc;
    bufferDesc.Size = dataSize;
    bufferDesc.InitialData = data;
    return BufferAllocator::GetInstance()->CreateBuffer(bufferDesc);
}

void Uploader::UpdateDynamic(BufferID bufferID, size_t dataSize, void* data)
{
    BufferAllocation alloc = BufferAllocator::GetInstance()->GetBufferAllocation(bufferID);
    if (alloc.IsMapped)
    {
        memcpy(alloc.Address, data, dataSize);
    }
}

const RHIStructures::BufferAllocation& Uploader::GetBufferAllocation(BufferID bufferID)
{
    return BufferAllocator::GetInstance()->GetBufferAllocation(bufferID);
}

Uploader::DescriptorID Uploader::AllocateDescriptor(BufferID bufferID)
{
    std::vector<DescriptorSetBinding> bindings;
    bindings.emplace_back(DescriptorSetBinding {
        .Binding = 0,
        .ResourceID = bufferID,
        .DynamicOffset = 0
    });
        
    return BufferAllocator::GetInstance()->AllocateDescriptorSet(
        0,
        RHIConstants::VARIANT_DESCRIPTOR_SET_BASE,
        bindings
    );
}
