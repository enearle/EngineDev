#include "Clip.h"
#include <algorithm>
#include "../Resources/AssetSerializer.h"

using namespace DirectX;

Clip::Clip(const std::string& name, float durationSeconds, std::vector<JointTrack> tracks)
    : AssetBase(name, AssetID::Generate()), Duration(durationSeconds), Tracks(std::move(tracks))
{
}

// The index of the last key at or before time, and the weight of the key after it.
template <typename Key>
static size_t FindKey(const std::vector<Key>& keys, float time, float& weight)
{
    weight = 0.0f;
    if (time <= keys.front().Time)
        return 0;
    if (time >= keys.back().Time)
        return keys.size() - 1;

    size_t next = std::upper_bound(keys.begin(), keys.end(), time, [](float t, const Key& key) { return t < key.Time; }) - keys.begin();
    float span = keys[next].Time - keys[next - 1].Time;
    weight = span > 0.0f ? (time - keys[next - 1].Time) / span : 0.0f;
    return next - 1;
}

static XMFLOAT3 SampleVector(const std::vector<VectorKey>& keys, float time)
{
    float weight;
    size_t index = FindKey(keys, time, weight);
    if (weight == 0.0f)
        return keys[index].Value;
    XMFLOAT3 result;
    XMStoreFloat3(&result, XMVectorLerp(XMLoadFloat3(&keys[index].Value), XMLoadFloat3(&keys[index + 1].Value), weight));
    return result;
}

static XMFLOAT4 SampleRotation(const std::vector<RotationKey>& keys, float time)
{
    float weight;
    size_t index = FindKey(keys, time, weight);
    if (weight == 0.0f)
        return keys[index].Value;
    XMFLOAT4 result;
    XMStoreFloat4(&result, NlerpShortest(XMLoadFloat4(&keys[index].Value), XMLoadFloat4(&keys[index + 1].Value), weight));
    return result;
}

void Clip::Sample(const Skeleton& skeleton, float time, Pose& out) const
{
    out.Joints.resize(skeleton.GetJointCount());
    for (uint32_t i = 0; i < skeleton.GetJointCount(); i++)
    {
        JointTransform& joint = out.Joints[i];
        joint = skeleton.GetJoint(i).BindLocal;
        if (i >= Tracks.size())
            continue;

        const JointTrack& track = Tracks[i];
        if (!track.Translations.empty())
            joint.Translation = SampleVector(track.Translations, time);
        if (!track.Rotations.empty())
            joint.Rotation = SampleRotation(track.Rotations, time);
        if (!track.Scales.empty())
            joint.Scale = SampleVector(track.Scales, time);
    }
}

template <typename Key>
static void WriteKeys(std::string& data, const std::vector<Key>& keys)
{
    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(keys.size()));
    AssetSerializer::WriteBytes(data, keys.data(), keys.size() * sizeof(Key));
}

template <typename Key>
static void ReadKeys(std::string& data, long& offset, std::vector<Key>& keys)
{
    keys.resize(AssetSerializer::Read<uint32_t>(data, offset));
    AssetSerializer::ReadBytes(data, offset, keys.data(), keys.size() * sizeof(Key));
}

void Clip::Serialize(std::string& data)
{
    AssetBase::Serialize(data);
    AssetSerializer::Write(data, Duration);
    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(Tracks.size()));
    for (const JointTrack& track : Tracks)
    {
        WriteKeys(data, track.Translations);
        WriteKeys(data, track.Rotations);
        WriteKeys(data, track.Scales);
    }
}

void Clip::Deserialize(std::string& data, long& offset)
{
    AssetBase::Deserialize(data, offset);
    Duration = AssetSerializer::Read<float>(data, offset);
    Tracks.resize(AssetSerializer::Read<uint32_t>(data, offset));
    for (JointTrack& track : Tracks)
    {
        ReadKeys(data, offset, track.Translations);
        ReadKeys(data, offset, track.Rotations);
        ReadKeys(data, offset, track.Scales);
    }
}
