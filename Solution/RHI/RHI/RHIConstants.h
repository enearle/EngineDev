#pragma once
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

    static const std::vector<std::vector<uint8_t>> DefaultMetalnessRoughnessOcclusion =
    {
        { 0 },
        { 128 },
        { 0 }
    };

    inline constexpr ImageUsage DefaultUploadImageUsage
    {
        .Sampled = true,
        .TransferDestination = true
    };

    inline constexpr ImageDesc DefaultTextureDesc
    {
        .Width = 0,
        .Height = 0,
        .Size = 0,
        .Format = Format::R8G8B8A8_UNORM,
        .Usage = DefaultUploadImageUsage,
        .Access = DeviceLocalMemory,
        .Layout = ImageLayout::General,
        .InitialData = nullptr
    };

    inline constexpr BufferUsage DefaultDynamicBufferUsage
    {
        .Uniform = true,
        .TransferSource = true,
        .Access = UploadMemory,
    };

    inline constexpr BufferDesc DefaultDynamicBufferDesc
    {
        .Size = 0,
        .Usage = DefaultDynamicBufferUsage,
        .InitialData = nullptr
    };

    inline constexpr BufferUsage DefaultVertexBufferUsage
    {
        .Vertex = true,
        .TransferDestination = true,
        .Access = DeviceLocalMemory,
    };

    inline constexpr BufferDesc DefaultVertexBufferDesc
    {
        .Size = 0,
        .Usage = DefaultVertexBufferUsage,
        .InitialData = nullptr
    };

    inline constexpr BufferUsage DefaultIndexBufferUsage
    {
        .Index = true,
        .TransferDestination = true,
        .Access = DeviceLocalMemory,
    };

    inline constexpr BufferDesc DefaultIndexBufferDesc
    {
        .Size = 0,
        .Usage = DefaultIndexBufferUsage,
        .InitialData = nullptr
    };
};
