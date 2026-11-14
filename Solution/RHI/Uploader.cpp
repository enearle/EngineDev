#include "Uploader.h"

#include "RHI/BufferAllocator.h"
#include "RHI/RHIConstants.h"

static Uploader::BufferID Upload(BufferDesc bufferDesc, size_t dataSize, const void* data)
{
    bufferDesc.Size = dataSize;
    bufferDesc.InitialData = data;
    return BufferAllocator::GetInstance()->CreateBuffer(bufferDesc);
}

Uploader::BufferID Uploader::UploadDynamic(size_t dataSize, const void* data)
{
    return Upload(RHIConstants::DefaultDynamicBufferDesc, dataSize, data);
}

Uploader::BufferID Uploader::UploadStatic(size_t dataSize, const void* data)
{
    return Upload(RHIConstants::DefaultStaticUploadBufferDesc, dataSize, data);
}

Uploader::BufferID Uploader::UploadVertices(size_t dataSize, const void* data)
{
    return Upload(RHIConstants::DefaultVertexBufferDesc, dataSize, data);
}

Uploader::BufferID Uploader::UploadIndices(size_t dataSize, const void* data)
{
    return Upload(RHIConstants::DefaultIndexBufferDesc, dataSize, data);
}

void Uploader::UpdateDynamic(BufferID bufferID, size_t dataSize, const void* data)
{
    const BufferAllocation& allocation = BufferAllocator::GetInstance()->GetBufferAllocation(bufferID);
    if (allocation.IsMapped)
        memcpy(allocation.Address, data, dataSize);
}

void Uploader::Release(BufferID bufferID)
{
    BufferAllocator::GetInstance()->DestroyBuffer(bufferID);
}

const RHIStructures::BufferAllocation& Uploader::GetBufferAllocation(BufferID bufferID)
{
    return BufferAllocator::GetInstance()->GetBufferAllocation(bufferID);
}

Uploader::DescriptorID Uploader::AllocateDescriptor(BufferID bufferID)
{
    std::vector<DescriptorSetBinding> bindings = { DescriptorSetBinding::ForBuffer(0, bufferID) };

    return BufferAllocator::GetInstance()->AllocateDescriptorSet(
        0,
        RHIConstants::VARIANT_DESCRIPTOR_SET_BASE,
        bindings
    );
}
