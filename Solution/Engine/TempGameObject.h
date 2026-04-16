#pragma once
#include "EngineConstants.h"
#include "Common/RHI/Geometry/Mesh.h"

class TempGameObject
{
    
    std::vector<uint64_t> Materials;
    EngineConstants::ModelData ModelData;
    std::vector<EngineConstants::ModelData> ModelDataArray;
    RootNode MeshRoot;
    uint64_t geometryVPDescriptorSet;
    uint64_t lightingVPDescriptorSet;

public:
    
    TempGameObject(std::vector<std::string> materials, std::string filename, std::string name);
    void AddSceneNode(const SceneNode& node);
};
