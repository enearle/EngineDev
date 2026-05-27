#include "Field.h"
#include "ResourceManager.h"

void Field::SetID(const AssetID& id)
{
    // Shoul do this in imgui section of code maybe FieldUI
    if (ResourceManager::ValidateAssetType(id, Type)) ID = id;
}
