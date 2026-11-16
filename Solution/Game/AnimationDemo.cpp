#include "AnimationDemo.h"
#include <cmath>
#include <iostream>
#include <string>
#include "imgui.h"
#include "Readback.h"
#include "Renderer.h"
#include "Skinning.h"
#include "Animation/AnimationSystem.h"
#include "Animation/AnimatorComponent.h"
#include "Scene/SceneNode.h"
#include "UI/DebugOverlay.h"

using namespace DirectX;

static constexpr float FadeSeconds = 0.5f;
static constexpr float SecondsBetweenFades = 4.0f;
static constexpr uint32_t CheckedVertexCount = 256;

struct DemoCharacter
{
    AnimatorComponent* Animator = nullptr;
    float SecondsToNextFade = 0.0f;
};

static std::vector<DemoCharacter> Characters;
static std::vector<Clip*> CodeClips;
static uint32_t CrowdCount = 0;
static uint32_t FadesStarted = 0;
static uint32_t FramesFading = 0;
static bool IsTestRunning = false;
static uint32_t BindPoseJoints = 0;
static std::string BindPoseFailure;
static DebugOverlay::PanelID AnimationPanel = 0;

// The skinning check: the first vertices of one skinned mesh of the character, skinned on the CPU
// with the palette of the frame that was read back and compared with the GPU's result.
static MeshComponent* CheckedComponent = nullptr;
static uint32_t CheckedSubmesh = 0;
static std::vector<RHIStructures::SkinnedVertex> CheckedSource;
static uint32_t SkinningFramesMatched = 0;
static float LargestSkinningError = 0.0f;
static std::string SkinningFailure;

static double GpuMilliseconds(const std::string& passName)
{
    for (const GpuPassTime& time : Renderer::GetFrameStats().GpuPasses)
        if (time.Name == passName)
            return time.Milliseconds;
    return 0.0;
}

static void DrawAnimationPanel()
{
    const AnimationStats& stats = AnimationSystem::GetStats();
    ImGui::Text("Characters %u, joints %u", stats.Characters, stats.Joints);
    ImGui::Text("Skinned vertices %u", Skinning::GetSkinnedVertexCount());
    ImGui::Text("Animation CPU %.3f ms, skinning GPU %.3f ms", stats.CpuMilliseconds, GpuMilliseconds("Skinning"));
}

static void FindSkinnedSubmesh(SceneNode* node)
{
    MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType));
    if (component && !CheckedComponent)
    {
        MeshAsset* asset = component->GetMeshAsset();
        for (uint32_t i = 0; i < asset->GetMeshCount() && !CheckedComponent; i++)
            if (asset->GetMesh(i).IsSkinned())
            {
                CheckedComponent = component;
                CheckedSubmesh = i;
                const Mesh& mesh = asset->GetMesh(i);
                const RHIStructures::SkinnedVertex* vertices = static_cast<const RHIStructures::SkinnedVertex*>(mesh.GetCachedVertexData());
                CheckedSource.assign(vertices, vertices + (std::min)(mesh.GetVertexCount(), CheckedVertexCount));
            }
    }
    for (SceneNode* child : node->GetChildren())
        FindSkinnedSubmesh(child);
}

// Mirrors Shaders/cs_skinning.hlsl.
static void SkinOnCpu(const RHIStructures::SkinnedVertex& source, const std::vector<XMFLOAT4X4>& palette, float out[14])
{
    const float weights[4] = { source.BoneWeights.x, source.BoneWeights.y, source.BoneWeights.z, source.BoneWeights.w };
    const uint32_t joints[4] = { source.BoneIndices.x, source.BoneIndices.y, source.BoneIndices.z, source.BoneIndices.w };
    XMMATRIX skin = XMMatrixIdentity();
    if (weights[0] + weights[1] + weights[2] + weights[3] > 0.0f)
    {
        skin = XMMATRIX(XMVectorZero(), XMVectorZero(), XMVectorZero(), XMVectorZero());
        for (int i = 0; i < 4; i++)
            if (weights[i] > 0.0f)
                skin += XMLoadFloat4x4(&palette[joints[i]]) * weights[i];
    }

    XMFLOAT3 position, normal, tangent, bitangent;
    XMStoreFloat3(&position, XMVector3Transform(XMLoadFloat3(&source.Position), skin));
    XMStoreFloat3(&normal, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&source.Normal), skin)));
    XMStoreFloat3(&tangent, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&source.Tangent), skin)));
    XMStoreFloat3(&bitangent, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&source.Bitangent), skin)));
    const float values[14] = { position.x, position.y, position.z, normal.x, normal.y, normal.z, tangent.x, tangent.y, tangent.z,
                               bitangent.x, bitangent.y, bitangent.z, source.TexCoord.x, source.TexCoord.y };
    std::copy(values, values + 14, out);
}

// Also checks that every vertex lies inside the model-space bounds used for culling that frame.
static void CompareSkinnedVertices(const ReadbackResult& result, const std::vector<XMFLOAT4X4>& palette, const XMFLOAT4& bounds)
{
    const float* gpu = reinterpret_cast<const float*>(result.Data);
    for (size_t v = 0; v < CheckedSource.size(); v++)
    {
        float expected[14];
        SkinOnCpu(CheckedSource[v], palette, expected);
        for (int k = 0; k < 14; k++)
        {
            float error = std::abs(gpu[v * 14 + k] - expected[k]);
            LargestSkinningError = (std::max)(LargestSkinningError, error);
            if (error > 1e-3f * (1.0f + std::abs(expected[k])) && SkinningFailure.empty())
                SkinningFailure = "vertex " + std::to_string(v) + " word " + std::to_string(k) + " is " + std::to_string(gpu[v * 14 + k]) +
                                  ", expected " + std::to_string(expected[k]) + " (frame " + std::to_string(result.RequestedFrame) + ")";
        }
    }
    for (size_t v = 0; v < CheckedSource.size() && SkinningFailure.empty(); v++)
    {
        float distance = XMVectorGetX(XMVector3Length(XMVectorSet(gpu[v * 14], gpu[v * 14 + 1], gpu[v * 14 + 2], 0.0f) - XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&bounds))));
        if (distance > bounds.w * 1.0001f + 1e-5f)
            SkinningFailure = "vertex " + std::to_string(v) + " is outside the culling bounds by " + std::to_string(distance - bounds.w);
    }
    SkinningFramesMatched++;
}

// Checks that the bind pose gives the identity palette for every joint.
static void CheckBindPose(const AnimatorComponent* animator)
{
    if (!animator || !animator->GetSkeleton())
    {
        BindPoseFailure = "the character has no skeleton";
        return;
    }

    const Skeleton& skeleton = *animator->GetSkeleton();
    Pose pose;
    pose.SetToBind(skeleton);
    std::vector<XMFLOAT4X4> palette;
    Pose::BuildPalette(skeleton, pose, palette);
    for (uint32_t j = 0; j < skeleton.GetJointCount(); j++)
        for (int row = 0; row < 4; row++)
            for (int column = 0; column < 4; column++)
            {
                float expected = row == column ? 1.0f : 0.0f;
                if (std::abs(palette[j].m[row][column] - expected) > 1e-4f && BindPoseFailure.empty())
                    BindPoseFailure = "joint " + skeleton.GetJoint(j).Name + " entry " + std::to_string(row) + "," + std::to_string(column) +
                                      " is " + std::to_string(palette[j].m[row][column]);
            }
    BindPoseJoints = skeleton.GetJointCount();
}

static XMFLOAT4 RotateFromBind(const JointTransform& bind, FXMVECTOR axis, float angle)
{
    XMFLOAT4 rotation;
    XMStoreFloat4(&rotation, XMQuaternionMultiply(XMQuaternionRotationAxis(axis, angle), XMLoadFloat4(&bind.Rotation)));
    return rotation;
}

struct JointSwing
{
    const char* Joint;
    XMFLOAT3 Axis;          // In the joint's own space
    float Amplitude;        // Radians
};

// A looping clip that swings each named joint about an axis by Amplitude * sin, so it is at the bind
// pose at time 0. Rotations only, so it survives changes to the character's proportions. Null when
// the skeleton has none of the joints.
static Clip* BuildSwingClip(const Skeleton& skeleton, const std::string& name, float seconds, std::initializer_list<JointSwing> swings)
{
    constexpr uint32_t KeyCount = 31;
    std::vector<JointTrack> tracks(skeleton.GetJointCount());
    bool hasJoint = false;
    for (const JointSwing& swing : swings)
    {
        int32_t joint = skeleton.FindJoint(swing.Joint);
        if (joint < 0)
            continue;
        hasJoint = true;
        for (uint32_t k = 0; k < KeyCount; k++)
        {
            float time = seconds * k / (KeyCount - 1);
            float angle = swing.Amplitude * std::sin(XM_2PI * time / seconds);
            tracks[joint].Rotations.push_back({ time, RotateFromBind(skeleton.GetJoint(joint).BindLocal, XMLoadFloat3(&swing.Axis), angle) });
        }
    }
    return hasJoint ? new Clip(name, seconds, std::move(tracks)) : nullptr;
}

// Clips for a Genesis 9 skeleton, found by joint name.
static bool AddNamedJointClips(AnimatorComponent* animator)
{
    const Skeleton& skeleton = *animator->GetSkeleton();
    Clip* wave = BuildSwingClip(skeleton, "Wave", 2.0f, {
        { "r_upperarm", { 0, 0, 1 }, 0.6f }, { "r_forearm", { 1, 0, 0 }, 0.5f }, { "neck1", { 0, 1, 0 }, 0.15f } });
    Clip* sway = BuildSwingClip(skeleton, "Sway", 3.0f, {
        { "spine2", { 0, 0, 1 }, 0.12f }, { "neck1", { 0, 0, 1 }, -0.12f }, { "l_upperarm", { 0, 0, 1 }, 0.25f },
        { "r_upperarm", { 0, 0, 1 }, -0.25f } });
    if (!wave || !sway)
    {
        delete wave;
        delete sway;
        return false;
    }
    CodeClips.push_back(wave);
    CodeClips.push_back(sway);
    animator->AddState({ "Wave", wave });
    animator->AddState({ "Sway", sway });
    return true;
}

// Two looping clips, both at the bind pose at time 0. A Genesis 9 skeleton gets "Wave" and "Sway";
// any other gets clips on its last three joints: "Dance" moves the first of them round a circle and
// bends the other two about their Z axes, "Sway" bends the first about its X axis.
static void AddCodeClips(AnimatorComponent* animator)
{
    if (AddNamedJointClips(animator))
        return;

    const Skeleton& skeleton = *animator->GetSkeleton();
    uint32_t count = skeleton.GetJointCount();
    if (count < 3)
        return;

    uint32_t first = count - 3;
    const JointTransform& firstBind = skeleton.GetJoint(first).BindLocal;
    float boneLength = XMVectorGetX(XMVector3Length(XMLoadFloat3(&skeleton.GetJoint(first + 1).BindLocal.Translation)));
    float radius = boneLength > 0.0f ? boneLength * 0.25f : 0.1f;

    constexpr uint32_t KeyCount = 31;
    constexpr float DanceSeconds = 3.0f;
    std::vector<JointTrack> dance(count);
    for (uint32_t k = 0; k < KeyCount; k++)
    {
        float time = DanceSeconds * k / (KeyCount - 1);
        float phase = XM_2PI * time / DanceSeconds;
        XMFLOAT3 translation = { firstBind.Translation.x + radius * (std::cos(phase) - 1.0f), firstBind.Translation.y,
                                 firstBind.Translation.z + radius * std::sin(phase) };
        dance[first].Translations.push_back({ time, translation });
        for (uint32_t j = first + 1; j < count; j++)
            dance[j].Rotations.push_back({ time, RotateFromBind(skeleton.GetJoint(j).BindLocal, XMVectorSet(0, 0, 1, 0), XM_PI / 8 * std::sin(phase)) });
    }

    constexpr float SwaySeconds = 2.0f;
    std::vector<JointTrack> sway(count);
    for (uint32_t k = 0; k < KeyCount; k++)
    {
        float time = SwaySeconds * k / (KeyCount - 1);
        float phase = XM_2PI * time / SwaySeconds;
        sway[first].Rotations.push_back({ time, RotateFromBind(firstBind, XMVectorSet(1, 0, 0, 0), XM_PI / 10 * std::sin(phase)) });
    }

    CodeClips.push_back(new Clip("Dance", DanceSeconds, std::move(dance)));
    CodeClips.push_back(new Clip("Sway", SwaySeconds, std::move(sway)));
    animator->AddState({ "Dance", CodeClips[0] });
    animator->AddState({ "Sway", CodeClips[1] });
}

static SceneNode* CopyHierarchy(SceneNode* source, SceneNode* parent, const XMMATRIX& local, const std::vector<MaterialHandle>& materials)
{
    SceneNode* copy = new SceneNode(source->GetAssetName(), local, parent);
    if (MeshComponent* mesh = static_cast<MeshComponent*>(source->GetComponent(MeshComponentType)))
    {
        MeshComponent* copyMesh = static_cast<MeshComponent*>(copy->AddComponent(MeshComponentType));
        copyMesh->SetMeshAsset(mesh->GetMeshAsset());
        copyMesh->AssignMaterials(materials);
    }
    for (SceneNode* child : source->GetChildren())
        CopyHierarchy(child, copy, child->GetLocalMatrix(), materials);
    return copy;
}

// Copies of the character on a square grid behind it, each starting its clip at a different time.
static uint32_t CountSkinnedVertices(SceneNode* node)
{
    uint32_t count = 0;
    if (MeshComponent* component = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType)))
        for (uint32_t i = 0; i < component->GetMeshAsset()->GetMeshCount(); i++)
            if (component->GetMeshAsset()->GetMesh(i).IsSkinned())
                count += component->GetMeshAsset()->GetMesh(i).GetVertexCount();
    for (SceneNode* child : node->GetChildren())
        count += CountSkinnedVertices(child);
    return count;
}

static void BuildCrowd(SceneNode* character, AnimatorComponent* animator, const std::vector<MaterialHandle>& materials, uint32_t count)
{
    XMMATRIX world = character->GetWorldMatrix();
    float scale = XMVectorGetX(XMVector3Length(world.r[0]));
    float spacing = (std::max)(animator->GetModelBounds().w * scale * 2.5f, 0.5f);
    uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    float half = (side - 1) * spacing * 0.5f;

    for (uint32_t i = 0; i < count; i++)
    {
        XMMATRIX offset = XMMatrixTranslation((i % side) * spacing - half, 0.0f, (i / side + 1) * spacing);
        SceneNode* copy = CopyHierarchy(character, nullptr, character->GetLocalMatrix() * offset, materials);
        AnimatorComponent* copyAnimator = static_cast<AnimatorComponent*>(copy->AddComponent(AnimatorComponentType));
        copyAnimator->SetSkeleton(animator->GetSkeleton());
        for (const AnimationState& state : animator->GetStates())
            copyAnimator->AddState(state);

        const std::vector<AnimationState>& states = copyAnimator->GetStates();
        copyAnimator->Play(states[i % states.size()].Name, 0.0f);
        copyAnimator->SetCurrentTime(0.37f * i);
        Characters.push_back({ copyAnimator, SecondsBetweenFades * (0.25f + 0.75f * ((i * 7) % 11) / 11.0f) });
    }
}

// A character with fewer than two clips in its file gets clips built in code, so fades can be seen.
static AnimatorComponent* StartCharacter(SceneNode* character)
{
    AnimatorComponent* animator = static_cast<AnimatorComponent*>(character->GetComponent(AnimatorComponentType));
    if (!animator || !animator->GetSkeleton())
        return nullptr;
    if (animator->GetStates().size() < 2)
        AddCodeClips(animator);
    if (animator->GetStates().empty())
        return nullptr;

    animator->Play(animator->GetStates()[0].Name, 0.0f);
    Characters.push_back({ animator, SecondsBetweenFades });
    return animator;
}

void AnimationDemo::Start(const std::vector<SceneNode*>& characters, const std::vector<MaterialHandle>& materials, uint32_t crowdCount, bool isAnimationTest)
{
    IsTestRunning = isAnimationTest;
    AnimationPanel = DebugOverlay::EnlistPanel("Animation", DrawAnimationPanel);
    SceneNode* character = characters.back();
    if (isAnimationTest)
    {
        CheckBindPose(static_cast<AnimatorComponent*>(character->GetComponent(AnimatorComponentType)));
        FindSkinnedSubmesh(character);
        Readback::WatchBuffer(Skinning::GetPool());
    }

    AnimatorComponent* animator = nullptr;
    for (SceneNode* each : characters)
        animator = StartCharacter(each);
    if (!animator)
        return;

    // Every copy needs its own range of the skinning pool, which is capped; the characters already
    // started take their share first.
    uint32_t characterVertices = (std::max)(CountSkinnedVertices(character), 1u);
    uint32_t usedVertices = 0;
    for (SceneNode* each : characters)
        usedVertices += CountSkinnedVertices(each);
    uint32_t fittingCopies = (Skinning::MaxPoolVertices - (std::min)(usedVertices, Skinning::MaxPoolVertices)) / characterVertices;
    if (crowdCount > fittingCopies)
    {
        std::cout << "Crowd: " << crowdCount << " copies of " << characterVertices << " vertices do not fit the skinning pool; using "
                  << fittingCopies << std::endl;
        crowdCount = fittingCopies;
    }
    CrowdCount = crowdCount;
    if (crowdCount > 0)
        BuildCrowd(character, animator, materials, crowdCount);
}

void AnimationDemo::Update(float dt)
{
    bool isAnyFading = false;
    for (DemoCharacter& character : Characters)
    {
        character.SecondsToNextFade -= dt;
        if (character.SecondsToNextFade <= 0.0f)
        {
            const std::vector<AnimationState>& states = character.Animator->GetStates();
            const AnimationState* current = character.Animator->GetCurrentState();
            size_t next = 0;
            for (size_t i = 0; i < states.size(); i++)
                if (&states[i] == current)
                    next = (i + 1) % states.size();
            if (character.Animator->Play(states[next].Name, FadeSeconds))
                FadesStarted++;
            character.SecondsToNextFade = SecondsBetweenFades;
        }
        isAnyFading |= character.Animator->IsFading();
    }
    FramesFading += isAnyFading ? 1 : 0;
}

void AnimationDemo::RequestChecks()
{
    if (!IsTestRunning || !CheckedComponent || CheckedSource.empty() || !SkinningFailure.empty())
        return;
    AnimatorComponent* animator = CheckedComponent->GetAnimator();
    SkinnedMeshHandle mesh = CheckedComponent->GetSkinnedMesh(CheckedSubmesh);
    if (!animator || !Skinning::IsLive(mesh))
        return;

    uint64_t stride = sizeof(RHIStructures::Vertex);
    Readback::RequestBuffer(Skinning::GetPool(), Skinning::GetBaseVertex(mesh) * stride, CheckedSource.size() * stride,
        [palette = animator->GetPalette(), bounds = animator->GetModelBounds()](const ReadbackResult& result) { CompareSkinnedVertices(result, palette, bounds); });
}

void AnimationDemo::PrintResults()
{
    const AnimationStats& stats = AnimationSystem::GetStats();
    std::cout << "Animation: " << stats.Characters << " characters, " << stats.Joints << " joints, " << Skinning::GetSkinnedVertexCount()
              << " skinned vertices, CPU " << stats.CpuMilliseconds << " ms, skinning GPU " << GpuMilliseconds("Skinning") << " ms" << std::endl;
    if (CrowdCount > 0)
        std::cout << "Crowd: " << CrowdCount << " copies, " << FadesStarted << " cross-fades started, " << FramesFading << " frames fading" << std::endl;
    if (!IsTestRunning)
        return;
    if (BindPoseFailure.empty())
        std::cout << "Animation test: bind pose identity for " << BindPoseJoints << " joints" << std::endl;
    else
        std::cout << "Animation test: bind pose failed, " << BindPoseFailure << std::endl;
    if (!SkinningFailure.empty())
        std::cout << "Skinning test: failed, " << SkinningFailure << std::endl;
    else if (SkinningFramesMatched == 0)
        std::cout << "Skinning test: failed, no readback arrived" << std::endl;
    else
        std::cout << "Skinning test: " << CheckedSource.size() << " vertices matched on " << SkinningFramesMatched
                  << " frames, largest error " << LargestSkinningError << std::endl;
}

void AnimationDemo::Shutdown()
{
    DebugOverlay::RetirePanel(AnimationPanel);
    CheckedComponent = nullptr;
    Characters.clear();
    for (Clip* clip : CodeClips)
        delete clip;
    CodeClips.clear();
}
