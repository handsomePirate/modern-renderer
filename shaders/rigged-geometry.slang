struct VertexData
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    uint4 boneIndexes;
    float4 weights;
};

struct FragmentData
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float weight;
};

struct PixelData
{
    float4 color : SV_Target0;
};

struct CameraData
{
    float4x4 vp;
};

struct BoneData
{
    float4x4 transforms[256];
};

struct PushConstants
{
    uint selectedBone;
};

[[vk::push_constant]]
PushConstants pc;

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[[vk::binding(0, 1)]]
ConstantBuffer<BoneData> rig;

[shader("vertex")]
FragmentData vertexMain(VertexData vertex)
{
    float4x4 blendedTransform = float4x4(0);
    for (uint i = 0; i < 4; ++i)
    {
        blendedTransform += rig.transforms[vertex.boneIndexes[i]] * vertex.weights[i];
    }
    float4 skinnedPosition = mul(blendedTransform, float4(vertex.position, 1));
    float3 skinnedNormal = mul((float3x3)blendedTransform, vertex.normal);
    // float4 skinnedPosition = float4(vertex.position, 1);
    // float3 skinnedNormal = vertex.normal;

    FragmentData fragment;
    fragment.position = mul(camera.vp, skinnedPosition);
    fragment.normal = skinnedNormal;
    fragment.weight = 0;
    for (uint i = 0; i < 4; ++i)
    {
        fragment.weight += vertex.boneIndexes[i] == pc.selectedBone ? vertex.weights[i] : 0;
    }
    return fragment;
}

[shader("pixel")]
PixelData fragmentMain(FragmentData fragment)
{
    float3 lightDir = normalize(float3(-0.2, 0.1, 1));
    PixelData pixel;
    pixel.color = max(dot(fragment.normal, lightDir), 0.1);
    pixel.color = lerp(pixel.color, float4(1, 0, 0, 1), fragment.weight);
    return pixel;
}
