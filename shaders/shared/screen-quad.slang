struct VertexData
{
    uint vertexId : SV_VertexID;
};

struct FragmentData
{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
FragmentData vertexMain(VertexData in)
{
    FragmentData fragment;
    fragment.texCoord = float2((in.vertexId & 1) ^ ((in.vertexId >> 1) & 1),
                               (in.vertexId >> 1) & 1);
    fragment.position = float4(fragment.texCoord * 2.0 - 1.0, 0.0, 1.0);
    return fragment;
}
