#include "PassGraph.h"

#include <algorithm>
#include <stdexcept>
#include "RHI/BufferAllocator.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/PipelineLayout.h"

using namespace RHIStructures;

PassGraph::PassGraph()
{
    ImageEntry backBuffer;
    backBuffer.Desc.Name = "BackBuffer";
    backBuffer.Desc.ClearColour = { 0.1f, 0.1f, 0.1f, 1.0f };
    backBuffer.IsLive = true;
    Images.push_back(backBuffer);
}

GraphImageID PassGraph::CreateImage(const GraphImageDesc& desc)
{
    GraphImageID id = 0;
    for (GraphImageID i = 1; i < Images.size() && id == 0; i++)
        if (!Images[i].IsLive)
            id = i;
    if (id == 0)
    {
        id = static_cast<GraphImageID>(Images.size());
        Images.emplace_back();
    }

    ImageEntry& entry = Images[id];
    entry = {};
    entry.Desc = desc;
    entry.IsLive = true;
    if (desc.Size == GraphImageSize::Fixed || ViewportWidth > 0)
        CreateImage(entry);
    return id;
}

void PassGraph::CreateImage(ImageEntry& entry)
{
    bool isDepth = IsDepthFormat(entry.Desc.Format);

    ImageDesc desc;
    desc.Width = entry.Desc.Size == GraphImageSize::Viewport ? ViewportWidth : entry.Desc.Width;
    desc.Height = entry.Desc.Size == GraphImageSize::Viewport ? ViewportHeight : entry.Desc.Height;
    desc.ArrayLayers = entry.Desc.ArrayLayers;
    desc.Format = entry.Desc.Format;
    desc.Usage.Sampled = true;
    desc.Usage.RenderTarget = !isDepth;
    desc.Usage.DepthStencil = isDepth;
    desc.ClearColour = entry.Desc.ClearColour;
    desc.ClearDepth = entry.Desc.ClearDepth;

    entry.Image = BufferAllocator::GetInstance()->CreateImage(desc);
    entry.Layout = ImageLayout::Undefined;
}

void PassGraph::DestroyImage(GraphImageID image)
{
    if (image == BackBuffer || image >= Images.size() || !Images[image].IsLive)
        return;

    if (Images[image].Image.IsValid())
        BufferAllocator::GetInstance()->DestroyImage(Images[image].Image);
    Images[image] = {};
}

ImageHandle PassGraph::GetImage(GraphImageID image) const
{
    return Images[image].Image;
}

PassID PassGraph::AddPass(PassDesc desc)
{
    PassEntry entry;
    entry.ID = NextPassID++;
    entry.Desc = std::move(desc);
    Passes.push_back(std::move(entry));
    IsOrderDirty = true;
    return Passes.back().ID;
}

void PassGraph::RemovePass(PassID pass)
{
    for (size_t i = 0; i < Passes.size(); i++)
        if (Passes[i].ID == pass)
        {
            if (Passes[i].InputSet.IsValid())
                BufferAllocator::GetInstance()->FreeDescriptorSet(Passes[i].InputSet);
            Passes.erase(Passes.begin() + i);
            IsOrderDirty = true;
            return;
        }
}

void PassGraph::SortPasses()
{
    size_t passCount = Passes.size();
    std::vector<std::vector<uint32_t>> dependents(passCount);
    std::vector<uint32_t> dependencyCount(passCount, 0);

    auto addEdge = [&](uint32_t before, uint32_t after)
    {
        if (before == after)
            return;
        dependents[before].push_back(after);
        dependencyCount[after]++;
    };

    auto writes = [](const PassDesc& pass, GraphImageID image)
    {
        for (const PassAttachment& attachment : pass.ColorWrites)
            if (attachment.Image == image)
                return true;
        return pass.HasDepthWrite && pass.DepthWrite.Image == image;
    };

    std::vector<uint32_t> overlays;
    for (uint32_t i = 0; i < passCount; i++)
    {
        const PassDesc& pass = Passes[i].Desc;
        if (pass.Kind == PassKind::Overlay)
        {
            overlays.push_back(i);
            continue;
        }

        for (uint32_t j = 0; j < passCount; j++)
        {
            const PassDesc& other = Passes[j].Desc;
            if (other.Kind == PassKind::Overlay)
                continue;

            for (const PassInput& input : pass.SampledReads)
                if (writes(other, input.Image))
                    addEdge(j, i);

            if (j < i)
                for (const PassAttachment& attachment : pass.ColorWrites)
                    if (writes(other, attachment.Image))
                        addEdge(j, i);
            if (j < i && pass.HasDepthWrite && writes(other, pass.DepthWrite.Image))
                addEdge(j, i);
        }
    }

    std::stable_sort(overlays.begin(), overlays.end(),
        [&](uint32_t a, uint32_t b) { return Passes[a].Desc.Order < Passes[b].Desc.Order; });
    for (size_t k = 0; k < overlays.size(); k++)
    {
        for (uint32_t i = 0; i < passCount; i++)
            if (Passes[i].Desc.Kind == PassKind::Raster)
                addEdge(i, overlays[k]);
        if (k > 0)
            addEdge(overlays[k - 1], overlays[k]);
    }

    ExecutionOrder.clear();
    std::vector<bool> isScheduled(passCount, false);
    while (ExecutionOrder.size() < passCount)
    {
        uint32_t next = UINT32_MAX;
        for (uint32_t i = 0; i < passCount && next == UINT32_MAX; i++)
            if (!isScheduled[i] && dependencyCount[i] == 0)
                next = i;

        if (next == UINT32_MAX)
            throw std::runtime_error("Pass graph has a cycle");

        isScheduled[next] = true;
        ExecutionOrder.push_back(next);
        for (uint32_t dependent : dependents[next])
            dependencyCount[dependent]--;
    }

    IsOrderDirty = false;
}

std::vector<DescriptorSetBinding> PassGraph::InputBindings(const PassEntry& pass) const
{
    std::vector<DescriptorSetBinding> bindings;
    for (const PassInput& input : pass.Desc.SampledReads)
        bindings.push_back(DescriptorSetBinding::ForImage(input.Binding, Images[input.Image].Image));
    return bindings;
}

void PassGraph::AllocateInputSet(PassEntry& pass)
{
    if (pass.InputSet.IsValid() || pass.Desc.SampledReads.empty() || !pass.Desc.InputLayout)
        return;

    pass.InputSet = BufferAllocator::GetInstance()->AllocateDescriptorSet(
        pass.Desc.InputLayout->GetID(), pass.Desc.InputSetIndex, InputBindings(pass));
}

void PassGraph::Transition(GraphImageID image, ImageLayout layout, std::vector<ImageTransition>& out)
{
    ImageEntry& entry = Images[image];
    if (entry.Layout == layout)
        return;
    out.push_back({ entry.Image, entry.Layout, layout });
    entry.Layout = layout;
}

void PassGraph::Execute(PipelineExecutor* executor, uint32_t width, uint32_t height)
{
    if (ViewportWidth == 0)
    {
        ViewportWidth = width;
        ViewportHeight = height;
    }

    for (GraphImageID i = 1; i < Images.size(); i++)
        if (Images[i].IsLive && !Images[i].Image.IsValid())
            CreateImage(Images[i]);

    if (IsOrderDirty)
        SortPasses();

    for (PassEntry& pass : Passes)
        AllocateInputSet(pass);

    BufferAllocator* allocator = BufferAllocator::GetInstance();
    Images[BackBuffer].Image = executor->GetBackBuffer();
    Images[BackBuffer].Layout = ImageLayout::Undefined;

    auto clearBackBuffer = [&]()
    {
        RenderingDesc clear;
        clear.ColorAttachments.push_back({ Images[BackBuffer].Image, AttachmentLoadOp::Clear, true, Images[BackBuffer].Desc.ClearColour });
        clear.Width = width;
        clear.Height = height;
        executor->BeginRendering(clear);
        executor->EndRendering();
    };

    bool isBackBufferWritten = false;
    std::vector<ImageTransition> transitions;
    for (uint32_t passIndex : ExecutionOrder)
    {
        PassEntry& pass = Passes[passIndex];
        const PassDesc& desc = pass.Desc;

        transitions.clear();
        for (const PassInput& input : desc.SampledReads)
            Transition(input.Image, ImageLayout::ShaderReadOnly, transitions);
        for (const PassAttachment& attachment : desc.ColorWrites)
            Transition(attachment.Image, ImageLayout::ColorAttachment, transitions);
        if (desc.HasDepthWrite)
            Transition(desc.DepthWrite.Image, ImageLayout::DepthStencilAttachment, transitions);
        if (desc.Kind == PassKind::Overlay)
            Transition(BackBuffer, ImageLayout::ColorAttachment, transitions);
        executor->TransitionImages(transitions);

        PassContext context;
        context.Executor = executor;
        context.InputSet = pass.InputSet;

        if (desc.Kind == PassKind::Overlay)
        {
            if (!isBackBufferWritten)
                clearBackBuffer();
            isBackBufferWritten = true;

            context.Width = width;
            context.Height = height;
            executor->BeginOverlayPass(width, height);
            desc.Execute(context);
            executor->EndOverlayPass();
            continue;
        }

        GraphImageID sizeSource = !desc.ColorWrites.empty() ? desc.ColorWrites[0].Image : desc.DepthWrite.Image;
        const ImageDesc& sizeDesc = allocator->GetImageAllocation(Images[sizeSource].Image).Desc;

        RenderingDesc rendering;
        rendering.Width = sizeDesc.Width;
        rendering.Height = sizeDesc.Height;
        rendering.ViewMask = desc.ViewMask;
        for (const PassAttachment& attachment : desc.ColorWrites)
        {
            rendering.ColorAttachments.push_back({ Images[attachment.Image].Image, attachment.LoadOp });
            isBackBufferWritten |= attachment.Image == BackBuffer;
        }
        if (desc.HasDepthWrite)
            rendering.Depth = { Images[desc.DepthWrite.Image].Image, desc.DepthWrite.LoadOp };

        context.Width = rendering.Width;
        context.Height = rendering.Height;
        executor->BeginRendering(rendering);
        desc.Execute(context);
        executor->EndRendering();
    }

    transitions.clear();
    if (!isBackBufferWritten)
    {
        Transition(BackBuffer, ImageLayout::ColorAttachment, transitions);
        executor->TransitionImages(transitions);
        clearBackBuffer();
        transitions.clear();
    }
    Transition(BackBuffer, ImageLayout::Present, transitions);
    executor->TransitionImages(transitions);
}

void PassGraph::Resize(uint32_t width, uint32_t height)
{
    ViewportWidth = width;
    ViewportHeight = height;

    BufferAllocator* allocator = BufferAllocator::GetInstance();
    for (GraphImageID i = 1; i < Images.size(); i++)
    {
        ImageEntry& entry = Images[i];
        if (!entry.IsLive || entry.Desc.Size != GraphImageSize::Viewport || !entry.Image.IsValid())
            continue;
        allocator->DestroyImage(entry.Image);
        CreateImage(entry);
    }

    for (PassEntry& pass : Passes)
        if (pass.InputSet.IsValid())
            allocator->UpdateDescriptorSet(pass.InputSet, InputBindings(pass));
}

void PassGraph::Clear()
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    for (PassEntry& pass : Passes)
        if (pass.InputSet.IsValid())
            allocator->FreeDescriptorSet(pass.InputSet);
    Passes.clear();
    ExecutionOrder.clear();

    for (GraphImageID i = 1; i < Images.size(); i++)
        DestroyImage(i);
    Images.resize(1);
    IsOrderDirty = true;
}
