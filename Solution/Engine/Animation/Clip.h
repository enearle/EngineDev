#pragma once
#include <string>
#include <vector>
#include "Pose.h"

struct VectorKey
{
    float Time = 0.0f;
    DirectX::XMFLOAT3 Value = {};
};

struct RotationKey
{
    float Time = 0.0f;
    DirectX::XMFLOAT4 Value = { 0.0f, 0.0f, 0.0f, 1.0f };
};

// Keys in increasing time. An empty track leaves that part of the joint at its bind pose.
struct JointTrack
{
    std::vector<VectorKey> Translations;
    std::vector<RotationKey> Rotations;
    std::vector<VectorKey> Scales;
};

// Timed keys for the joints of one skeleton, one track per joint.
class ENGINE_API Clip : public AssetBase
{
    float Duration = 0.0f;
    std::vector<JointTrack> Tracks;

public:
    Clip() = default;
    Clip(const std::string& name, float durationSeconds, std::vector<JointTrack> tracks);

    void Serialize(std::string& data) override;
    void Deserialize(std::string& data, long& offset) override;

    float GetDuration() const { return Duration; }
    const std::vector<JointTrack>& GetTracks() const { return Tracks; }

    // Linear interpolation between the keys around time, clamped to the first and last key.
    void Sample(const Skeleton& skeleton, float time, Pose& out) const;
};
