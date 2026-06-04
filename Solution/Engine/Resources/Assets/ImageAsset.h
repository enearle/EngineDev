#pragma once
#include <map>
#include "RHI/Image/ImageImport.h"
#include "../AssetBase.h"

class ImageAsset : GPUAssetBase
{
    static std::map<AssetID, uint64_t> LoadedImages;
    ImageData* CachedImage;
public:

    ImageAsset(std::string name, ImageData& imageData);
    
    virtual void Serialize(std::string& data) override;
    virtual void Deserialize(std::string& data, long& offset) override;
    
    virtual void UploadToGPU() override;
    virtual void FreeGPUResources() override;
    
};
