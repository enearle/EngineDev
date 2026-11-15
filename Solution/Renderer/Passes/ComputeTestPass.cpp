#include "ComputeTestPass.h"

#include <algorithm>
#include <iostream>
#include <string>
#include "../PassGraph.h"
#include "../Readback.h"
#include "../Renderer.h"
#include "../ShaderLibrary.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/RHIConstants.h"

using namespace RHIStructures;

static constexpr uint32_t TestSize = 256;
static constexpr uint32_t GroupSize = 8;
static constexpr uint32_t ShownSize = 256;
static constexpr uint32_t TestSetIndex = 1;

static bool IsEnlisted = false;
static GraphImageID TestImage = 0;
static PassID WritePass = 0;
static PassID ShowPass = 0;
static PipelineLayout* WriteLayout = nullptr;
static PipelineLayout* ShowLayout = nullptr;
static Pipeline* WritePipeline = nullptr;
static Pipeline* ShowPipeline = nullptr;

static uint32_t MatchCount = 0;
static uint64_t MinimumLatency = UINT64_MAX;
static uint64_t MaximumLatency = 0;
static std::string FirstMismatch;

struct TestConstants
{
    uint32_t Frame;
    uint32_t Padding[3];
};

// Mirrors cs_compute_test.hlsl.
static void ComputeTestTexel(uint32_t frame, uint32_t x, uint32_t y, uint8_t out[4])
{
    out[0] = static_cast<uint8_t>((x + frame) & 255);
    out[1] = static_cast<uint8_t>((y + frame * 3) & 255);
    out[2] = static_cast<uint8_t>(frame & 255);
    out[3] = 255;
}

static void CheckRow(const ReadbackResult& result, uint32_t row)
{
    uint32_t frame = static_cast<uint32_t>(result.RequestedFrame);
    for (uint32_t x = 0; x < result.Width && FirstMismatch.empty(); x++)
    {
        uint8_t expected[4];
        ComputeTestTexel(frame, x, row, expected);
        const uint8_t* actual = result.Data + x * 4;
        if (!std::equal(expected, expected + 4, actual))
            FirstMismatch = "frame " + std::to_string(frame) + " texel (" + std::to_string(x) + ", " + std::to_string(row) + "): expected "
                + std::to_string(expected[0]) + " " + std::to_string(expected[1]) + " " + std::to_string(expected[2]) + ", read "
                + std::to_string(actual[0]) + " " + std::to_string(actual[1]) + " " + std::to_string(actual[2]);
    }

    if (!FirstMismatch.empty())
        return;
    MatchCount++;
    uint64_t latency = result.DeliveredFrame - result.RequestedFrame;
    MinimumLatency = (std::min)(MinimumLatency, latency);
    MaximumLatency = (std::max)(MaximumLatency, latency);
}

static void ExecuteWrite(const PassContext& context)
{
    PipelineExecutor* executor = context.Executor;
    executor->BindPipeline(WritePipeline);
    executor->BindDescriptorSet(context.InputSet);

    uint64_t frame = executor->GetFrameNumber();
    TestConstants constants = { static_cast<uint32_t>(frame), { 0, 0, 0 } };
    executor->PushConstants(&constants, sizeof(constants));
    executor->Dispatch(TestSize / GroupSize, TestSize / GroupSize, 1);

    uint32_t row = static_cast<uint32_t>(frame % TestSize);
    Readback::Request(TestImage, { 0, 0, 0, row, TestSize, 1 }, [row](const ReadbackResult& result) { CheckRow(result, row); });
}

static void ExecuteShow(const PassContext& context)
{
    PipelineExecutor* executor = context.Executor;
    executor->BindPipeline(ShowPipeline);
    executor->BindDescriptorSet(context.InputSet);
    executor->SetViewport(context.Width - ShownSize - 16, context.Height - ShownSize - 16, ShownSize, ShownSize);
    executor->Draw(6, 1);
}

void ComputeTestPass::Enlist()
{
    if (IsEnlisted)
        return;

    ShaderStageMask computeStage(0);
    computeStage.SetCompute(true);
    ResourceLayout writeResources;
    writeResources.VisibleStages = computeStage;
    writeResources.Bindings = { { .Type = DescriptorType::StorageImage, .Slot = 0, .Set = TestSetIndex, .Count = 1 } };
    WriteLayout = PipelineLayout::Create(writeResources, { { .Size = sizeof(TestConstants), .VisibleStages = computeStage } });

    ShaderStage computeShader = LoadShader("cs_compute_test");
    WritePipeline = Pipeline::CreateCompute(WriteLayout, computeShader);
    ReleaseShader(computeShader);

    ShaderStageMask fragmentStage(0);
    fragmentStage.SetFragment(true);
    ResourceLayout showResources;
    showResources.VisibleStages = fragmentStage;
    showResources.Bindings = { { .Type = DescriptorType::SampledImage, .Slot = 0, .Set = TestSetIndex, .Count = 1 } };
    ShowLayout = PipelineLayout::Create(showResources, {});

    StencilOpState keep = { CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep };
    PipelineDesc show;
    show.Layout = ShowLayout;
    show.VertexShader = LoadShader("vs_lighting");
    show.FragmentShader = LoadShader("ps_compute_test");
    show.RasterizerState = { FillMode::Solid, CullMode::None, false, 0.0f, 0.0f, 0.0f, true };
    show.DepthStencilState = { false, false, CompareOp::Always, false, 0.0f, 1.0f, false, 0xFF, 0xFF, keep, keep };
    show.BlendAttachmentStates = { RHIConstants::DisabledBlendAttachmentState };
    show.RenderTargetFormats = { Renderer::GetBackBufferFormat() };
    ShowPipeline = Pipeline::Create(show);
    ReleaseShader(show.VertexShader);
    ReleaseShader(show.FragmentShader);

    PassGraph& graph = Renderer::GetPassGraph();
    TestImage = graph.CreateImage({ .Name = "ComputeTest", .Format = Format::R8G8B8A8_UNORM, .Size = GraphImageSize::Fixed,
        .Width = TestSize, .Height = TestSize, .IsStorage = true });
    Readback::Watch(TestImage);

    PassDesc write;
    write.Name = "Compute test";
    write.Kind = PassKind::Compute;
    write.StorageWrites = { { TestImage, 0 } };
    write.InputLayout = WriteLayout;
    write.InputSetIndex = TestSetIndex;
    write.Execute = ExecuteWrite;
    WritePass = graph.AddPass(std::move(write));

    PassDesc showPass;
    showPass.Name = "Compute test view";
    showPass.ColorWrites = { { PassGraph::BackBuffer, AttachmentLoadOp::Load } };
    showPass.SampledReads = { { TestImage, 0 } };
    showPass.InputLayout = ShowLayout;
    showPass.InputSetIndex = TestSetIndex;
    showPass.Execute = ExecuteShow;
    ShowPass = graph.AddPass(std::move(showPass));

    MatchCount = 0;
    MinimumLatency = UINT64_MAX;
    MaximumLatency = 0;
    FirstMismatch.clear();
    IsEnlisted = true;
}

void ComputeTestPass::Retire()
{
    if (!IsEnlisted)
        return;

    if (!FirstMismatch.empty())
        std::cout << "Compute test: mismatch at " << FirstMismatch << " after " << MatchCount << " matching readbacks" << std::endl;
    else if (MatchCount == 0)
        std::cout << "Compute test: no readbacks completed" << std::endl;
    else
        std::cout << "Compute test: " << MatchCount << " readbacks matched, latency " << MinimumLatency << " to " << MaximumLatency << " frames" << std::endl;

    Renderer::WaitForGpu();
    PassGraph& graph = Renderer::GetPassGraph();
    graph.RemovePass(WritePass);
    graph.RemovePass(ShowPass);
    Readback::Unwatch(TestImage);
    graph.DestroyImage(TestImage);

    delete WritePipeline;
    delete ShowPipeline;
    delete WriteLayout;
    delete ShowLayout;
    WritePipeline = ShowPipeline = nullptr;
    WriteLayout = ShowLayout = nullptr;
    IsEnlisted = false;
}
