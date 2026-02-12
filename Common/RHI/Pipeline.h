#pragma once
#include "RHIStructures.h"

using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

class Pipeline
{
    std::vector<uint64_t> PipelineInputDescriptorSetIDs;
public:
    static Pipeline* Create(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources, IOResource* outputLayout =nullptr);
    virtual ~Pipeline() = default;
};

class D3DPipeline : public Pipeline
{
public:
    
    D3DPipeline(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources, IOResource* outputLayout =nullptr);
    ID3D12PipelineState* GetPipelineState() const { return PipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return RootSignature.Get(); }
    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }
    
    std::vector<ComPtr<ID3D12Resource>> GetOwnedColorResources() const { return OwnedColorResources; }
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetOwnedRTVs() const { return OwnedRTVs; }
    ComPtr<ID3D12Resource> GetOwnedDepthResource() const { return OwnedDepthResource; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetOwnedDSV() const { return OwnedDSV; }
    
private:
    
    std::vector<ComPtr<ID3D12Resource>> OwnedColorResources;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> OwnedRTVs;
    ComPtr<ID3D12Resource> OwnedDepthResource;
    D3D12_CPU_DESCRIPTOR_HANDLE OwnedDSV = {};
    
    D3D12_PRIMITIVE_TOPOLOGY Topology;
    ComPtr<ID3D12RootSignature> RootSignature;
    ComPtr<ID3D12PipelineState> PipelineState;
};

class VulkanPipeline : public Pipeline
{
public:
    
    VulkanPipeline(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources, IOResource* outputLayout =nullptr);
    ~VulkanPipeline();
    
    VkPipeline GetVulkanPipeline() const { return Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return PipelineLayout; }
    VkRenderPass GetRenderPass() const { return RenderPass; }
    
    std::vector<VkImage> GetOwnedImages() const { return OwnedImages; }
    std::vector<VkImageView> GetOwnedImageViews() const { return OwnedImageViews; }
    std::vector<VkDeviceMemory> GetOwnedImageMemory() const { return OwnedImageMemory; }
    VkImage GetOwnedDepthImage() const { return OwnedDepthImage; }
    VkDeviceMemory GetOwnedDepthImageMemory() const { return OwnedDepthImageMemory; }
    VkImageView GetOwnedDepthImageView() const { return OwnedDepthImageView; }
    
    std::vector<VkAttachmentDescription> GetAttachmentDescriptions() const { return attachments; }

private:
    
    std::vector<VkImage> OwnedImages;
    std::vector<VkImageView> OwnedImageViews;
    std::vector<VkDeviceMemory> OwnedImageMemory;
    VkImage OwnedDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory OwnedDepthImageMemory = VK_NULL_HANDLE;
    VkImageView OwnedDepthImageView = VK_NULL_HANDLE;
    
    std::vector<VkAttachmentDescription> attachments;
    void CreateRenderPass(const PipelineDesc& desc);
    std::vector<VkShaderModule> ShaderModules;
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> SetLayouts;
    VkPipelineCache PipelineCache = VK_NULL_HANDLE;
    VkRenderPass RenderPass = VK_NULL_HANDLE;
};

