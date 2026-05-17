#pragma once
#include <cstdint>
#include "RHI_API_Macro.h"


namespace RHIStructures
{
    struct BufferAllocation;
}

class RHI_API Uploader
{
public:
    using BufferID = uint64_t;
    using DescriptorID = uint64_t;
    
    static BufferID UploadDynamic(size_t dataSize, void* data);
    static BufferID UploadStatic(size_t dataSize, void* data);
    static BufferID UploadVertices(size_t dataSize, void* data);
    static BufferID UploadIndices(size_t dataSize, void* data);

    static void UpdateDynamic(BufferID bufferID, size_t dataSize, void* data);
    
    static const RHIStructures::BufferAllocation& GetBufferAllocation(BufferID bufferID);
    static DescriptorID AllocateDescriptor(BufferID bufferID);
};
