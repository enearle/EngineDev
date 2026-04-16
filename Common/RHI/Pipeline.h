#pragma once
#include "RHIStructures.h"
#include "../../Solution/RHI/RHI_API_Macro.h"

using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

class Pipeline
{
protected:
    std::vector<uint64_t> PipelineInputDescriptorSetIDs;
    IOResource* PipelineOutputResource = nullptr;
    std::vector<DirectX::XMFLOAT4> ClearColors;
    float DepthClearValue = 1;
    uint32_t PushConstantCount = 0;

public:
    static RHI_API Pipeline* Create(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources = nullptr);
    virtual ~Pipeline() = default;
    std::vector<DirectX::XMFLOAT4> GetClearColors() const { return ClearColors; }
    float GetDepthClearValue() const { return DepthClearValue; }
    std::vector<uint64_t> GetInputDescriptorSetIDs() const { return PipelineInputDescriptorSetIDs; }
    IOResource* GetOutputResource() const { return PipelineOutputResource; }
    uint32_t GetPushConstantCount() const { return PushConstantCount; }
    virtual size_t GetOwnedImageCount() const = 0;
    virtual void* GetOwnedImage(uint32_t index) = 0;
    virtual void* GetOwnedDepthImage() = 0;
};

class D3DPipeline : public Pipeline
{
public:
    
    D3DPipeline(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources = nullptr);
    ~D3DPipeline() override = default;
    
    ID3D12PipelineState* GetPipelineState() const { return PipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return RootSignature.Get(); }
    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }
    
    void* GetOwnedImage(uint32_t index) override { return OwnedColorResources[index].Get(); }
    size_t GetOwnedImageCount() const override { return OwnedColorResources.size(); }
    std::vector<ComPtr<ID3D12Resource>> GetOwnedColorResources() const { return OwnedColorResources; }
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetOwnedRTVs() const { return OwnedRTVs; }
    ComPtr<ID3D12Resource> GetOwnedDepthResource() const { return OwnedDepthResource; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetOwnedDSV() const { return OwnedDSV; }
    void* GetOwnedDepthImage() override { return OwnedDepthResource.Get(); }
    
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
    ~VulkanPipeline() override;
    
    VkPipeline GetVulkanPipeline() const { return Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return PipelineLayout; }
    
    void* GetOwnedImage(uint32_t index) override { return OwnedImages[index]; }
    size_t GetOwnedImageCount() const override { return OwnedImages.size(); }
    std::vector<VkImage> GetOwnedImages() const { return OwnedImages; }
    std::vector<VkImageView> GetOwnedImageViews() const { return OwnedImageViews; }
    std::vector<VkDeviceMemory> GetOwnedImageMemory() const { return OwnedImageMemory; }
    VkImage GetOwnedDepthImage() const { return OwnedDepthImage; }
    VkDeviceMemory GetOwnedDepthImageMemory() const { return OwnedDepthImageMemory; }
    VkImageView GetOwnedDepthImageView() const { return OwnedDepthImageView; }
    void* GetOwnedDepthImage() override { return OwnedDepthImage; }
    
    std::vector<VkAttachmentDescription> GetAttachmentDescriptions() const { return AttachmentDescriptions; }
    VkAttachmentDescription GetDepthAttachmentDescription() const { return DepthAttachmentDescription; }

private:
    // Owned images are tracked in buffer allocator and do not need cleanup in pipeline
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

