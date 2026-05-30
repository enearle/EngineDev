#include "SceneComponent.h"

void SceneComponentBase::Serialize(std::string& data)
{
    data += static_cast<char>(Fields.size());
    for (const Field& field : Fields)
        data += field.GetID().to_string();
}

void SceneComponentBase::Deserialize(std::string& data, long& offset)
{
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
