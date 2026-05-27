#include "AssetBase.h"

#include <stdexcept>
#include "ResourceManager.h"
#include "UUID.h"

void AssetBase::Serialize(std::string& data)
{
    data = static_cast<char>(Name.size());
    data += Name;
    data += ID.to_string();
}

void AssetBase::Deserialize(std::string& data, long offset)
{
    if (data.empty()) throw std::invalid_argument("Data cannot be empty when deserializing.");
    int count = static_cast<uint8_t>(data[0]);
    Name = data.substr(1, count);
    ID = AssetID::from_string(data.substr(1 + count, 36));
    
    offset = 1 + count + 36;
}

void DependencyBase::Serialize(std::string& data)
{
    AssetBase::Serialize(data);
    
    data += static_cast<char>(Fields.size());
    for (const Field& field : Fields)
        data += field.GetID().to_string();
}

void DependencyBase::Deserialize(std::string& data, long offset)
{
    AssetBase::Deserialize(data, offset);
    
    if (data.empty()) return;
    int count = static_cast<uint8_t>(data[offset]);
    for (int i = 0; i < count && i < static_cast<int>(Fields.size()); ++i)
    {
        size_t current = offset + 1 + i * 36;
        if (current + 36 > data.size()) break;
        Fields[i].SetID(AssetID::from_string(data.substr(current, 36)));
    }
    
    offset += 1 + count * 36;
}
