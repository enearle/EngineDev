#pragma once
#include <map>
#include "../AssetBase.h"

// Only one instance of material can be loaded at a time
// Instanced drawcalls are batched by material 


class MaterialAsset : public GPUAssetBase
{
    static std::map<AssetID, uint64_t> LoadedMaterials;
    bool CastsShadows = false;
    
public:
    void UploadToGPU() override;
    void FreeGPUResources() override;
};
