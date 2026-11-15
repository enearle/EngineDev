#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../ENGINE_API_Macro.h"
#include "../Resources/AssetBase.h"

// A joint's transform relative to its parent: scale, then rotation, then translation.
struct JointTransform
{
    DirectX::XMFLOAT3 Translation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 Rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
};

struct Joint
{
    std::string Name;
    int32_t Parent = -1;                    // Earlier in the joint list; -1 for a joint directly under the model root
    JointTransform BindLocal;
    DirectX::XMFLOAT4X4 InverseBind = {};   // Model space to joint space in the bind pose
    float InfluenceRadius = 0.0f;           // Largest distance from the joint to a vertex it moves, in model space
};

// The joints of one model in parent-first order. Matrices are row-vector (DirectXMath) form:
// Model[j] = Local[j] * Model[parent] and Skin[j] = InverseBind[j] * Model[j].
class ENGINE_API Skeleton : public AssetBase
{
    std::vector<Joint> Joints;

public:
    Skeleton() = default;
    Skeleton(const std::string& name, std::vector<Joint> joints);

    void Serialize(std::string& data) override;
    void Deserialize(std::string& data, long& offset) override;

    uint32_t GetJointCount() const { return static_cast<uint32_t>(Joints.size()); }
    const Joint& GetJoint(uint32_t index) const { return Joints[index]; }
    const std::vector<Joint>& GetJoints() const { return Joints; }
    int32_t FindJoint(const std::string& name) const;
};
