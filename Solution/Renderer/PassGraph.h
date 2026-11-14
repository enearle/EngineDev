#pragma once
#include <functional>
#include <string>
#include <vector>
#include "RENDERER_API_Macro.h"
#include "RHI/RHIStructures.h"

class PipelineExecutor;
class PipelineLayout;

using GraphImageID = uint32_t;
using PassID = uint32_t;

enum class GraphImageSize : uint8_t
{
    Viewport,       // Follows the window size
    Fixed
};

struct GraphImageDesc
{
    std::string Name;
    RHIStructures::Format Format = RHIStructures::Format::R8G8B8A8_UNORM;
    GraphImageSize Size = GraphImageSize::Viewport;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t ArrayLayers = 1;
    DirectX::XMFLOAT4 ClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
    float ClearDepth = 1.0f;
};

enum class PassKind : uint8_t
{
    Raster,
    Overlay         // Records into the back buffer through PipelineExecutor::BeginOverlayPass, after every raster pass
};

struct PassAttachment
{
    GraphImageID Image = 0;
    RHIStructures::AttachmentLoadOp LoadOp = RHIStructures::AttachmentLoadOp::Clear;
};

struct PassInput
{
    GraphImageID Image = 0;
    uint32_t Binding = 0;
};

struct PassContext
{
    PipelineExecutor* Executor = nullptr;
    uint32_t Width = 0;
    uint32_t Height = 0;
    RHIStructures::DescriptorSetHandle InputSet = {};
};

// A pass declares what it writes and samples. The graph orders passes, creates the images, places
// barriers, and allocates the descriptor set for the sampled inputs at InputSetIndex of InputLayout.
struct PassDesc
{
    std::string Name;
    PassKind Kind = PassKind::Raster;
    std::vector<PassAttachment> ColorWrites;
    bool HasDepthWrite = false;
    PassAttachment DepthWrite;
    std::vector<PassInput> SampledReads;
    PipelineLayout* InputLayout = nullptr;
    uint32_t InputSetIndex = 0;
    uint32_t ViewMask = 0;
    int Order = 0;                  // Orders overlay passes among themselves
    std::function<void(const PassContext&)> Execute;
};

// Retained: passes and images persist across frames, and the order is recomputed only when passes
// are added or removed. Scope is ordering, attachments, barriers and resize.
class RENDERER_API PassGraph
{
public:
    static constexpr GraphImageID BackBuffer = 0;

private:
    struct ImageEntry
    {
        GraphImageDesc Desc;
        RHIStructures::ImageHandle Image = {};
        RHIStructures::ImageLayout Layout = RHIStructures::ImageLayout::Undefined;
        bool IsLive = false;
    };

    struct PassEntry
    {
        PassID ID = 0;
        PassDesc Desc;
        RHIStructures::DescriptorSetHandle InputSet = {};
    };

    std::vector<ImageEntry> Images;
    std::vector<PassEntry> Passes;
    std::vector<uint32_t> ExecutionOrder;
    PassID NextPassID = 0;
    bool IsOrderDirty = true;
    uint32_t ViewportWidth = 0;
    uint32_t ViewportHeight = 0;

    void SortPasses();
    void CreateImage(ImageEntry& entry);
    void AllocateInputSet(PassEntry& pass);
    std::vector<RHIStructures::DescriptorSetBinding> InputBindings(const PassEntry& pass) const;
    void Transition(GraphImageID image, RHIStructures::ImageLayout layout, std::vector<RHIStructures::ImageTransition>& out);

public:
    PassGraph();

    GraphImageID CreateImage(const GraphImageDesc& desc);
    void DestroyImage(GraphImageID image);
    RHIStructures::ImageHandle GetImage(GraphImageID image) const;

    PassID AddPass(PassDesc desc);
    void RemovePass(PassID pass);

    void Execute(PipelineExecutor* executor, uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);
    void Clear();
};
