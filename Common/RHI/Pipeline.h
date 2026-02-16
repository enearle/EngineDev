#pragma once
#include "RHIStructures.h"

using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

class Pipeline
{
protected:
    std::vector<uint64_t> PipelineInputDescriptorSetIDs;
    IOResource* PipelineOutputResource;
public:
    static Pipeline* Create(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources = nullptr);
    virtual ~Pipeline() = default;
    
    IOResource* GetOutputResource() const { return PipelineOutputResource; }
    virtual void* GetOwnedImage(uint32_t index) = 0;
};

class D3DPipeline : public Pipeline
{
public:
    
    D3DPipeline(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources = nullptr);
    ID3D12PipelineState* GetPipelineState() const { return PipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return RootSignature.Get(); }
    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }
    
    void* GetOwnedImage(uint32_t index) override { return OwnedColorResources[index].Get(); }
    
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
    
    VulkanPipeline(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources = nullptr);
    ~VulkanPipeline();
    
    VkPipeline GetVulkanPipeline() const { return Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return PipelineLayout; }
    VkRenderPass GetRenderPass() const { return RenderPass; }
    
    void* GetOwnedImage(uint32_t index) override { return OwnedImages[index]; }
    
    // Owned images are tracked in buffer allocator and do not need cleanup in pipeline
    std::vector<VkImage> GetOwnedImages() const { return OwnedImages; }
    std::vector<VkImageView> GetOwnedImageViews() const { return OwnedImageViews; }
    std::vector<VkDeviceMemory> GetOwnedImageMemory() const { return OwnedImageMemory; }
    VkImage GetOwnedDepthImage() const { return OwnedDepthImage; }
    VkDeviceMemory GetOwnedDepthImageMemory() const { return OwnedDepthImageMemory; }
    VkImageView GetOwnedDepthImageView() const { return OwnedDepthImageView; }
    std::vector<uint64_t> GetInputDescriptorSetIDs() const { return PipelineInputDescriptorSetIDs; }
    
    std::vector<VkAttachmentDescription> GetAttachmentDescriptions() const { return AttachmentDescriptions; }
    VkAttachmentDescription GetDepthAttachmentDescription() const { return DepthAttachmentDescription; }

private:
    
    std::vector<VkImage> OwnedImages;
    std::vector<VkImageView> OwnedImageViews;
    std::vector<VkDeviceMemory> OwnedImageMemory;
    VkImage OwnedDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory OwnedDepthImageMemory = VK_NULL_HANDLE;
    VkImageView OwnedDepthImageView = VK_NULL_HANDLE;
    
    std::vector<VkAttachmentDescription> AttachmentDescriptions;
    VkAttachmentDescription DepthAttachmentDescription;
    std::vector<VkShaderModule> ShaderModules;
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> SetLayouts;
    VkPipelineCache PipelineCache = VK_NULL_HANDLE;
    VkRenderPass RenderPass = VK_NULL_HANDLE;
};

