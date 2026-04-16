#pragma once
#include <iostream>
#include "RHIStructures.h"

using namespace RHIStructures;

namespace RHIConstants
{
    inline constexpr BlendAttachmentState DisabledBlendAttachmentState {
        .ColorBlendOp = BlendOp::Add,
        .SrcColorBlendFactor = BlendFactor::SrcAlpha,
        .DestColorBlendFactor = BlendFactor::InvSrcAlpha,
        .AlphaBlendOp = BlendOp::Add,
        .SrcAlphaBlendFactor = BlendFactor::One,
        .DestAlphaBlendFactor = BlendFactor::Zero,
        .BlendEnable = false
    };
    
    inline constexpr ImageMemoryBarrier PRE_BARRIER{
        .SrcStage      = PipelineStage::TopOfPipe,
        .DstStage      = PipelineStage::ColorAttachmentOutput,
        .SrcAccessMask = 0u,
        .DstAccessMask = static_cast<uint32_t>(AccessFlag::ColorAttachmentWrite),
        .OldLayout     = ImageLayout::Present,
        .NewLayout     = ImageLayout::ColorAttachment,
    };

    inline constexpr ImageMemoryBarrier POST_BARRIER{
        .SrcStage      = PipelineStage::ColorAttachmentOutput,
        .DstStage      = PipelineStage::BottomOfPipe,
        .SrcAccessMask = static_cast<uint32_t>(AccessFlag::ColorAttachmentWrite),
        .DstAccessMask = 0u,
        .OldLayout     = ImageLayout::ColorAttachment,
        .NewLayout     = ImageLayout::Present,
    };

    inline constexpr ImageMemoryBarrier INIT_BARRIER{
        .SrcStage = PipelineStage::TopOfPipe,
        .DstStage = PipelineStage::FragmentShader,
        .SrcAccessMask = 0u,
        .DstAccessMask = static_cast<uint32_t>(AccessFlag::ShaderRead),
        .OldLayout = ImageLayout::Undefined,
        .NewLayout = ImageLayout::ShaderReadOnly,
    };
    
    inline constexpr ImageMemoryBarrier INIT_DEPTH_BARRIER{
        .SrcStage = PipelineStage::TopOfPipe,
        .DstStage = PipelineStage::EarlyFragmentTests,
        .SrcAccessMask = 0u,
        .DstAccessMask = static_cast<uint32_t>(AccessFlag::DepthStencilAttachmentWrite),
        .OldLayout = ImageLayout::Undefined,
        .NewLayout = ImageLayout::DepthStencilAttachment,
        .IsDepthImage = true
    };
    
    inline constexpr ImageMemoryBarrier ATTACHMENT_TO_READ_BARRIER{
        .SrcStage = PipelineStage::ColorAttachmentOutput,
        .DstStage = PipelineStage::FragmentShader,
        .SrcAccessMask = static_cast<uint32_t>(AccessFlag::ColorAttachmentWrite),
        .DstAccessMask = static_cast<uint32_t>(AccessFlag::ShaderRead),
        .OldLayout = ImageLayout::ColorAttachment,
        .NewLayout = ImageLayout::ShaderReadOnly,
    };
    
    inline constexpr ImageMemoryBarrier READ_TO_ATTACHMENT_BARRIER{
        .SrcStage = PipelineStage::FragmentShader,
        .DstStage = PipelineStage::ColorAttachmentOutput,
        .SrcAccessMask = static_cast<uint32_t>(AccessFlag::ShaderRead),
        .DstAccessMask = static_cast<uint32_t>(AccessFlag::ColorAttachmentWrite),
        .OldLayout = ImageLayout::ShaderReadOnly,
        .NewLayout = ImageLayout::ColorAttachment,
    };
    
    static const std::vector<std::vector<uint8_t>> DefaultMetalnessRoughnessOcclusion = 
    {
        { 0 },
        { 128 },
        { 0 }
    };
    
    inline constexpr ImageUsage DefaultUploadImageUsage
    {
        .TransferSource = false,
        .TransferDestination = true,
        .Type = ImageType::Sampled
    };
    
    inline constexpr ImageDesc DefaultTextureDesc 
    {
        .Width = 0,
        .Height = 0,
        .Size = 0,
        .Format = Format::R8G8B8A8_UNORM,
        .Usage = DefaultUploadImageUsage,
        .Type = ImageType::Sampled,
        .Access = MemoryAccess(8),
        .Layout = ImageLayout::General,
        .InitialData = nullptr
    };
    
    inline constexpr BufferUsage DefaultUniformBufferUsage
    {
        .TransferSource = true,
        .TransferDestination = false,
        .Type = BufferType::Constant,
    };
    
    inline constexpr BufferDesc DefaultConstantBufferDesc
    {
        .Size = 0,
        .Usage = DefaultUniformBufferUsage,
        .Type = BufferType::Constant,
        .Access = MemoryAccess(9),
        .InitialData = nullptr
    };
    
};
