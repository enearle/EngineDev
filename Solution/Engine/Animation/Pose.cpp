#include "Pose.h"

using namespace DirectX;

XMVECTOR NlerpShortest(FXMVECTOR a, FXMVECTOR b, float weight)
{
    XMVECTOR target = XMVectorGetX(XMVector4Dot(a, b)) < 0.0f ? XMVectorNegate(b) : b;
    return XMQuaternionNormalize(XMVectorLerp(a, target, weight));
}

void Pose::SetToBind(const Skeleton& skeleton)
{
    Joints.resize(skeleton.GetJointCount());
    for (uint32_t i = 0; i < skeleton.GetJointCount(); i++)
        Joints[i] = skeleton.GetJoint(i).BindLocal;
}

void Pose::Blend(const Pose& a, const Pose& b, float weight, Pose& out)
{
    out.Joints.resize(a.Joints.size());
    for (size_t i = 0; i < a.Joints.size(); i++)
    {
        const JointTransform& from = a.Joints[i];
        const JointTransform& to = b.Joints[i];
        XMStoreFloat3(&out.Joints[i].Translation, XMVectorLerp(XMLoadFloat3(&from.Translation), XMLoadFloat3(&to.Translation), weight));
        XMStoreFloat4(&out.Joints[i].Rotation, NlerpShortest(XMLoadFloat4(&from.Rotation), XMLoadFloat4(&to.Rotation), weight));
        XMStoreFloat3(&out.Joints[i].Scale, XMVectorLerp(XMLoadFloat3(&from.Scale), XMLoadFloat3(&to.Scale), weight));
    }
}

void Pose::BuildPalette(const Skeleton& skeleton, const Pose& pose, std::vector<XMFLOAT4X4>& out, std::vector<XMFLOAT3>* jointPositions)
{
    uint32_t count = skeleton.GetJointCount();
    out.resize(count);
    if (jointPositions)
        jointPositions->resize(count);

    static thread_local std::vector<XMMATRIX> model;
    model.resize(count);
    for (uint32_t i = 0; i < count; i++)
    {
        const JointTransform& local = pose.Joints[i];
        XMMATRIX matrix = XMMatrixAffineTransformation(XMLoadFloat3(&local.Scale), XMVectorZero(), XMLoadFloat4(&local.Rotation),
                                                       XMLoadFloat3(&local.Translation));
        int32_t parent = skeleton.GetJoint(i).Parent;
        model[i] = parent >= 0 ? matrix * model[parent] : matrix;
        XMStoreFloat4x4(&out[i], XMLoadFloat4x4(&skeleton.GetJoint(i).InverseBind) * model[i]);
        if (jointPositions)
            XMStoreFloat3(&(*jointPositions)[i], model[i].r[3]);
    }
}
