#include "ImageAsset.h"
#include "../AssetSerializer.h"

ImageAsset::ImageAsset(std::string name, ImageData& imageData) : GPUAssetBase(name, AssetID::Generate(), {})
{
    CachedImage = &imageData;
}

void ImageAsset::Serialize(std::string& data)
{
    GPUAssetBase::Serialize(data);
    
    AssetSerializer::Write<bool>(data, CachedImage->Is16Bit);
    AssetSerializer::Write<uint32_t>(data, CachedImage->Width);
    AssetSerializer::Write<uint32_t>(data, CachedImage->Height);
    AssetSerializer::Write<uint8_t>(data, CachedImage->Channels);
    AssetSerializer::Write<uint64_t>(data, CachedImage->TotalSize);
}

void ImageAsset::Deserialize(std::string& data, long& offset)
{
    GPUAssetBase::Deserialize(data, offset);
    
    CachedImage = new ImageData();
    CachedImage->Is16Bit = AssetSerializer::Read<bool>(data, offset);
    CachedImage->Width = AssetSerializer::Read<uint32_t>(data, offset);
    CachedImage->Height = AssetSerializer::Read<uint32_t>(data, offset);
    CachedImage->Channels = AssetSerializer::Read<uint8_t>(data, offset);
    CachedImage->TotalSize = AssetSerializer::Read<uint64_t>(data, offset);
}

void ImageAsset::UploadToGPU()
{
    ////////////////////////////////////////////////////////////////////////
    // TODO: 
    //      -   Finish Image upload and import
    //      -   Finish Material upload
    //      -   Finish converting RHI to use Material bin
    ////////////////////////////////////////////////////////////////////////
    delete CachedImage;
}

void ImageAsset::FreeGPUResources()
{
    
}
