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
    backBuffer.IsImported = true;
    Images.push_back(backBuffer);
}

GraphImageID PassGraph::ClaimImageSlot()
{
    for (GraphImageID i = 1; i < Images.size(); i++)
        if (!Images[i].IsLive)
            return i;
    Images.emplace_back();
    return static_cast<GraphImageID>(Images.size() - 1);
}

GraphImageID PassGraph::CreateImage(const GraphImageDesc& desc)
{
    GraphImageID id = ClaimImageSlot();
    ImageEntry& entry = Images[id];
    entry = {};
    entry.Desc = desc;
    entry.IsLive = true;
    if (desc.Size == GraphImageSize::Fixed || ViewportWidth > 0)
        CreateImage(entry);
    return id;
}

GraphImageID PassGraph::ImportImage(ImageHandle image, ImageLayout restingLayout)
{
    GraphImageID id = ClaimImageSlot();
    ImageEntry& entry = Images[id];
    entry = {};
    entry.Desc.Format = BufferAllocator::GetInstance()->GetImageAllocation(image).Desc.Format;
    entry.Image = image;
    entry.RestingLayout = restingLayout;
    entry.Layout = restingLayout;
    entry.IsLive = true;
    entry.IsImported = true;
    return id;
}

void PassGraph::CreateImage(ImageEntry& entry)
{
    bool isDepth = IsDepthFormat(entry.Desc.Format);

    ImageDesc desc;
    desc.Width = entry.Desc.Size == GraphImageSize::Viewport ? ViewportWidth : entry.Desc.Width;
    desc.Height = entry.Desc.Size == GraphImageSize::Viewport ? ViewportHeight : entry.Desc.Height;
    desc.ArrayLayers = entry.Desc.ArrayLayers;
    desc.MipLevels = entry.Desc.MipLevels;
    desc.Format = entry.Desc.Format;
    desc.Usage.Sampled = true;
    desc.Usage.RenderTarget = !isDepth;
    desc.Usage.DepthStencil = isDepth;
    desc.Usage.Storage = entry.Desc.IsStorage;
    desc.Usage.TransferSource = true;
    desc.ClearColour = entry.Desc.ClearColour;
    desc.ClearDepth = entry.Desc.ClearDepth;

    entry.Image = BufferAllocator::GetInstance()->CreateImage(desc);
    entry.Layout = ImageLayout::Undefined;
}

void PassGraph::DestroyImage(GraphImageID image)
{
    if (image == BackBuffer || image >= Images.size() || !Images[image].IsLive)
        return;

    if (Images[image].Image.IsValid() && !Images[image].IsImported)
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

static bool IsWriting(const PassDesc& pass, GraphImageID image)
{
    for (const PassAttachment& attachment : pass.ColorWrites)
        if (attachment.Image == image)
            return true;
    for (const PassInput& storage : pass.StorageWrites)
        if (storage.Image == image)
            return true;
    return pass.HasDepthWrite && pass.DepthWrite.Image == image;
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

    for (uint32_t i = 0; i < passCount; i++)
    {
        const PassDesc& pass = Passes[i].Desc;
        if (pass.Kind == PassKind::Overlay)
        {
            for (uint32_t j = 0; j < passCount; j++)
                if (Passes[j].Desc.Kind != PassKind::Overlay)
                    addEdge(j, i);
            continue;
        }

        for (uint32_t j = 0; j < passCount; j++)
        {
            const PassDesc& other = Passes[j].Desc;
            if (other.Kind == PassKind::Overlay)
                continue;

            for (const PassInput& input : pass.SampledReads)
                if (IsWriting(other, input.Image))
                    addEdge(j, i);

            if (j >= i)
                continue;
            for (const PassAttachment& attachment : pass.ColorWrites)
                if (IsWriting(other, attachment.Image))
                    addEdge(j, i);
            for (const PassInput& storage : pass.StorageWrites)
                if (IsWriting(other, storage.Image))
                    addEdge(j, i);
            if (pass.HasDepthWrite && IsWriting(other, pass.DepthWrite.Image))
                addEdge(j, i);
        }
    }

    // Overlays among themselves follow Order, then declaration.
    std::vector<uint32_t> overlays;
    for (uint32_t i = 0; i < passCount; i++)
        if (Passes[i].Desc.Kind == PassKind::Overlay)
            overlays.push_back(i);
    std::stable_sort(overlays.begin(), overlays.end(),
        [&](uint32_t a, uint32_t b) { return Passes[a].Desc.Order < Passes[b].Desc.Order; });
    for (size_t k = 1; k < overlays.size(); k++)
        addEdge(overlays[k - 1], overlays[k]);

    ExecutionOrder.clear();
    std::vector<bool> isScheduled(passCount, false);
    while (ExecutionOrder.size() < passCount)
    {
        uint32_t next = UINT32_MAX;
        for (uint32_t i = 0; i < passCount; i++)
            if (!isScheduled[i] && dependencyCount[i] == 0 && (next == UINT32_MAX || Passes[i].Desc.Order < Passes[next].Desc.Order))
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
        bindings.push_back(DescriptorSetBinding::ForImage(input.Binding, Images[input.Image].Image, input.Range));
    for (const PassInput& storage : pass.Desc.StorageWrites)
    {
        SubresourceRange range = storage.Range;
        if (range.MipCount == 0)
            range.MipCount = 1;
        bindings.push_back(DescriptorSetBinding::ForImage(storage.Binding, Images[storage.Image].Image, range));
    }
    return bindings;
}

void PassGraph::AllocateInputSet(PassEntry& pass)
{
    bool hasInputs = !pass.Desc.SampledReads.empty() || !pass.Desc.StorageWrites.empty();
    if (pass.InputSet.IsValid() || !hasInputs || !pass.Desc.InputLayout)
        return;

    pass.InputSet = BufferAllocator::GetInstance()->AllocateDescriptorSet(
        pass.Desc.InputLayout->GetID(), pass.Desc.InputSetIndex, InputBindings(pass));
}

void PassGraph::Transition(GraphImageID image, ImageLayout layout, std::vector<ImageTransition>& out)
{
    ImageEntry& entry = Images[image];
    if (entry.Layout == layout && layout != ImageLayout::Storage)
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
        if (Images[i].IsLive && !Images[i].IsImported && !Images[i].Image.IsValid())
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
        for (const PassInput& storage : desc.StorageWrites)
            Transition(storage.Image, ImageLayout::Storage, transitions);
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
            uint32_t timer = executor->BeginTimer(desc.Name);
            executor->BeginOverlayPass(width, height);
            desc.Execute(context);
            executor->EndOverlayPass();
            executor->EndTimer(timer);
            continue;
        }

        if (desc.Kind == PassKind::Compute)
        {
            if (!desc.StorageWrites.empty())
            {
                const ImageDesc& sizeDesc = allocator->GetImageAllocation(Images[desc.StorageWrites[0].Image].Image).Desc;
                context.Width = sizeDesc.Width;
                context.Height = sizeDesc.Height;
            }
            uint32_t timer = executor->BeginTimer(desc.Name);
            desc.Execute(context);
            executor->EndTimer(timer);
            continue;
        }

        GraphImageID sizeSource = !desc.ColorWrites.empty() ? desc.ColorWrites[0].Image : desc.DepthWrite.Image;
        const ImageDesc& sizeDesc = allocator->GetImageAllocation(Images[sizeSource].Image).Desc;

        RenderingDesc rendering;
        rendering.Width = sizeDesc.Width;
        rendering.Height = sizeDesc.Height;
        for (const PassAttachment& attachment : desc.ColorWrites)
        {
            ColorAttachment color = { Images[attachment.Image].Image, attachment.LoadOp };
            color.Range = attachment.Range;
            rendering.ColorAttachments.push_back(color);
            isBackBufferWritten |= attachment.Image == BackBuffer;
        }
        if (desc.HasDepthWrite)
            rendering.Depth = { Images[desc.DepthWrite.Image].Image, desc.DepthWrite.LoadOp, desc.DepthWrite.Range };

        context.Width = rendering.Width;
        context.Height = rendering.Height;
        uint32_t timer = executor->BeginTimer(desc.Name);
        executor->BeginRendering(rendering);
        desc.Execute(context);
        executor->EndRendering();
        executor->EndTimer(timer);
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
    for (GraphImageID i = 1; i < Images.size(); i++)
        if (Images[i].IsLive && Images[i].IsImported && Images[i].Layout != Images[i].RestingLayout)
            Transition(i, Images[i].RestingLayout, transitions);
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
        if (!entry.IsLive || entry.IsImported || entry.Desc.Size != GraphImageSize::Viewport || !entry.Image.IsValid())
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
