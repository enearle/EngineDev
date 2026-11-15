#include "Skeleton.h"
#include "../Resources/AssetSerializer.h"

Skeleton::Skeleton(const std::string& name, std::vector<Joint> joints) : AssetBase(name, AssetID::Generate()), Joints(std::move(joints))
{
}

int32_t Skeleton::FindJoint(const std::string& name) const
{
    for (size_t i = 0; i < Joints.size(); i++)
        if (Joints[i].Name == name)
            return static_cast<int32_t>(i);
    return -1;
}

void Skeleton::Serialize(std::string& data)
{
    AssetBase::Serialize(data);
    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(Joints.size()));
    for (const Joint& joint : Joints)
    {
        AssetSerializer::WriteString(data, joint.Name);
        AssetSerializer::Write(data, joint.Parent);
        AssetSerializer::Write(data, joint.BindLocal);
        AssetSerializer::Write(data, joint.InverseBind);
        AssetSerializer::Write(data, joint.InfluenceRadius);
    }
}

void Skeleton::Deserialize(std::string& data, long& offset)
{
    AssetBase::Deserialize(data, offset);
    Joints.resize(AssetSerializer::Read<uint32_t>(data, offset));
    for (Joint& joint : Joints)
    {
        joint.Name = AssetSerializer::ReadString(data, offset);
        joint.Parent = AssetSerializer::Read<int32_t>(data, offset);
        joint.BindLocal = AssetSerializer::Read<JointTransform>(data, offset);
        joint.InverseBind = AssetSerializer::Read<DirectX::XMFLOAT4X4>(data, offset);
        joint.InfluenceRadius = AssetSerializer::Read<float>(data, offset);
    }
}
