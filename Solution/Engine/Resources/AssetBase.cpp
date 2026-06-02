#include "AssetBase.h"
#include <stdexcept>
#include "AssetSerializer.h"
#include "ResourceManager.h"
#include "UUID.h"

void AssetBase::Serialize(std::string& data)
{
    AssetSerializer::WriteString(data, Name);
    AssetSerializer::WriteBytes(data, &ID, sizeof(AssetID));
}

void AssetBase::Deserialize(std::string& data, long& offset)
{
    Name = AssetSerializer::ReadString(data, offset);
    AssetSerializer::ReadBytes(data, offset, &ID, sizeof(AssetID));
}

void DependentAssetBase::Serialize(std::string& data)
{
    AssetBase::Serialize(data);

    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(Fields.size()));
    for (const Field& field : Fields)
    {
        AssetID id = field.GetID();
        AssetSerializer::WriteBytes(data, &id, sizeof(AssetID));
    }
}

void DependentAssetBase::Deserialize(std::string& data, long& offset)
{
    AssetBase::Deserialize(data, offset);

    uint32_t count = AssetSerializer::Read<uint32_t>(data, offset);
    if (Fields.size() < count) Fields.resize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        AssetID id;
        AssetSerializer::ReadBytes(data, offset, &id, sizeof(AssetID));
        // Write directly to bypass type-validation in SetID; field metadata may
        // not be reconstructed yet at this point of deserialization.
        Fields[i].ID = id;
    }
}

