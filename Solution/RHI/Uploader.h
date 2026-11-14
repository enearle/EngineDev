#pragma once
#include <cstdint>
#include "RHI_API_Macro.h"
#include "RHI/RHIStructures.h"

class RHI_API Uploader
{
public:
    using BufferID = RHIStructures::BufferHandle;
    using DescriptorID = RHIStructures::DescriptorSetHandle;

    static BufferID UploadDynamic(size_t dataSize, const void* data);
    static BufferID UploadStatic(size_t dataSize, const void* data);
    static BufferID UploadVertices(size_t dataSize, const void* data);
    static BufferID UploadIndices(size_t dataSize, const void* data);

    static void UpdateDynamic(BufferID bufferID, size_t dataSize, const void* data);
    static void Release(BufferID bufferID);

    static const RHIStructures::BufferAllocation& GetBufferAllocation(BufferID bufferID);
    static DescriptorID AllocateDescriptor(BufferID bufferID);
};
