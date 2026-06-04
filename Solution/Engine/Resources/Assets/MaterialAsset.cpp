#include "MaterialAsset.h"

#include <iostream>
#include <ostream>

std::map<AssetID, uint64_t> MaterialAsset::LoadedMaterials;

void MaterialAsset::UploadToGPU()
{
    if(LoadedMaterials.find(ID) != LoadedMaterials.end()) return;

    for (Field field : Fields)
        if (!field.GetID().IsValid())
        {
            std::cout << "Material field has no ID: " << field.GetName() << std::endl;
            return;
        }
    
    
}

void MaterialAsset::FreeGPUResources()
{
    
}
