#pragma once
#include <map>

#include "RHIStructures.h"
#include "../RHI_API_Macro.h"

using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

class Pipeline
{
public:
    std::vector<Pipeline*> PipelineVariants = {};
    DirectX::XMUINT2 ViewportSize = {0, 0};
    bool AttachmentsAreViewportDims = false;
    
protected:
    std::vector<uint64_t> PipelineInputDescriptorSetIDs;
    std::vector<IOResource*> InputIOResources;
    IOResource* PipelineOutputResource = nullptr;
    std::vector<DirectX::XMFLOAT4> ClearColors;
    float DepthClearValue = 1;
    uint32_t PushConstantCount = 0;
    std::map<uint32_t, uint32_t> SetIndexToBuilderIndex;
    bool IsVariant = false;
    uint32_t ViewMask = 0;
    uint32_t ArrayLayerCount = 1;
    
    // For recreating attachments
    std::vector<Format> RenderTargetFormats;
    std::vector<SamplerType> AttachmentSamplers;
    Format DepthStencilFormat = Format::Unknown;
    uint32_t OutputDescriptorSetIndex = 0;
    bool CreateOwnAttachments = false;
    bool CreateDepthImage = false;
    bool CreateDepthAttachment = false;
    MultisampleState MultisampleStateInfo;

public:
    static RHI_API Pipeline* Create(const PipelineDesc& desc, std::vector<IOResource*>* inputIOResources = nullptr);
    virtual ~Pipeline() = default;
    virtual void RefreshInputDescriptorSets() {}
    std::vector<DirectX::XMFLOAT4> GetClearColors() const { return ClearColors; }
    float GetDepthClearValue() const { return DepthClearValue; }
    std::vector<uint64_t> GetInputDescriptorSetIDs() const { return PipelineInputDescriptorSetIDs; }
    IOResource* GetOutputResource() const { return PipelineOutputResource; }
    uint32_t GetPushConstantCount() const { return PushConstantCount; }
    virtual size_t GetOwnedImageCount() const = 0;
    virtual void* GetOwnedImage(uint32_t index) = 0;
    virtual void* GetOwnedDepthImage() = 0;
    virtual void* GetOwnedImageView(uint32_t index) = 0;
    virtual void* GetOwnedDepthImageView() = 0;
    size_t GetPipelineVariantCount() const { return PipelineVariants.size(); }
    const std::map<uint32_t, uint32_t>& GetSetToRootParamMapping() const { return SetIndexToBuilderIndex; }
    uint32_t GetViewMask() const { return ViewMask; }
    uint32_t GetArrayLayerCount() const { return ArrayLayerCount; }
    bool GetIsVariant() { return IsVariant; }
    virtual void RecreateAttachments(uint32_t width, uint32_t uint32) = 0;
};

class D3DPipeline : public Pipeline
{
public:
    
    D3DPipeline(const PipelineDesc& desc, std::vector<IOResource*>* inputIOResources = nullptr);
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
    void* GetOwnedImageView(uint32_t index) override 
    { 
        return reinterpret_cast<void*>(OwnedRTVs[index].ptr);
    }
    void* GetOwnedDepthImageView() override
    {
        return reinterpret_cast<void*>(OwnedDSV.ptr);
    }

    void RefreshInputDescriptorSets() override;

private:

    std::vector<ComPtr<ID3D12Resource>> OwnedColorResources;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> OwnedRTVs;
    ComPtr<ID3D12Resource> OwnedDepthResource;
    D3D12_CPU_DESCRIPTOR_HANDLE OwnedDSV = {};
    std::vector<uint64_t> OwnedColorResourceIDs;
    uint64_t OwnedDepthImageResourceID = UINT64_MAX;

    D3D12_PRIMITIVE_TOPOLOGY Topology;
    ComPtr<ID3D12RootSignature> RootSignature;
    ComPtr<ID3D12PipelineState> PipelineState;

    void RecreateAttachments(uint32_t width, uint32_t height) override;
    

};

class VulkanPipeline : public Pipeline
{
public:
    
    VulkanPipeline(const PipelineDesc& desc, std::vector<IOResource*>* inputIOResources = nullptr);
    ~VulkanPipeline() override;
    void RefreshInputDescriptorSets() override;
    
    VkPipeline GetVulkanPipeline() const { return Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return PipelineLayout; }
    
    void* GetOwnedImage(uint32_t index) override { return OwnedImages[index]; }
    size_t GetOwnedImageCount() const override { return OwnedImages.size(); }
    std::vector<VkImage> GetOwnedImages() const { return OwnedImages; }
    std::vector<VkImageView> GetOwnedVkImageViews() const { return OwnedImageViews; }
    std::vector<VkDeviceMemory> GetOwnedVkImageMemory() const { return OwnedImageMemory; }
    VkImage GetOwnedDepthImage() const { return OwnedDepthImage; }
    VkDeviceMemory GetOwnedDepthVkImageMemory() const { return OwnedDepthImageMemory; }
    VkImageView GetOwnedDepthVkImageView() const { return OwnedDepthImageView; }
    void* GetOwnedDepthImage() override { return OwnedDepthImage; }
    void* GetOwnedImageView(uint32_t index) override { return reinterpret_cast<void*>(OwnedImageViews[index]); }
    void* GetOwnedDepthImageView() override { return reinterpret_cast<void*>(OwnedDepthImageView); }
    std::vector<VkAttachmentDescription> GetAttachmentDescriptions() const { return AttachmentDescriptions; }
    VkAttachmentDescription GetDepthAttachmentDescription() const { return DepthAttachmentDescription; }

private:
    std::vector<VkImage> OwnedImages;
    std::vector<VkImageView> OwnedImageViews;
    std::vector<VkDeviceMemory> OwnedImageMemory;
    std::vector<uint64_t> OwnedColorResourceIDs;
    VkImage OwnedDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory OwnedDepthImageMemory = VK_NULL_HANDLE;
    VkImageView OwnedDepthImageView = VK_NULL_HANDLE;
    uint64_t OwnedDepthImageResourceID = UINT64_MAX;
    
    std::vector<VkAttachmentDescription> AttachmentDescriptions;
    VkAttachmentDescription DepthAttachmentDescription;
    std::vector<VkShaderModule> ShaderModules;
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> SetLayouts;
    VkPipelineCache PipelineCache = VK_NULL_HANDLE;
    VkRenderPass RenderPass = VK_NULL_HANDLE;
    
    void RecreateAttachments(uint32_t width, uint32_t height) override;
    void CreateColorAttachments(uint32_t arrayLayers);
    void DestroyColorAttachments();
    void ReallocDepthImage();
    void DestroyDepthImage();
};

