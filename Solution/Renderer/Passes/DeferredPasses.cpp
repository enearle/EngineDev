#include "DeferredPasses.h"

#include <array>
#include "../DrawList.h"
#include "../MaterialRegistry.h"
#include "../PassGraph.h"
#include "../Renderer.h"
#include "../ShaderLibrary.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/RHIConstants.h"

using namespace RHIStructures;
using namespace RHIConstants;

static constexpr uint32_t ShadowMapSize = 1024;
static constexpr uint32_t ShadowViewMask = 0xF;
static constexpr uint32_t LightingInputSetIndex = 3;

using LayoutPipelines = std::array<Pipeline*, static_cast<size_t>(VertexLayout::Count)>;

static bool IsEnlisted = false;
static GraphImageID ShadowMap = 0;
static GraphImageID ShadowDepth = 0;
static GraphImageID Albedo = 0;
static GraphImageID Normal = 0;
static GraphImageID Material = 0;
static GraphImageID Depth = 0;
static std::vector<PassID> Passes;

static LayoutPipelines ShadowPipelines = {};
static LayoutPipelines GBufferPipelines = {};
static Pipeline* LightingPipeline = nullptr;
static PipelineLayout* LightingLayout = nullptr;

static DrawList ShadowDraws;
static DrawList GBufferDraws;

static std::vector<VertexAttribute> StaticAttributes(bool isPositionOnly)
{
    std::vector<VertexAttribute> attributes = {
        { .Binding = 0, .Location = 0, .Format = Format::R32G32B32_FLOAT, .Offset = 0, .SemanticName = SemanticName::Position, .SemanticIndex = 0 },
    };
    if (isPositionOnly)
        return attributes;

    attributes.push_back({ .Binding = 0, .Location = 1, .Format = Format::R32G32B32_FLOAT, .Offset = 12, .SemanticName = SemanticName::Normal, .SemanticIndex = 0 });
    attributes.push_back({ .Binding = 0, .Location = 2, .Format = Format::R32G32B32_FLOAT, .Offset = 24, .SemanticName = SemanticName::Tangent, .SemanticIndex = 0 });
    attributes.push_back({ .Binding = 0, .Location = 3, .Format = Format::R32G32B32_FLOAT, .Offset = 36, .SemanticName = SemanticName::Binormal, .SemanticIndex = 0 });
    attributes.push_back({ .Binding = 0, .Location = 4, .Format = Format::R32G32_FLOAT, .Offset = 48, .SemanticName = SemanticName::TexCoord, .SemanticIndex = 0 });
    return attributes;
}

static std::vector<VertexAttribute> SkinnedAttributes(bool isPositionOnly)
{
    std::vector<VertexAttribute> attributes = StaticAttributes(isPositionOnly);
    uint32_t location = static_cast<uint32_t>(attributes.size());
    attributes.push_back({ .Binding = 0, .Location = location, .Format = Format::R32G32B32A32_FLOAT, .Offset = 56, .SemanticName = SemanticName::BlendWeight, .SemanticIndex = 0 });
    attributes.push_back({ .Binding = 0, .Location = location + 1, .Format = Format::R32G32B32A32_UINT, .Offset = 72, .SemanticName = SemanticName::BlendIndices, .SemanticIndex = 0 });
    return attributes;
}

static DepthStencilState DepthState(bool isEnabled)
{
    StencilOpState keep = { CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep };
    return { isEnabled, isEnabled, isEnabled ? CompareOp::LessEqual : CompareOp::Always, false, 0.0f, 1.0f, false, 0xFF, 0xFF, keep, keep };
}

static RasterizerState Rasterizer(CullMode cullMode)
{
    return { FillMode::Solid, cullMode, false, 0.0f, 0.0f, 0.0f, true };
}

static LayoutPipelines CreateMeshPipelines(PipelineDesc desc, const char* staticShader, const char* skinnedShader, bool isPositionOnly)
{
    LayoutPipelines pipelines = {};

    desc.VertexShader = LoadShader(staticShader);
    desc.VertexBindings = { { 0, sizeof(Vertex), false } };
    desc.VertexAttributes = StaticAttributes(isPositionOnly);
    pipelines[static_cast<size_t>(VertexLayout::Static)] = Pipeline::Create(desc);
    ReleaseShader(desc.VertexShader);

    desc.VertexShader = LoadShader(skinnedShader);
    desc.VertexBindings = { { 0, sizeof(SkinnedVertex), false } };
    desc.VertexAttributes = SkinnedAttributes(isPositionOnly);
    pipelines[static_cast<size_t>(VertexLayout::Skinned)] = Pipeline::Create(desc);
    ReleaseShader(desc.VertexShader);
    ReleaseShader(desc.FragmentShader);

    return pipelines;
}

static void CreatePipelines()
{
    PipelineDesc shadow;
    shadow.Layout = Renderer::GetSceneLayout();
    shadow.FragmentShader = LoadShader("ps_vsm");
    shadow.RasterizerState = Rasterizer(CullMode::Back);
    shadow.DepthStencilState = DepthState(true);
    shadow.BlendAttachmentStates = { DisabledBlendAttachmentState };
    shadow.RenderTargetFormats = { Format::R32G32_FLOAT };
    shadow.DepthStencilFormat = Format::D32_FLOAT;
    shadow.ViewMask = ShadowViewMask;
    ShadowPipelines = CreateMeshPipelines(shadow, "vs_vsm", "vs_vsm_skinned", true);

    PipelineDesc gBuffer;
    gBuffer.Layout = Renderer::GetSceneLayout();
    gBuffer.FragmentShader = LoadShader("ps_pbr");
    gBuffer.RasterizerState = Rasterizer(CullMode::Back);
    gBuffer.DepthStencilState = DepthState(true);
    gBuffer.BlendAttachmentStates = { DisabledBlendAttachmentState, DisabledBlendAttachmentState, DisabledBlendAttachmentState };
    gBuffer.RenderTargetFormats = { Format::R8G8B8A8_UNORM, Format::R16G16_UNORM, Format::R8G8B8A8_UNORM };
    gBuffer.DepthStencilFormat = Format::D32_FLOAT;
    GBufferPipelines = CreateMeshPipelines(gBuffer, "vs_pbr", "vs_pbr_skinned", false);

    ResourceLayout lightingResources;
    lightingResources.VisibleStages = Renderer::GetSceneVisibleStages();
    lightingResources.Bindings = Renderer::GetSceneSetBindings();
    for (uint32_t slot = 0; slot < 5; slot++)
        lightingResources.Bindings.push_back({ .Type = DescriptorType::SampledImage, .Slot = slot, .Set = LightingInputSetIndex, .Count = 1 });
    LightingLayout = PipelineLayout::Create(lightingResources, {});

    PipelineDesc lighting;
    lighting.Layout = LightingLayout;
    lighting.VertexShader = LoadShader("vs_lighting");
    lighting.FragmentShader = LoadShader("ps_lighting");
    lighting.RasterizerState = Rasterizer(CullMode::None);
    lighting.DepthStencilState = DepthState(false);
    lighting.BlendAttachmentStates = { DisabledBlendAttachmentState };
    lighting.RenderTargetFormats = { Renderer::GetBackBufferFormat() };
    LightingPipeline = Pipeline::Create(lighting);
    ReleaseShader(lighting.VertexShader);
    ReleaseShader(lighting.FragmentShader);
}

static void ExecuteShadowPass(const PassContext& context)
{
    std::vector<Frustum> frusta;
    for (const RenderLight& light : Renderer::GetRenderScene().Lights)
        if (light.CastsShadows && light.Data.Type != 0 && frusta.size() < MaxLights)
            frusta.push_back(Frustum::FromViewProjection(light.ViewProjection));

    if (frusta.empty())
        return;

    ShadowDraws.Build(frusta, true);
    ShadowDraws.Record(context.Executor, ShadowPipelines, false, "Shadow");
}

static void ExecuteGBufferPass(const PassContext& context)
{
    const RenderScene& scene = Renderer::GetRenderScene();
    if (!scene.HasCamera)
        return;

    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMStoreFloat4x4(&viewProjection, DirectX::XMLoadFloat4x4(&scene.Camera.View) * DirectX::XMLoadFloat4x4(&scene.Camera.Projection));
    Frustum frustum = Frustum::FromViewProjection(viewProjection);

    GBufferDraws.Build({ &frustum, 1 }, false);
    GBufferDraws.Record(context.Executor, GBufferPipelines, true, "G-buffer");
}

static void ExecuteLightingPass(const PassContext& context)
{
    PipelineExecutor* executor = context.Executor;
    executor->BindPipeline(LightingPipeline);
    Renderer::BindSceneSet(executor);
    executor->BindDescriptorSet(context.InputSet);

    uint32_t x, y, width, height;
    Renderer::GetGameViewport(x, y, width, height);
    if (width > 0 && height > 0)
        executor->SetViewport(x, y, width, height);

    executor->Draw(6, 1);
    Renderer::ReportPassStats("Lighting", 0, 0, 1);
}

void DeferredPasses::Enlist()
{
    if (IsEnlisted)
        return;

    CreatePipelines();
    PassGraph& graph = Renderer::GetPassGraph();

    ShadowMap = graph.CreateImage({ .Name = "ShadowMap", .Format = Format::R32G32_FLOAT, .Size = GraphImageSize::Fixed,
        .Width = ShadowMapSize, .Height = ShadowMapSize, .ArrayLayers = MaxLights, .ClearColour = { 1.0f, 1.0f, 0.0f, 0.0f } });
    ShadowDepth = graph.CreateImage({ .Name = "ShadowDepth", .Format = Format::D32_FLOAT, .Size = GraphImageSize::Fixed,
        .Width = ShadowMapSize, .Height = ShadowMapSize, .ArrayLayers = MaxLights });
    Albedo = graph.CreateImage({ .Name = "Albedo", .Format = Format::R8G8B8A8_UNORM });
    Normal = graph.CreateImage({ .Name = "Normal", .Format = Format::R16G16_UNORM, .ClearColour = { 0.5f, 0.5f, 0.0f, 0.0f } });
    Material = graph.CreateImage({ .Name = "Material", .Format = Format::R8G8B8A8_UNORM });
    Depth = graph.CreateImage({ .Name = "Depth", .Format = Format::D32_FLOAT });

    PassDesc shadow;
    shadow.Name = "Shadow";
    shadow.ColorWrites = { { ShadowMap, AttachmentLoadOp::Clear } };
    shadow.HasDepthWrite = true;
    shadow.DepthWrite = { ShadowDepth, AttachmentLoadOp::Clear };
    shadow.ViewMask = ShadowViewMask;
    shadow.Execute = ExecuteShadowPass;

    PassDesc gBuffer;
    gBuffer.Name = "G-buffer";
    gBuffer.ColorWrites = { { Albedo, AttachmentLoadOp::Clear }, { Normal, AttachmentLoadOp::Clear }, { Material, AttachmentLoadOp::Clear } };
    gBuffer.HasDepthWrite = true;
    gBuffer.DepthWrite = { Depth, AttachmentLoadOp::Clear };
    gBuffer.Execute = ExecuteGBufferPass;

    PassDesc lighting;
    lighting.Name = "Lighting";
    lighting.ColorWrites = { { PassGraph::BackBuffer, AttachmentLoadOp::Clear } };
    lighting.SampledReads = { { ShadowMap, 0 }, { Albedo, 1 }, { Normal, 2 }, { Material, 3 }, { Depth, 4 } };
    lighting.InputLayout = LightingLayout;
    lighting.InputSetIndex = LightingInputSetIndex;
    lighting.Execute = ExecuteLightingPass;

    Passes = { graph.AddPass(std::move(lighting)), graph.AddPass(std::move(gBuffer)), graph.AddPass(std::move(shadow)) };
    IsEnlisted = true;
}

void DeferredPasses::Retire()
{
    if (!IsEnlisted)
        return;

    Renderer::WaitForGpu();
    PassGraph& graph = Renderer::GetPassGraph();
    for (PassID pass : Passes)
        graph.RemovePass(pass);
    Passes.clear();

    for (GraphImageID image : { ShadowMap, ShadowDepth, Albedo, Normal, Material, Depth })
        graph.DestroyImage(image);

    for (Pipeline*& pipeline : ShadowPipelines)
    {
        delete pipeline;
        pipeline = nullptr;
    }
    for (Pipeline*& pipeline : GBufferPipelines)
    {
        delete pipeline;
        pipeline = nullptr;
    }
    delete LightingPipeline;
    LightingPipeline = nullptr;
    delete LightingLayout;
    LightingLayout = nullptr;
    IsEnlisted = false;
}
