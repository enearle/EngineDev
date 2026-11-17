#include "DeferredPasses.h"

#include <array>
#include <string>
#include "../DrawList.h"
#include "../MaterialRegistry.h"
#include "../MaterialType.h"
#include "../PassGraph.h"
#include "../Renderer.h"
#include "../ShaderLibrary.h"
#include "../Skinning.h"
#include "RHI/Pipeline.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/RHIConstants.h"

using namespace RHIStructures;
using namespace RHIConstants;

static constexpr uint32_t ShadowMapSize = 1024;
static constexpr uint32_t LightingInputSetIndex = 3;

static bool IsEnlisted = false;
static GraphImageID ShadowMap = 0;
static GraphImageID ShadowDepth = 0;
static GraphImageID Albedo = 0;
static GraphImageID Normal = 0;
static GraphImageID Material = 0;
static GraphImageID Depth = 0;
static std::vector<PassID> Passes;

static Pipeline* ShadowPipeline = nullptr;
static std::vector<Pipeline*> GBufferPipelines;     // One per material type
static Pipeline* LightingPipeline = nullptr;
static PipelineLayout* LightingLayout = nullptr;

static DrawList ShadowDraws[MaxLights];
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
    attributes.push_back({ .Binding = 0, .Location = 5, .Format = Format::R32G32_FLOAT, .Offset = 56, .SemanticName = SemanticName::TexCoord, .SemanticIndex = 1 });
    attributes.push_back({ .Binding = 0, .Location = 6, .Format = Format::R32G32_FLOAT, .Offset = 64, .SemanticName = SemanticName::TexCoord, .SemanticIndex = 2 });
    return attributes;
}

static DepthStencilState DepthState(bool isEnabled, CompareOp compare = CompareOp::LessEqual)
{
    StencilOpState keep = { CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep };
    return { isEnabled, isEnabled, isEnabled ? compare : CompareOp::Always, false, 0.0f, 1.0f, false, 0xFF, 0xFF, keep, keep };
}

static RasterizerState Rasterizer(CullMode cullMode)
{
    return { FillMode::Solid, cullMode, false, 0.0f, 0.0f, 0.0f, true };
}

// Every mesh is drawn with the static Vertex layout; skinned ones from the skinning pool.
static Pipeline* CreateMeshPipeline(PipelineDesc desc, const char* vertexShader, bool isPositionOnly)
{
    desc.VertexShader = LoadShader(vertexShader);
    desc.VertexBindings = { { 0, sizeof(Vertex), false } };
    desc.VertexAttributes = StaticAttributes(isPositionOnly);
    Pipeline* pipeline = Pipeline::Create(desc);
    ReleaseShader(desc.VertexShader);
    ReleaseShader(desc.FragmentShader);
    return pipeline;
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
    ShadowPipeline = CreateMeshPipeline(shadow, "vs_vsm", true);

    for (MaterialTypeID type = 0; type < MaterialTypes::GetCount(); type++)
    {
        PipelineDesc gBuffer;
        gBuffer.Layout = Renderer::GetSceneLayout();
        gBuffer.FragmentShader = LoadShader("ps_gbuffer_" + MaterialTypes::Get(type).Name);
        gBuffer.RasterizerState = Rasterizer(CullMode::Back);
        gBuffer.DepthStencilState = DepthState(true, CompareOp::GreaterEqual);
        gBuffer.BlendAttachmentStates = { DisabledBlendAttachmentState, DisabledBlendAttachmentState, DisabledBlendAttachmentState };
        gBuffer.RenderTargetFormats = { Format::R8G8B8A8_UNORM, Format::R16G16_UNORM, Format::R8G8B8A8_UNORM };
        gBuffer.DepthStencilFormat = Format::D32_FLOAT;
        GBufferPipelines.push_back(CreateMeshPipeline(gBuffer, "vs_pbr", false));
    }

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

// One pass per shadow map layer, so each light gets its own culled draw list and no backend needs
// multiview. A layer whose light is missing or casts no shadow is only cleared.
static void ExecuteShadowPass(const PassContext& context, uint32_t layer)
{
    const std::vector<RenderLight>& lights = Renderer::GetRenderScene().Lights;
    if (layer >= lights.size() || !lights[layer].CastsShadows || lights[layer].Data.Type == 0)
        return;

    Frustum frustum = Frustum::FromViewProjection(lights[layer].ViewProjection);
    ShadowDraws[layer].Build({ &frustum, 1 }, true);
    ShadowDraws[layer].Record(context.Executor, { &ShadowPipeline, 1 }, false, "Shadow " + std::to_string(layer), layer);
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
    Depth = graph.CreateImage({ .Name = "Depth", .Format = Format::D32_FLOAT, .ClearDepth = 0.0f });


    PassDesc gBuffer;
    gBuffer.Name = "G-buffer";
    gBuffer.ColorWrites = { { Albedo, AttachmentLoadOp::Clear }, { Normal, AttachmentLoadOp::Clear }, { Material, AttachmentLoadOp::Clear } };
    gBuffer.HasDepthWrite = true;
    gBuffer.DepthWrite = { Depth, AttachmentLoadOp::Clear };
    gBuffer.BufferReads = { { Skinning::GetPool(), BufferState::VertexBuffer } };
    const SurfacePassResources& surfaces = Renderer::GetSurfacePassResources();
    gBuffer.SampledReads = surfaces.SampledReads;
    gBuffer.BufferWrites = surfaces.BufferWrites;
    gBuffer.Execute = ExecuteGBufferPass;

    PassDesc lighting;
    lighting.Name = "Lighting";
    lighting.ColorWrites = { { PassGraph::BackBuffer, AttachmentLoadOp::Clear } };
    lighting.SampledReads = { { ShadowMap, 0 }, { Albedo, 1 }, { Normal, 2 }, { Material, 3 }, { Depth, 4 } };
    lighting.InputLayout = LightingLayout;
    lighting.InputSetIndex = LightingInputSetIndex;
    lighting.Execute = ExecuteLightingPass;

    Passes = { graph.AddPass(std::move(lighting)), graph.AddPass(std::move(gBuffer)) };
    for (uint32_t layer = 0; layer < MaxLights; layer++)
    {
        PassDesc shadow;
        shadow.Name = "Shadow " + std::to_string(layer);
        shadow.ColorWrites = { { ShadowMap, AttachmentLoadOp::Clear, SubresourceRange::Layer(layer) } };
        shadow.HasDepthWrite = true;
        shadow.DepthWrite = { ShadowDepth, AttachmentLoadOp::Clear, SubresourceRange::Layer(layer) };
        shadow.BufferReads = { { Skinning::GetPool(), BufferState::VertexBuffer } };
        shadow.Execute = [layer](const PassContext& context) { ExecuteShadowPass(context, layer); };
        Passes.push_back(graph.AddPass(std::move(shadow)));
    }
    IsEnlisted = true;
}

GraphImageID DeferredPasses::GetAlbedoImage()
{
    return Albedo;
}

GraphImageID DeferredPasses::GetNormalImage()
{
    return Normal;
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

    delete ShadowPipeline;
    ShadowPipeline = nullptr;
    for (Pipeline* pipeline : GBufferPipelines)
        delete pipeline;
    GBufferPipelines.clear();
    delete LightingPipeline;
    LightingPipeline = nullptr;
    delete LightingLayout;
    LightingLayout = nullptr;
    IsEnlisted = false;
}
