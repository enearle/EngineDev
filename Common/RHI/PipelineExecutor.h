#pragma once
#include "Pipeline.h"
#include "../RHI/RHIStructures.h"
#include "Geometry/Mesh.h"

namespace DirectX { struct XMFLOAT4; }
class PipelineExecutor
{
public:
    static PipelineExecutor* Create();
    
    virtual ~PipelineExecutor() = default;
    
    // Begin a rendering operation with a pipeline and its attachments
    virtual void Begin(Pipeline* pipeline,
                      const std::vector<void*>& colorViews,
                      void* depthView,
                      uint32_t width, uint32_t height,
                      const std::vector<DirectX::XMFLOAT4>& clearColors,
                      float clearDepth) = 0;
    
    // End current rendering operation
    virtual void End() = 0;

    virtual void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) = 0;
    virtual void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) = 0;
    
    virtual void DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& perItemDrawSets, const DirectX::XMFLOAT4X4& viewProjX4, const DirectX::XMFLOAT4 camPos) = 0;
    virtual void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount, 
        void* pushConstants = nullptr, size_t pushConstantsSize = 0) = 0;
    virtual void DrawQuad(std::vector<uint64_t>* descriptorSets = nullptr) = 0;
};

class D3DPipelineExecutor : public PipelineExecutor
{
public:
    D3DPipelineExecutor();
    ~D3DPipelineExecutor() override;
    
    void Begin(Pipeline* pipeline,
               const std::vector<void*>& colorViews,
               void* depthView,
               uint32_t width, uint32_t height,
               const std::vector<DirectX::XMFLOAT4>& clearColors,
               float clearDepth) override;
    void End() override;
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;
    void DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& perItemDrawSets, const DirectX::XMFLOAT4X4& viewProjX4, const DirectX::XMFLOAT4 camPos) override;
    void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount, 
        void* pushConstant, size_t pushConstantSize) override;
    void DrawQuad(std::vector<uint64_t>* descriptorSets = nullptr) override;
    void BindDescriptorSets(std::vector<uint64_t>* descriptorSets, bool hasPushConstant = false);
    
private:
    ID3D12GraphicsCommandList* GetCommandList();
    D3DPipeline* CurrentPipeline;
};

class VulkanPipelineExecutor : public PipelineExecutor
{
public:
    VulkanPipelineExecutor();
    ~VulkanPipelineExecutor() override;
    
    void Begin(Pipeline* pipeline,
               const std::vector<void*>& colorViews,
               void* depthView,
               uint32_t width, uint32_t height,
               const std::vector<DirectX::XMFLOAT4>& clearColors,
               float clearDepth) override;
    void End() override;
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;
    void DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& perItemDrawSets, const DirectX::XMFLOAT4X4& viewProjX4, const DirectX::XMFLOAT4 camPos) override;
    void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount, 
        void* pushConstant, size_t pushConstantSize) override;
    void DrawQuad(std::vector<uint64_t>* descriptorSets = nullptr) override;
    void BindDescriptorSets(std::vector<uint64_t>* descriptorSets, bool hasPushConstant = false);

private:
    
    VulkanPipeline* CurrentPipeline;
    VkCommandBuffer GetCommandBuffer();
};
