#include "MipGenerator.h"

#include <algorithm>
#include <climits>
#include <deque>
#include "PassGraph.h"
#include "Renderer.h"
#include "ShaderLibrary.h"
#include "RHI/BufferAllocator.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"

using namespace RHIStructures;

static constexpr uint32_t GroupSize = 8;
static constexpr uint32_t DownsampleSetIndex = 1;

struct DownsampleConstants
{
    uint32_t DestinationWidth;
    uint32_t DestinationHeight;
    uint32_t Padding[2];
};

static std::deque<ImageHandle> Queue;
static PipelineLayout* DownsampleLayout = nullptr;
static Pipeline* DownsamplePipeline = nullptr;
static PassID GeneratorPass = UINT32_MAX;
static uint32_t GeneratedCount = 0;

static void GenerateMips(PipelineExecutor* executor, ImageHandle image)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    const ImageDesc& desc = allocator->GetImageAllocation(image).Desc;

    executor->BindPipeline(DownsamplePipeline);
    for (uint32_t mip = 1; mip < desc.MipLevels; mip++)
    {
        uint32_t width = (std::max)(desc.Width >> mip, 1u);
        uint32_t height = (std::max)(desc.Height >> mip, 1u);

        ImageTransition toStorage = { image, ImageLayout::ShaderReadOnly, ImageLayout::Storage, SubresourceRange::Mip(mip) };
        executor->TransitionImages({ &toStorage, 1 });

        DescriptorSetHandle set = allocator->AllocateDescriptorSet(DownsampleLayout->GetID(), DownsampleSetIndex, {
            DescriptorSetBinding::ForImage(0, image, SubresourceRange::Mip(mip - 1)),
            DescriptorSetBinding::ForImage(1, image, SubresourceRange::Mip(mip)),
        });
        executor->BindDescriptorSet(set);
        DownsampleConstants constants = { width, height, { 0, 0 } };
        executor->PushConstants(&constants, sizeof(constants));
        executor->Dispatch((width + GroupSize - 1) / GroupSize, (height + GroupSize - 1) / GroupSize, 1);
        allocator->FreeDescriptorSet(set);

        ImageTransition toRead = { image, ImageLayout::Storage, ImageLayout::ShaderReadOnly, SubresourceRange::Mip(mip) };
        executor->TransitionImages({ &toRead, 1 });
    }
}

static void ExecuteGenerator(const PassContext& context)
{
    for (uint32_t i = 0; i < MipGenerator::TexturesPerFrame && !Queue.empty(); i++)
    {
        ImageHandle image = Queue.front();
        Queue.pop_front();
        if (!BufferAllocator::GetInstance()->IsImageLive(image))
            continue;
        GenerateMips(context.Executor, image);
        GeneratedCount++;
    }
}

void MipGenerator::Start()
{
    ShaderStageMask computeStage(0);
    computeStage.SetCompute(true);

    ResourceLayout resources;
    resources.VisibleStages = computeStage;
    resources.Bindings = {
        { .Type = DescriptorType::SampledImage, .Slot = 0, .Set = DownsampleSetIndex, .Count = 1 },
        { .Type = DescriptorType::StorageImage, .Slot = 1, .Set = DownsampleSetIndex, .Count = 1 },
    };
    DownsampleLayout = PipelineLayout::Create(resources, { { .Size = sizeof(DownsampleConstants), .VisibleStages = computeStage } });

    ShaderStage shader = LoadShader("cs_downsample");
    DownsamplePipeline = Pipeline::CreateCompute(DownsampleLayout, shader);
    ReleaseShader(shader);

    PassDesc desc;
    desc.Name = "Mip generation";
    desc.Kind = PassKind::Compute;
    desc.Order = INT_MIN;
    desc.Execute = ExecuteGenerator;
    GeneratorPass = Renderer::GetPassGraph().AddPass(std::move(desc));
}

void MipGenerator::Shutdown()
{
    Queue.clear();
    delete DownsamplePipeline;
    delete DownsampleLayout;
    DownsamplePipeline = nullptr;
    DownsampleLayout = nullptr;
    GeneratorPass = UINT32_MAX;
}

uint32_t MipGenerator::FullMipCount(uint32_t width, uint32_t height)
{
    uint32_t count = 1;
    for (uint32_t size = (std::max)(width, height); size > 1; size >>= 1)
        count++;
    return count;
}

void MipGenerator::Enqueue(ImageHandle image)
{
    if (BufferAllocator::GetInstance()->GetImageAllocation(image).Desc.MipLevels > 1)
        Queue.push_back(image);
}

void MipGenerator::Cancel(ImageHandle image)
{
    std::erase(Queue, image);
}

uint32_t MipGenerator::GetGeneratedCount()
{
    return GeneratedCount;
}

uint32_t MipGenerator::GetQueueLength()
{
    return static_cast<uint32_t>(Queue.size());
}
