struct VertexData
{
    float3 position : POSITION;
};

struct FragmentData
{
    float4 position : SV_Position;
};

struct PushConstants
{
    float4x4 orto;
};

[[vk::push_constant]]
PushConstants pc;

[shader("vertex")]
FragmentData vertexMain(VertexData in: VertexIn)
{
    FragmentData fragment;
    fragment.position = mul(pc.orto, float4(in.position, 1));
    return fragment;
}

[shader("pixel")]
void fragmentMain()
{
}
